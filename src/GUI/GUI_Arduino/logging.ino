/*
 * Fault Logger
 * ============
 * Maintains a rolling PSRAM buffer of sensor readings.
 * On fault, flushes to LittleFS so the log survives a full power cycle.
 * On boot, prints any saved fault log to Serial before normal operation.
 *
 * Storage
 * -------
 *   /fault_log.csv on LittleFS — overwritten each fault (keeps last event)
 *   Format: ms_ago, v12, a12, v5, a5, v33, a33, ac_w, dc_w, eff, temp_c, faults
 *
 * Tuning
 * ------
 *   LOG_DEPTH   — number of entries kept in the rolling buffer.
 *                 At 500 ms/sample, 240 entries = 2 minutes of history.
 *   Call fault_log_record() from your loop() sensor block.
 *   Call fault_log_save()   from enter_state(PSU_STATE_FAULT).
 */

#include <LittleFS.h>

// ── Config ────────────────────────────────────────────────────────────────────
#define LOG_DEPTH       240                 // Entries in rolling buffer (2 min @ 500 ms)
#define LOG_FILE        "/fault_log.csv"
#define LOG_PREV_FILE   "/fault_log_prev.csv"  // Previous fault kept for comparison

// ── One log entry ─────────────────────────────────────────────────────────────
struct LogEntry {
    unsigned long ts_ms;      // millis() at time of reading

    float v12, a12;
    float v5,  a5;
    float v33, a33;
    float ac_w, dc_w;
    float efficiency;
    float temp_c;

    // Fault flags snapshot — mirrors supervisor globals
    uint16_t faults;          // bitmask, see FAULT_* defines below
};

#define FAULT_12V_UV  (1 << 0)
#define FAULT_12V_OV  (1 << 1)
#define FAULT_12V_OC  (1 << 2)
#define FAULT_5V_UV   (1 << 3)
#define FAULT_5V_OV   (1 << 4)
#define FAULT_5V_OC   (1 << 5)
#define FAULT_3V3_UV  (1 << 6)
#define FAULT_3V3_OV  (1 << 7)
#define FAULT_3V3_OC  (1 << 8)

// ── Ring buffer (allocated in PSRAM) ─────────────────────────────────────────
static LogEntry *s_log    = nullptr;
static int       s_head   = 0;       // Next write position
static int       s_count  = 0;       // How many entries are valid
static bool      s_fs_ok  = false;

// ── Init — call from setup() before anything else ────────────────────────────
void fault_log_init() {
    // Allocate ring buffer in PSRAM
    s_log = (LogEntry *)heap_caps_malloc(
        LOG_DEPTH * sizeof(LogEntry), MALLOC_CAP_SPIRAM);

    if (!s_log) {
        Serial.println("[LOG] PSRAM alloc failed — logging disabled");
        return;
    }
    memset(s_log, 0, LOG_DEPTH * sizeof(LogEntry));

    // Mount LittleFS
    if (!LittleFS.begin(true)) {   // true = format if mount fails
        Serial.println("[LOG] LittleFS mount failed — flash logging disabled");
    } else {
        s_fs_ok = true;
        Serial.println("[LOG] LittleFS mounted OK");
    }
}

// ── Record one reading — call from your sensor block in loop() ───────────────
void fault_log_record(float v12, float a12,
                      float v5,  float a5,
                      float v33, float a33,
                      float ac_w, float dc_w,
                      float efficiency, float temp_c) {
    if (!s_log) return;

    LogEntry &e = s_log[s_head];

    e.ts_ms      = millis();
    e.v12        = v12;   e.a12 = a12;
    e.v5         = v5;    e.a5  = a5;
    e.v33        = v33;   e.a33 = a33;
    e.ac_w       = ac_w;  e.dc_w = dc_w;
    e.efficiency = efficiency;
    e.temp_c     = temp_c;

    // Snapshot fault flags from supervisor globals
    e.faults = 0;
    if (sup_fault_12v_uv) e.faults |= FAULT_12V_UV;
    if (sup_fault_12v_ov) e.faults |= FAULT_12V_OV;
    if (sup_fault_12v_oc) e.faults |= FAULT_12V_OC;
    if (sup_fault_5v_uv)  e.faults |= FAULT_5V_UV;
    if (sup_fault_5v_ov)  e.faults |= FAULT_5V_OV;
    if (sup_fault_5v_oc)  e.faults |= FAULT_5V_OC;
    if (sup_fault_3v3_uv) e.faults |= FAULT_3V3_UV;
    if (sup_fault_3v3_ov) e.faults |= FAULT_3V3_OV;
    if (sup_fault_3v3_oc) e.faults |= FAULT_3V3_OC;

    s_head = (s_head + 1) % LOG_DEPTH;
    if (s_count < LOG_DEPTH) s_count++;
}

// ── Internal: decode fault bitmask to short string ───────────────────────────
static void faults_to_str(uint16_t f, char *buf, size_t len) {
    buf[0] = '\0';
    if (!f) { strlcat(buf, "OK", len); return; }
    if (f & FAULT_12V_UV)  strlcat(buf, "12UV ",  len);
    if (f & FAULT_12V_OV)  strlcat(buf, "12OV ",  len);
    if (f & FAULT_12V_OC)  strlcat(buf, "12OC ",  len);
    if (f & FAULT_5V_UV)   strlcat(buf, "5UV ",   len);
    if (f & FAULT_5V_OV)   strlcat(buf, "5OV ",   len);
    if (f & FAULT_5V_OC)   strlcat(buf, "5OC ",   len);
    if (f & FAULT_3V3_UV)  strlcat(buf, "3UV ",   len);
    if (f & FAULT_3V3_OV)  strlcat(buf, "3OV ",   len);
    if (f & FAULT_3V3_OC)  strlcat(buf, "3OC ",   len);
}

// ── Flush ring buffer to LittleFS — call when entering PSU_STATE_FAULT ────────
//    Rotates the previous log to _prev so you can compare two consecutive faults.
void fault_log_save() {
    if (!s_log || !s_fs_ok || s_count == 0) return;

    // Rotate: current → prev
    if (LittleFS.exists(LOG_FILE)) {
        LittleFS.remove(LOG_PREV_FILE);
        LittleFS.rename(LOG_FILE, LOG_PREV_FILE);
    }

    File f = LittleFS.open(LOG_FILE, FILE_WRITE);
    if (!f) {
        Serial.println("[LOG] Failed to open fault log for writing");
        return;
    }

    // Header
    f.println("ms_ago,v12,a12,v5,a5,v33,a33,ac_w,dc_w,eff,temp_c,faults");

    unsigned long fault_ts = millis();
    char fault_str[48];

    // Walk buffer oldest → newest
    int start = (s_count < LOG_DEPTH) ? 0 : s_head;  // oldest entry index
    for (int i = 0; i < s_count; i++) {
        int idx = (start + i) % LOG_DEPTH;
        LogEntry &e = s_log[idx];

        faults_to_str(e.faults, fault_str, sizeof(fault_str));

        // ms_ago = how far before the fault this entry was recorded
        unsigned long ms_ago = fault_ts - e.ts_ms;

        f.printf("-%lu,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%.1f,%.1f,%.1f,%.1f,%s\n",
            ms_ago,
            e.v12, e.a12,
            e.v5,  e.a5,
            e.v33, e.a33,
            e.ac_w, e.dc_w,
            e.efficiency, e.temp_c,
            fault_str);
    }

    f.close();

    Serial.printf("[LOG] Fault log saved: %d entries, %.1f seconds of history\n",
                  s_count, (s_count * 0.5f));
}

// ── Print saved log to Serial — call at the top of setup() ───────────────────
void fault_log_print_saved() {
    if (!s_fs_ok) return;

    for (int pass = 0; pass < 2; pass++) {
        const char *path  = (pass == 0) ? LOG_FILE : LOG_PREV_FILE;
        const char *label = (pass == 0) ? "LAST FAULT" : "PREVIOUS FAULT";

        if (!LittleFS.exists(path)) continue;

        File f = LittleFS.open(path, FILE_READ);
        if (!f) continue;

        Serial.printf("\n====== %s LOG (%s) ======\n", label, path);
        while (f.available()) {
            Serial.write(f.read());
        }
        Serial.println("========================================\n");
        f.close();
    }
}

// ── Clear saved logs (e.g. after user acknowledges fault) ────────────────────
void fault_log_clear() {
    if (!s_fs_ok) return;
    LittleFS.remove(LOG_FILE);
    LittleFS.remove(LOG_PREV_FILE);
    Serial.println("[LOG] Fault logs cleared");
}
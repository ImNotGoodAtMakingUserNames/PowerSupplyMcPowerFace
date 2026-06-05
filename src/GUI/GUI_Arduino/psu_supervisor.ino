/*
 * PSU Supervisor + Power-On Sequencer
 * =====================================
 * Combines TPS3510DR-style rail supervision with ATX PS_ON sequencing.
 *
 * Pin assignments
 * ---------------
 *   GPIO 17  PS_ON_IN   INPUT   ATX PS_ON from motherboard (via voltage divider)
 *                               LOW  = motherboard requesting power on
 *                               HIGH = motherboard requesting standby / off
 *
 *   GPIO 18  PSU_EN_OUT OUTPUT  To inverter that controls PSU mains enable
 *                               HIGH = PSU running
 *                               LOW  = PSU shutdown (inverter kills mains)
 *
 * Voltage divider on PS_ON_IN
 * ----------------------------
 *   ATX PS_ON is a 5 V logic signal; the ESP32 is 3.3 V tolerant.
 *   A 10 kΩ / 6.8 kΩ divider gives 5 V → ~3.1 V, safe for GPIO input.
 *
 * Power-on sequence
 * -----------------
 *   1. STANDBY  — PSU_EN_OUT LOW (PSU off). Waiting for PS_ON_IN to go LOW.
 *   2. ENABLING — PSU_EN_OUT HIGH (PSU on). Wait RAIL_SETTLE_MS for rails to ramp.
 *   3. RUNNING  — All rails within limits. Supervisor monitors continuously.
 *   4. FAULT    — Rail fault or bad startup. PSU_EN_OUT driven LOW immediately.
 *                 Latched until motherboard releases PS_ON_IN.
 *
 * Rail thresholds — matched to TPS3510DR datasheet
 * -------------------------------------------------
 *   Rail    UV        OV
 *   12 V    10.56 V   13.20 V
 *    5 V     4.55 V    5.50 V
 *   3.3 V    2.93 V    3.63 V
 */

// ── Pins ─────────────────────────────────────────────────────────────────────
#define PS_ON_IN    17          // INPUT  — ATX PS_ON from motherboard (active LOW)
#define PSU_EN_OUT  18          // OUTPUT — inverter enable (HIGH = run, LOW = shutdown)

// ── Rail thresholds (volts) ───────────────────────────────────────────────────
#define UV_12V   11.40f
#define OV_12V   12.60f
#define UV_5V     4.75f
#define OV_5V     5.25f
#define UV_3V3    3.135f
#define OV_3V3    3.465f

// ── Overcurrent limits (amps) ─────────────────────────────────────────────────
#define OC_12V   8.0f
#define OC_5V    20.0f
#define OC_3V3   20.0f

// ── Timing ───────────────────────────────────────────────────────────────────
#define SUPERVISOR_INTERVAL_MS   10UL    // Rail check rate (100 Hz)
#define RESET_HOLD_MS           200UL    // Matches TPS3510DR t_d: clear fault only after
                                         // this many ms of clean readings
#define RAIL_SETTLE_MS          500UL    // ATX spec: up to 500 ms for rails to reach
                                         // regulation after PSU enable
#define REARM_LOCKOUT_MS       2000UL    // Min standby time after a fault before
                                         // re-enable is permitted (prevents rapid cycling)

// ── Public fault flags — readable from main sketch / GUI ─────────────────────
bool sup_fault_12v_uv, sup_fault_12v_ov, sup_fault_12v_oc;
bool sup_fault_5v_uv,  sup_fault_5v_ov,  sup_fault_5v_oc;
bool sup_fault_3v3_uv, sup_fault_3v3_ov, sup_fault_3v3_oc;

// ── Sequencer state machine ───────────────────────────────────────────────────
static psu_state_t   s_psu_state      = PSU_STATE_STANDBY;
static psu_state_t   s_prev_state     = PSU_STATE_STANDBY;
static unsigned long s_state_entry_ms = 0;

static char s_last_fault_str[32] = "None";

psu_state_t supervisor_get_state() { return s_psu_state; }

static void enter_state(psu_state_t next) {
    s_prev_state     = s_psu_state;
    s_psu_state      = next;
    s_state_entry_ms = millis();

    if (next == PSU_STATE_FAULT) {
        fault_log_save();

        // Build a compact fault string for the display
        char fault_str[32] = "";
        if (sup_fault_12v_uv) strlcat(fault_str, "12UV ",  sizeof(fault_str));
        if (sup_fault_12v_ov) strlcat(fault_str, "12OV ",  sizeof(fault_str));
        if (sup_fault_12v_oc) strlcat(fault_str, "12OC ",  sizeof(fault_str));
        if (sup_fault_5v_uv)  strlcat(fault_str, "5UV ",   sizeof(fault_str));
        if (sup_fault_5v_ov)  strlcat(fault_str, "5OV ",   sizeof(fault_str));
        if (sup_fault_5v_oc)  strlcat(fault_str, "5OC ",   sizeof(fault_str));
        if (sup_fault_3v3_uv) strlcat(fault_str, "3.3UV ", sizeof(fault_str));
        if (sup_fault_3v3_ov) strlcat(fault_str, "3.3OV ", sizeof(fault_str));
        if (sup_fault_3v3_oc) strlcat(fault_str, "3.3OC ", sizeof(fault_str));
        if (fault_str[0] == '\0') strlcat(fault_str, "???", sizeof(fault_str));
    }

    const char *names[] = { "STANDBY", "ENABLING", "RUNNING", "FAULT" };
    Serial.printf("[SEQ] %s → %s\n", names[s_prev_state], names[next]);
}

const char* supervisor_last_fault() { return s_last_fault_str; }

static inline unsigned long ms_in_state() { return millis() - s_state_entry_ms; }
static inline bool mb_wants_power_on()    { return digitalRead(PS_ON_IN) == LOW; }

static void psu_enable()  { digitalWrite(PSU_EN_OUT, HIGH); }  // HIGH = inverter runs PSU
static void psu_disable() { digitalWrite(PSU_EN_OUT, LOW);  }  // LOW  = inverter kills PSU

// ── Internal supervisor state ─────────────────────────────────────────────────
static bool           s_powerBad       = false;
static unsigned long  s_faultClearedAt = 0;
static bool           s_faultWasActive = false;

// ── Mock / test injection ─────────────────────────────────────────────────────
static bool  s_mockEnabled = false;
static float s_mock_v12 = 12.0f, s_mock_a12 = 0.5f;
static float s_mock_v5  =  5.0f, s_mock_a5  = 0.5f;
static float s_mock_v33 =  3.3f, s_mock_a33 = 0.3f;

void supervisor_set_mock(bool enabled,
                         float v12 = 12.0f, float a12 = 0.5f,
                         float v5  =  5.0f, float a5  = 0.5f,
                         float v33 =  3.3f, float a33 = 0.3f) {
    s_mockEnabled = enabled;
    s_mock_v12 = v12;  s_mock_a12 = a12;
    s_mock_v5  = v5;   s_mock_a5  = a5;
    s_mock_v33 = v33;  s_mock_a33 = a33;
}

// ── Public query functions ────────────────────────────────────────────────────
bool supervisor_power_bad() { return s_powerBad; }
bool supervisor_any_uv()    { return sup_fault_12v_uv || sup_fault_5v_uv || sup_fault_3v3_uv; }
bool supervisor_any_ov()    { return sup_fault_12v_ov || sup_fault_5v_ov || sup_fault_3v3_ov; }
bool supervisor_any_oc()    { return sup_fault_12v_oc || sup_fault_5v_oc || sup_fault_3v3_oc; }

// ── Setup — call once from setup(), after Wire and INA219 are ready ───────────
void supervisor_setup() {
    pinMode(PS_ON_IN,   INPUT_PULLUP);  // Pulled high; MB shorts to GND to request on
    pinMode(PSU_EN_OUT, OUTPUT);
    psu_disable();                      // Safe default: PSU off at boot
    enter_state(PSU_STATE_STANDBY);
    Serial.println("[SUP] Supervisor ready — PSU in standby");
}

// ── Rail checker — call every loop(), internally rate-limited to 100 Hz ──────
void supervisor_update() {
    static unsigned long lastRun = 0;
    unsigned long now = millis();
    if (now - lastRun < SUPERVISOR_INTERVAL_MS) return;
    lastRun = now;

    // Only bother reading rails when the PSU is actually on
    if (s_psu_state == PSU_STATE_STANDBY) return;

    // ── Read rails (live or mock) ─────────────────────────────────────────────
    float v12, a12, v5, a5, v33, a33;
    if (s_mockEnabled) {
        v12 = s_mock_v12;  a12 = s_mock_a12;
        v5  = s_mock_v5;   a5  = s_mock_a5;
        v33 = s_mock_v33;  a33 = s_mock_a33;
    } else {
        v12 = ina_v12.getBusVoltage_V();  a12 = getCurrent_mA(ina_v12) / 1000.0f;
        v5  = ina_v5.getBusVoltage_V();   a5  = getCurrent_mA(ina_v5)  / 1000.0f;
        v33 = ina_v33.getBusVoltage_V();  a33 = getCurrent_mA(ina_v33) / 1000.0f;
    }

    // ── Evaluate thresholds ───────────────────────────────────────────────────
    sup_fault_12v_uv = (v12 < UV_12V);
    sup_fault_12v_ov = (v12 > OV_12V);
    sup_fault_12v_oc = (a12 > OC_12V);

    sup_fault_5v_uv  = (v5  < UV_5V);
    sup_fault_5v_ov  = (v5  > OV_5V);
    sup_fault_5v_oc  = (a5  > OC_5V);

    sup_fault_3v3_uv = (v33 < UV_3V3);
    sup_fault_3v3_ov = (v33 > OV_3V3);
    sup_fault_3v3_oc = (a33 > OC_3V3);

    bool anyFault = sup_fault_12v_uv || sup_fault_12v_ov || sup_fault_12v_oc ||
                    sup_fault_5v_uv  || sup_fault_5v_ov  || sup_fault_5v_oc  ||
                    sup_fault_3v3_uv || sup_fault_3v3_ov || sup_fault_3v3_oc;

    // ── Assert fault immediately, de-assert only after RESET_HOLD_MS clean ───
    if (anyFault) {
        s_faultWasActive = true;
        s_faultClearedAt = 0;
        s_powerBad = true;
    } else if (s_faultWasActive) {
        if (s_faultClearedAt == 0) {
            s_faultClearedAt = now;
        } else if (now - s_faultClearedAt >= RESET_HOLD_MS) {
            s_faultWasActive = false;
            s_faultClearedAt = 0;
            s_powerBad = false;
        }
    } else {
        s_powerBad = false;
        s_faultClearedAt = 0;
    }

#ifdef SUPERVISOR_SERIAL_DEBUG
    if (s_powerBad || anyFault) {
        Serial.printf("[SUP] FAULT | 12V: %.2fV %.2fA %s%s%s| "
                      "5V: %.2fV %.2fA %s%s%s| 3V3: %.2fV %.2fA %s%s%s\n",
            v12, a12,
            sup_fault_12v_uv ? "UV " : "", sup_fault_12v_ov ? "OV " : "", sup_fault_12v_oc ? "OC " : "",
            v5,  a5,
            sup_fault_5v_uv  ? "UV " : "", sup_fault_5v_ov  ? "OV " : "", sup_fault_5v_oc  ? "OC " : "",
            v33, a33,
            sup_fault_3v3_uv ? "UV " : "", sup_fault_3v3_ov ? "OV " : "", sup_fault_3v3_oc ? "OC " : "");
    }
#endif
}

// ── Sequencer — call every loop(), after supervisor_update() ─────────────────
void sequencer_update() {

    switch (s_psu_state) {

    case PSU_STATE_STANDBY:
        // Enforce lockout after a fault — don't allow immediate re-enable
        if (s_prev_state == PSU_STATE_FAULT &&
            ms_in_state() < REARM_LOCKOUT_MS) break;

        if (mb_wants_power_on()) {
            psu_enable();
            enter_state(PSU_STATE_ENABLING);
        }
        break;

    case PSU_STATE_ENABLING:
        // Motherboard cancelled before rails settled
        if (!mb_wants_power_on()) {
            psu_disable();
            enter_state(PSU_STATE_STANDBY);
            break;
        }
        // Still within settling window — wait
        if (ms_in_state() < RAIL_SETTLE_MS) break;

        // Settling complete — check rails
        if (!supervisor_power_bad()) {
            enter_state(PSU_STATE_RUNNING);
        } else {
            psu_disable();
            enter_state(PSU_STATE_FAULT);
            Serial.printf("[SEQ] Startup fault — 12V:%s%s%s 5V:%s%s%s 3V3:%s%s%s\n",
                sup_fault_12v_uv?"UV ":"", sup_fault_12v_ov?"OV ":"", sup_fault_12v_oc?"OC ":"",
                sup_fault_5v_uv ?"UV ":"", sup_fault_5v_ov ?"OV ":"", sup_fault_5v_oc ?"OC ":"",
                sup_fault_3v3_uv?"UV ":"", sup_fault_3v3_ov?"OV ":"", sup_fault_3v3_oc?"OC ":"");
        }
        break;

    case PSU_STATE_RUNNING:
        // Ordered shutdown
        if (!mb_wants_power_on()) {
            psu_disable();
            enter_state(PSU_STATE_STANDBY);
            break;
        }
        // Runtime fault
        if (supervisor_power_bad()) {
            psu_disable();
            enter_state(PSU_STATE_FAULT);
            Serial.printf("[SEQ] Runtime fault — 12V:%s%s%s 5V:%s%s%s 3V3:%s%s%s\n",
                sup_fault_12v_uv?"UV ":"", sup_fault_12v_ov?"OV ":"", sup_fault_12v_oc?"OC ":"",
                sup_fault_5v_uv ?"UV ":"", sup_fault_5v_ov ?"OV ":"", sup_fault_5v_oc ?"OC ":"",
                sup_fault_3v3_uv?"UV ":"", sup_fault_3v3_ov?"OV ":"", sup_fault_3v3_oc?"OC ":"");
        }
        break;

    case PSU_STATE_FAULT:
        // Hold until motherboard releases PS_ON (acknowledges the fault)
        // REARM_LOCKOUT_MS is enforced in STANDBY on re-entry
        if (!mb_wants_power_on()) {
            enter_state(PSU_STATE_STANDBY);
        }
        break;
    }
}


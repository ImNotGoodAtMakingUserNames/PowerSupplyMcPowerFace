// ─────────────────────────────────────────────────────────────────────────────
// Supervisor self-test
// ─────────────────────────────────────────────────────────────────────────────
// Call supervisor_test_run() once per loop() — it runs a non-blocking
// sequence so lv_timer_handler() keeps ticking and the display stays alive.
//
// Sequence
// ────────
//   Phase 0  (2 s)  Inject stable, in-spec voltages.
//                   Pin should be HIGH. Printed every 500 ms.
//
//   Phase 1  (2 s)  Inject a 12 V undervoltage (9.0 V < 10.56 V threshold).
//                   Pin goes LOW immediately. Printed every 500 ms.
//
//   Phase 2  (1 s)  Restore good voltages.
//                   Pin stays LOW for the 200 ms TPS3510DR hold delay,
//                   then returns HIGH. Printed every 500 ms.
//
//   Phase 3         Test done — mock disabled, live INA219 data resumes.
//
// To run: call supervisor_test_start() once (e.g. from setup() or a button).
// ─────────────────────────────────────────────────────────────────────────────

static bool          s_testRunning   = false;
static uint8_t       s_testPhase     = 0;
static unsigned long s_testPhaseStart = 0;
static unsigned long s_testLastPrint  = 0;

void supervisor_test_start() {
    Serial.println(F("\n══════════════════════════════════════════════"));
    Serial.println(F("  PSU Supervisor self-test starting"));
    Serial.println(F("══════════════════════════════════════════════"));
    s_testPhase      = 0;
    s_testPhaseStart = millis();
    s_testLastPrint  = 0;
    s_testRunning    = true;

    // Phase 0: stable, healthy rails
    supervisor_set_mock(true,
        /*v12*/ 12.05f, /*a12*/ 0.80f,
        /*v5 */  5.01f, /*a5 */  1.20f,
        /*v33*/  3.30f, /*a33*/  0.45f);
}

// Helper — prints pin state + which faults are active
static void printSupState(const char* phase) {
    bool bad = supervisor_power_bad();
    Serial.printf("[%s]  IO18 POWER_BAD = %s\n", phase, bad ? "LOW  ⚡ FAULT" : "HIGH  ✓ OK");

    Serial.printf("        12V: %.2fV  %s%s%s\n",
        12.0f,   // printed value is the mock we set, not a re-read
        sup_fault_12v_uv ? "[UV] " : "",
        sup_fault_12v_ov ? "[OV] " : "",
        sup_fault_12v_oc ? "[OC] " : "");
    Serial.printf("         5V: %.2fV  %s%s%s\n",
        5.0f,
        sup_fault_5v_uv  ? "[UV] " : "",
        sup_fault_5v_ov  ? "[OV] " : "",
        sup_fault_5v_oc  ? "[OC] " : "");
    Serial.printf("        3V3: %.2fV  %s%s%s\n",
        3.3f,
        sup_fault_3v3_uv ? "[UV] " : "",
        sup_fault_3v3_ov ? "[OV] " : "",
        sup_fault_3v3_oc ? "[OC] " : "");
}

void supervisor_test_run() {
    if (!s_testRunning) return;

    unsigned long now     = millis();
    unsigned long elapsed = now - s_testPhaseStart;

    // ── Print every 500 ms within a phase ────────────────────────────────────
    if (now - s_testLastPrint >= 500) {
        s_testLastPrint = now;

        switch (s_testPhase) {
            case 0: printSupState("PHASE 0 — HEALTHY "); break;
            case 1: printSupState("PHASE 1 — 12V UV  "); break;
            case 2: printSupState("PHASE 2 — RECOVERY"); break;
        }
    }

    // ── Phase transitions ─────────────────────────────────────────────────────
    switch (s_testPhase) {

        case 0:   // 2 s of healthy rails
            if (elapsed >= 2000) {
                Serial.println(F("\n  ► Injecting 12 V undervoltage (9.0 V < 10.56 V threshold)"));
                supervisor_set_mock(true,
                    /*v12*/  9.00f, /*a12*/ 0.80f,   // ← below UV_12V (10.56 V)
                    /*v5 */  5.01f, /*a5 */  1.20f,
                    /*v33*/  3.30f, /*a33*/  0.45f);
                s_testPhase     = 1;
                s_testPhaseStart = now;
            }
            break;

        case 1:   // 2 s of faulted 12 V rail
            if (elapsed >= 2000) {
                Serial.println(F("\n  ► Restoring good voltages — watching 200 ms hold timer"));
                supervisor_set_mock(true,
                    /*v12*/ 12.05f, /*a12*/ 0.80f,
                    /*v5 */  5.01f, /*a5 */  1.20f,
                    /*v33*/  3.30f, /*a33*/  0.45f);
                s_testPhase     = 2;
                s_testPhaseStart = now;
            }
            break;

        case 2:   // wait for hold timer to expire + a little margin
            if (elapsed >= 1000) {
                Serial.println(F("\n══════════════════════════════════════════════"));
                Serial.printf (  "  Test complete. Final pin state: %s\n",
                                 supervisor_power_bad() ? "LOW (still in hold)" : "HIGH (cleared)");
                Serial.println(F("  Returning to live INA219 data."));
                Serial.println(F("══════════════════════════════════════════════\n"));
                supervisor_set_mock(false);   // back to real sensors
                s_testRunning = false;
            }
            break;
    }
}

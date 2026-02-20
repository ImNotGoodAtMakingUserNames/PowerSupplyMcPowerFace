#include <Arduino.h>

// Pins you specified earlier
static const uint8_t PIN_FAN_PWM  = 6;
static const uint8_t PIN_FAN_TACH = 9;

// PWM config for 4-wire fan
static const uint32_t PWM_FREQ_HZ = 25000;  // 25 kHz
static const uint8_t  PWM_RES_BITS = 10;    // 0..1023

static const uint8_t TACH_PULSES_PER_REV = 2;

volatile uint32_t g_tachPulses = 0;

void IRAM_ATTR tachISR() {
  g_tachPulses++;
}

// Because transistor inverts PWM, we invert duty here
void setFanPercent(float percent) {
  percent = constrain(percent, 0.0f, 100.0f);

  const uint32_t maxDuty = (1u << PWM_RES_BITS) - 1u;

  float espPercent = 100.0f - percent;  // invert for NPN stage
  uint32_t duty = (uint32_t)((espPercent / 100.0f) * maxDuty);

  ledcWrite(PIN_FAN_PWM, duty);
}

float measureRPM(uint32_t windowMs = 1000) {
  g_tachPulses = 0;
  delay(windowMs);

  noInterrupts();
  uint32_t pulses = g_tachPulses;
  interrupts();

  float revPerSec = (float)pulses / TACH_PULSES_PER_REV / (windowMs / 1000.0f);
  return revPerSec * 60.0f;
}

void setup() {
  Serial.begin(115200);
  delay(300);

  if (!ledcAttach(PIN_FAN_PWM, PWM_FREQ_HZ, PWM_RES_BITS)) {
    Serial.println("PWM attach failed");
    while (true);
  }

  pinMode(PIN_FAN_TACH, INPUT);
  attachInterrupt(digitalPinToInterrupt(PIN_FAN_TACH), tachISR, FALLING);

  // Kick start
  setFanPercent(100.0f);
  delay(300);
}

void loop() {
  static float percent = 40.0f;

  setFanPercent(percent);

  float rpm = measureRPM(1000);  // 1 second measurement

  Serial.print("Setpoint: ");
  Serial.print(percent);
  Serial.print("%  |  RPM: ");
  Serial.println(rpm);

  if (percent < 100.0f) {
    percent += 5.0f;
  } else {
    Serial.println("Reached 100%. Holding...");
    while (true);  // stop ramp
  }

  delay(4000);  // Remaining time so total cycle = ~5 seconds
}

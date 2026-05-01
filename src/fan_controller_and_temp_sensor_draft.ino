#include <Wire.h>
// #include <Adafruit_EMC2101.h>

// Adafruit_EMC2101 emc2101;

#define I2C_SDA     12
#define I2C_SCL     11
#define LM75A_ADDR  0x48   // default; change if you've jumpered A0-A2

// Read LM75A temperature in °C. Returns NAN on bus error.
float readLM75A() {
  Wire.beginTransmission(LM75A_ADDR);
  Wire.write(0x00);  // point to temperature register
  if (Wire.endTransmission() != 0) return NAN;

  if (Wire.requestFrom(LM75A_ADDR, 2) != 2) return NAN;
  uint8_t msb = Wire.read();
  uint8_t lsb = Wire.read();

  // 16-bit signed, valid data in bits 15..5; arithmetic shift preserves sign
  int16_t raw = ((int16_t)msb << 8) | lsb;
  return (raw >> 5) * 0.125f;
}

void setup() {
  Serial.begin(115200);
  while (!Serial) delay(10);

  Wire.begin(I2C_SDA, I2C_SCL);

  // if (!emc2101.begin()) {
  //   Serial.println("Failed to find EMC2101 at 0x4C - check wiring");
  //   while (1) delay(10);
  // }
  // Serial.println("EMC2101 found!");

  // Verify LM75A is responding
  Wire.beginTransmission(LM75A_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("LM75A found!");
  } else {
    Serial.println("WARNING: LM75A not responding at 0x48");
  }

  // emc2101.enableTachInput(true);
  // emc2101.setPWMDivisor(0);
  // emc2101.setDutyCycle(50);
}

void loop() {
  // static int duty = 25;
  // static int step = 5;

  // emc2101.setDutyCycle(duty);

  // float chipTemp = emc2101.getInternalTemperature();
  float extTemp  = readLM75A();

  // Serial.print("Duty: ");
  // Serial.print(duty);
  // Serial.print("%  |  RPM: ");
  // Serial.print(emc2101.getFanRPM());
  // Serial.print("  |  EMC2101 chip: ");
  // Serial.print(chipTemp, 1);
  // Serial.print(" C  |  External (LM75A): ");
  Serial.print("LM75A: ");
  if (isnan(extTemp)) {
    Serial.println("READ ERROR");
  } else {
    Serial.print(extTemp, 2);
    Serial.println(" C");
  }

  // duty += step;
  // if (duty >= 100 || duty <= 25) step = -step;

  delay(2000);
}
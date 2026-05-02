#include <lvgl.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ACS37800.h>
#include <Arduino_GFX_Library.h>
#include "PowerSupply.h"

#define TFT_BL 2
#define SDA_PIN 37
#define SCL_PIN 38

// I2C addrs
  // INA219's (DC Volt/Amp measure)
  #define v12_ina219_adrr 0x40
  #define v5_ina219_addr 0x41
  #define v33_ina219_addr 0x44

  // ACS37800 (AC Volt/Amp measure)
  #define v120_acs37800_addr 0x60

  // EMC2101 (PWM Fan Controller)
  #define emc2101_addr 0x4c

  // LM75A (Temp Sensor)
  #define lm75a_addr  0x48


/* ---- INA219's ---- */
Adafruit_INA219 ina_v12(v12_ina219_adrr);
Adafruit_INA219 ina_v5(v5_ina219_addr);
Adafruit_INA219 ina_v33(v33_ina219_addr);

const float SHUNT_MOHM = 50.0f;

float getCurrent_mA(Adafruit_INA219 &ina) {
  return ina.getShuntVoltage_mV() / (SHUNT_MOHM / 1000.0f);
}



/* ---- ACS37800 ---- */




/* ---- EMC2101 ---- */




/* ---- LM75A ---- */


/* ---- Display ---- */
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  GFX_NOT_DEFINED, GFX_NOT_DEFINED, GFX_NOT_DEFINED,
  40, 41, 39, 42,
  45, 48, 47, 21, 14,
  5, 6, 7, 15, 16, 4,
  8, 3, 46, 9, 1
);

Arduino_RPi_DPI_RGBPanel *lcd = new Arduino_RPi_DPI_RGBPanel(
  bus,
  480, 0, 8, 4, 43,
  272, 0, 8, 4, 12,
  1, 7000000, true
);

void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

uint32_t my_tick(void) { return millis(); }


/* ---- LVGL ---- */
const uint32_t SCR_W = 480;
const uint32_t SCR_H = 272;
static lv_color_t buf1[SCR_W * 40];


/* ---- Timing ---- */
static unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 500;

void setup() {
  // Serial Viewer
  Serial.begin(115200);
  delay(500);

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  if (!ina_v12.begin()) Serial.println("12V INA219 NOT FOUND");
  if (!ina_v5.begin()) Serial.println("5V INA219 NOT FOUND");
  if (!ina_v33.begin()) Serial.println("3.3V INA219 NOT FOUND");

  ina_v12.setCalibration_32V_1A();
  ina_v5.setCalibration_32V_1A();
  ina_v33.setCalibration_32V_1A();



  // Display & GUI
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  lcd->begin();
  lcd->fillScreen(BLACK);

  lv_init();
  lv_tick_set_cb(my_tick);

  lv_display_t *disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);



  // Initiate GUI
  ps_gui();
}

void loop() {
  unsigned long now = millis();

  // 
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;

    float v12_rail_volts = ina_v12.getBusVoltage_V();
    float v12_rail_milliamps  = getCurrent_mA(ina_v12) / 1000.0f;

    float v5_rail_volts = ina_v5.getBusVoltage_V();
    float v5_rail_milliamps  = getCurrent_mA(ina_v5) / 1000.0f;

    float v33_rail_volts = ina_v33.getBusVoltage_V();
    float v33_rail_milliamps  = getCurrent_mA(ina_v33) / 1000.0f;   


    // ps_set_values(volts, amps); //Change for updated gui w/ multiple volt/amp values

    // Serial.printf("Voltage: %.3f V  |  Current: %.3f A\n", volts, amps);
  }

  lv_timer_handler();
  delay(5);
}
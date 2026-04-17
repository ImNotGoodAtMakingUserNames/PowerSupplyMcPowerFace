#include <lvgl.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Arduino_GFX_Library.h>
#include "touch.h"
#include "PowerSupply.h"

#define TFT_BL 2

/* ---- INA219 ---- */
Adafruit_INA219 inaA(0x40);

const float SHUNT_MOHM = 50.0f;

float getCurrent_mA(Adafruit_INA219 &ina) {
  return ina.getShuntVoltage_mV() / (SHUNT_MOHM / 1000.0f);
}

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

/* ---- LVGL ---- */
static const uint32_t SCR_W = 480;
static const uint32_t SCR_H = 272;
static lv_color_t buf1[SCR_W * 40];

static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touch_has_signal() && touch_touched()) {
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = touch_last_x;
    data->point.y = touch_last_y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

static uint32_t my_tick(void) { return millis(); }

/* ---- Timing ---- */
static unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 500;

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  lcd->begin();
  lcd->fillScreen(BLACK);

  touch_init();

  Wire.begin(37, 38);
  if (!inaA.begin()) Serial.println("INA219 0x40 NOT FOUND");
  inaA.setCalibration_32V_1A();

  lv_init();
  lv_tick_set_cb(my_tick);

  lv_display_t *disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1), LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  ps_gui();
}

void loop() {
  unsigned long now = millis();
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;

    float volts = inaA.getBusVoltage_V();
    float amps  = getCurrent_mA(inaA) / 1000.0f;

    ps_set_values(volts, amps);

    Serial.printf("Voltage: %.3f V  |  Current: %.3f A\n", volts, amps);
  }

  lv_timer_handler();
  delay(5);
}
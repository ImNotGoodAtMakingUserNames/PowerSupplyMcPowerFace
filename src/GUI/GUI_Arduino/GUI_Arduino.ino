#include <lvgl.h>
#include <SPI.h>
#include <Arduino_GFX_Library.h>
#include "touch.h"
#include "PowerSupply.h"

#define TFT_BL 2

/* ---- Display: Elecrow 4.3" HMI (480x272 RGB parallel) ---- */
Arduino_ESP32RGBPanel *bus = new Arduino_ESP32RGBPanel(
  GFX_NOT_DEFINED /* CS */, GFX_NOT_DEFINED /* SCK */, GFX_NOT_DEFINED /* SDA */,
  40 /* DE */, 41 /* VSYNC */, 39 /* HSYNC */, 42 /* PCLK */,
  45, 48, 47, 21, 14,                 // R0..R4
  5, 6, 7, 15, 16, 4,                 // G0..G5
  8, 3, 46, 9, 1                      // B0..B4
);

Arduino_RPi_DPI_RGBPanel *lcd = new Arduino_RPi_DPI_RGBPanel(
  bus,
  480, 0, 8, 4, 43,                   // hsync
  272, 0, 8, 4, 12,                   // vsync
  1, 7000000, true
);

/* ---- LVGL buffers ---- */
static const uint32_t SCR_W = 480;
static const uint32_t SCR_H = 272;
static lv_color_t buf1[SCR_W * 40];   // 40 lines — bigger buffer = smoother redraws

/* ---- LVGL v9 flush callback ---- */
static void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  lcd->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)px_map, w, h);
  lv_display_flush_ready(disp);
}

/* ---- LVGL v9 touch read callback ---- */
static void my_touchpad_read(lv_indev_t *indev, lv_indev_data_t *data) {
  if (touch_has_signal() && touch_touched()) {
    data->state   = LV_INDEV_STATE_PRESSED;
    data->point.x = touch_last_x;
    data->point.y = touch_last_y;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

/* ---- LVGL v9 tick callback ---- */
static uint32_t my_tick(void) {
  return millis();
}

void setup() {
  Serial.begin(115200);

  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  lcd->begin();
  lcd->fillScreen(BLACK);

  touch_init();

  lv_init();
  lv_tick_set_cb(my_tick);

  lv_display_t *disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_flush_cb(disp, my_disp_flush);
  lv_display_set_buffers(disp, buf1, NULL, sizeof(buf1),
                         LV_DISPLAY_RENDER_MODE_PARTIAL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(indev, my_touchpad_read);

  ps_gui();
}

void loop() {
  lv_timer_handler();
  delay(5);
}
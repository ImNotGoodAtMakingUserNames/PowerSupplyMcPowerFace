#include <lvgl.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_INA219.h>
#include <Adafruit_EMC2101.h>
#include <ACS37800.h>
#include <Arduino_GFX_Library.h>
#include <DFRobot_DHT20.h>
#include <math.h>
#include "PowerSupply.h"
// #include "telemetry.cpp"

//To disable wifi/bluetooth
#include "esp_wifi.h"
#include "esp_bt.h"

#define TFT_BL 2
#define SDA_PIN 37
#define SCL_PIN 38

// PS_ON pin numbers declared in psu_supervisor.ino

// I2C addrs
  // INA219's (DC Volt/Amp measure)
  #define v12_ina219_adrr 0x40
  #define v5_ina219_addr 0x41
  #define v33_ina219_addr 0x44

  // ACS37800 (AC Volt/Amp measure)
  #define v120_acs37800_addr 0x60  //Library defaults to this, here for debugging

  // EMC2101 (PWM Fan Controller)
  #define emc2101_addr 0x4c       //Library defaults to this, here for debugging

  // DHT20 (Temp Sensor)
  #define dht20_addr  0x38


/* ---- INA219's ---- */
Adafruit_INA219 ina_v12(v12_ina219_adrr);
Adafruit_INA219 ina_v5(v5_ina219_addr);
Adafruit_INA219 ina_v33(v33_ina219_addr);

const float SHUNT_MOHM = 50.0f;

float getCurrent_mA(Adafruit_INA219 &ina) {
  return ina.getShuntVoltage_mV() / (SHUNT_MOHM / 1000.0f);
}



/* ---- ACS37800 ---- */
ACS37800 acs;

/* ---- EMC2101 ---- */
Adafruit_EMC2101 emc;

// --- Fan Control ---
const float FAN_START_TEMP_C  = 35.0f;  // °C at which fan turns on
const float FAN_BASE_DUTY     = 50.0f;  // % duty cycle at start temp
const float FAN_DUTY_PER_DEG  = 5.0f;  // % added per °C above start temp
const float FAN_MAX_DUTY      = 100.0f;

void updateFanSpeed(float temp_c) {
  float duty;
  if (temp_c < FAN_START_TEMP_C) {
    duty = 0.0f;  // Fan off below threshold
  } else {
    float overage = temp_c - FAN_START_TEMP_C;
    duty = FAN_BASE_DUTY + (overage * FAN_DUTY_PER_DEG);
    duty = min(duty, FAN_MAX_DUTY);
  }
  emc.setDutyCycle((uint8_t)duty);
}


/* ---- DHT20 ---- */
DFRobot_DHT20 dft;

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
// static lv_color_t buf1[SCR_W * 40];
static lv_color_t *buf1 = nullptr;
static lv_color_t *buf2 = nullptr;


/* ---- Timing ---- */
static unsigned long lastSensorRead = 0;
const unsigned long SENSOR_INTERVAL_MS = 500;

void setup() {

  esp_wifi_stop();
  esp_wifi_deinit();
  esp_bt_controller_disable();

  // Serial Viewer
  Serial.begin(115200);
  delay(500);
  

  // I2C
  Wire.begin(SDA_PIN, SCL_PIN);
  Serial.println("Wire initiated");

  delay(2000);

  Wire.beginTransmission(0x60); if(Wire.endTransmission()){Serial.println("ACS37800 NOT FOUND");} else{Serial.println("ACS37800 CONNECTED");}
  if (!ina_v12.begin()){ Serial.println("12V INA219 NOT FOUND");} else Serial.println("12V INA219 CONNECTED");
  if (!ina_v5.begin()){ Serial.println("5V INA219 NOT FOUND");} else Serial.println("5V INA219 CONNECTED") ;
  if (!ina_v33.begin()){ Serial.println("3.3V INA219 NOT FOUND");} else Serial.println("3.3V INA219 CONNECTED");
  if (!emc.begin()){ Serial.println("EMC2101 (Fan Controller) NOT FOUND");} else Serial.println("EMC2101 (Fan Controller) CONNECTED");
  if (dft.begin()){ Serial.println("DHT20 (Temp Sensor) NOT FOUND");} else Serial.println("DHT20 (Temp Sensor) CONNECTED");

  Serial.println("I2C devices initiated");

  // INA setup
  ina_v12.setCalibration_32V_1A();
  ina_v5.setCalibration_32V_1A();
  ina_v33.setCalibration_32V_1A();

  supervisor_setup();
  fault_log_init();
  fault_log_print_saved();   

  // ACS setup
  acs.setBoardPololu(4); //4k-ohm test res for 120V RMS
  acs.setSampleCount(0); //0 for each zero-crossing value, otherwise 4-1023 samples

  // EMC setup
  emc.enableTachInput(true);
  emc.setPWMDivisor(0);
  emc.setDutyCycle(50);


  // Display & GUI
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  lcd->begin();
  lcd->fillScreen(BLACK);

  lv_init();
  lv_tick_set_cb(my_tick);

  lv_display_t *disp = lv_display_create(SCR_W, SCR_H);
  lv_display_set_flush_cb(disp, my_disp_flush);
  
  buf1 = (lv_color_t*)heap_caps_malloc(SCR_W * SCR_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);
  buf2 = (lv_color_t*)heap_caps_malloc(SCR_W * SCR_H * sizeof(lv_color_t), MALLOC_CAP_SPIRAM);

  if (!buf1 || !buf2) {
      Serial.println("PSRAM buffer allocation failed!");
      while(1);
  }

  lv_display_set_buffers(disp, buf1, buf2,
  SCR_W * SCR_H * sizeof(lv_color_t),
  LV_DISPLAY_RENDER_MODE_FULL);

  lv_indev_t *indev = lv_indev_create();
  lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);



  // Initiate GUI
  ps_gui();
}


void sendTestValues() {
    static float t = 0.0f;
    t += 0.05f;
    if (t > TWO_PI) t -= TWO_PI;

    float v12_rail_volts = 12.0f + sinf(t)        * 0.5f;
    float v12_rail_amps  = 1.0f  + sinf(t + 0.5f) * 0.8f;

    float v5_rail_volts  = 5.0f  + sinf(t + 1.0f) * 0.2f;
    float v5_rail_amps   = 0.5f  + sinf(t + 1.5f) * 0.4f;

    float v33_rail_volts = 3.3f  + sinf(t + 2.0f) * 0.1f;
    float v33_rail_amps  = 0.3f  + sinf(t + 2.5f) * 0.2f;

    float ac_watts       = 200.0f + sinf(t + 0.3f) * 80.0f;  // 120 – 280W
    float dc_watts       = ac_watts * (0.90f + sinf(t + 1.0f) * 0.02f); // tracks AC, efficiency drifts 88–92%
    float efficiency     = (dc_watts / ac_watts) * 100.0f;
    float temp_c         = 35.0f + sinf(t + 1.2f) * 15.0f;

    ps_set_values(v12_rail_volts, v12_rail_amps,
                  v5_rail_volts,  v5_rail_amps,
                  v33_rail_volts, v33_rail_amps,
                  ac_watts,       dc_watts,
                  efficiency,     temp_c);
}



void loop() {
  unsigned long now = millis();

  // 
  if (now - lastSensorRead >= SENSOR_INTERVAL_MS) {
    lastSensorRead = now;

    // DC Rail power values
    float v12_rail_volts = ina_v12.getBusVoltage_V();
    float v12_rail_amps  = getCurrent_mA(ina_v12) / 1000.0f;
    float v12_rail_watts = ina_v12.getPower_mW() / 1000.0f;

    float v5_rail_volts = ina_v5.getBusVoltage_V();
    float v5_rail_amps  = getCurrent_mA(ina_v5) / 1000.0f;
    float v5_rail_watts = ina_v12.getPower_mW() / 1000.0f;

    float v33_rail_volts = ina_v33.getBusVoltage_V();
    float v33_rail_amps  = getCurrent_mA(ina_v33) / 1000.0f; 
    float v33_rail_watts = ina_v33.getPower_mW() / 1000.0f;  

    // AC input power value
    acs.readActiveAndReactivePower();
    int32_t ac_watts = acs.activePowerMilliwatts / 1000.f;

    float dc_watts = v12_rail_watts + v5_rail_watts + v33_rail_watts;

    float efficiency = roundf((dc_watts / ac_watts) * 100) / 100;

    // Temp sensor value
    float temp_c              =  dft.getTemperature(); 
    if (temp_c >= 149.9) temp_c = NAN;
    //(temperatureRead() - 32) / 1.8;
    // updateFanSpeed(temp_c); 
    // Push everything to the GUI


    ps_set_values(v12_rail_volts, v12_rail_amps,
                  v5_rail_volts,  v5_rail_amps,
                  v33_rail_volts, v33_rail_amps,
                  ac_watts,       dc_watts,
                  efficiency,      temp_c);

    fault_log_record(v12_rail_volts, v12_rail_amps,
                    v5_rail_volts,  v5_rail_amps,
                    v33_rail_volts, v33_rail_amps,
                    ac_watts,       dc_watts,
                    efficiency,     temp_c);

    Serial.printf("--- 12V: %.3fV %.3fA | 5V: %.3fV %.3fA | 3.3V: %.3fV %.3fA | AC: %.2fW | Temp: %.1f°C ---\n",
                  v12_rail_volts, v12_rail_amps,
                  v5_rail_volts,  v5_rail_amps,
                  v33_rail_volts, v33_rail_amps,
                  ac_watts, temp_c);

  }

  // sendTestValues();
  
  supervisor_update();   // reads rails, sets s_powerBad
  sequencer_update();    // acts on s_powerBad, drives PSU_EN_OUT 

  supervisor_test_run();
  lv_timer_handler();
  delay(5);
}
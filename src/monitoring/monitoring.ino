#include <Wire.h>

// TEST CONSTANTS
PG_TEST_ENABLE = false;
SYS_AC_VALS_TEST_ENABLE = false;
ATX_RAIL_TELEM_TEST_ENABLE = false;
SYS_PWR_OFF_TEST_ENABLE = false;



// ATX rail test: Have a rail start in standard, then lower until its -6% adn throw a "bad" signal

// PG signal: Set PG to true for 10 seconds then throw bad and show response time

//


// Pin assignments
#define I2C_SDA  38   // INPUT  Data pin
#define I2C_SCL  39   // INPUT  Clock pin
#define PWR_GOOD 40   // INPUT  Power Good
#define PWR_BAD  41   // OUTPUT Power Bad

// I2C input address
#define TELEM_V12_ADDR 0x40 
#define TELEM_V5_ADDR  0x44
#define TELEM_V3_ADDR  0x41


// INA219
#define INA_PWR_REG 0x03



// ACS37800
#define ACS_APWR_REG 0x21



// Global Variables
uint32_t boot_time = NULL;
uint32_t time_since_pwr_check = NULL;
uint32_t time_since_pwr_bad = NULL;
uint32_t time_since_last_measure = NULL;

uint8_t telem_data = NULL;

uint8_t curr_V12_tel[2] = {0x00, 0x00};
uint8_t curr_V5_tel[2]  = {0x00, 0x00};
uint8_t curr_V3_tel[2]  = {0x00, 0x00};

void setup() {
  Serial.begin(115200);

  pinMode(PWR_GOOD, INPUT_PULLUP);
  pinMode(PWR_BAD,  OUTPUT);

  Wire.begin(I2C_SDA,I2C_SCL);
  // if(!lox.begin()){
  //   Serial.println("Failed to detect I2C line, check connection. Suspending system...");
  //   while(1);
  // }


}



bool check_PWR_GOOD(){
  
  if(!PWR_GOOD_SIG){
    return false;
  }
  return true;

}


uint8_t get_telem(){

  // Wire.beginTransmission(TELEM_V12_ADDR);
  // if(Wire.requestFrom(TELEM_V12_ADDR, (uint8_t)1) == 1){
  //   curr_V12_tel = {Wire.read(), millis()};
  // }
  // Wire.endTransmission(TELEM_V12_ADDR);
  

  // Wire.beginTransmission(TELEM_V3_ADDR);
  // if(Wire.requestFrom(TELEM_V5_ADDR, (uint8_t)1) == 1){
  //   curr_V5_tel = {Wire.read(), millis()};
  // }
  // Wire.endTransmission(TELEM_V3_ADDR);


  // Wire.beginTransmission(TELEM_V5_ADDR);
  // if(Wire.requestFrom(TELEM_V3_ADDR, (uint8_t)1) == 1){
  //   curr_V3_tel = {Wire.read(), millis()};
  // }
  // Wire.endTransmission(TELEM_V3_ADDR);

}


void loop() {
  
  check_PWR_GOOD();
  telem_data = get_telem();
  




}

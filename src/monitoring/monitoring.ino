#include <Wire.h>

// TEST CONSTANTS
bool PG_TEST_ENABLE = false;
bool SYS_AC_VALS_TEST_ENABLE = false;
bool ATX_RAIL_TELEM_TEST_ENABLE = false;
bool SYS_PWR_OFF_TEST_ENABLE = true;



// PG_test values
bool PWR_GOOD_SIG = true;
unsigned long PG_sample_sec = 0;
unsigned long PG_sample_count = 0;
unsigned long PG_test_start_time = 0;
unsigned long PG_test_trigger_time = 0;
unsigned long PG_test_hook_time = 0;
bool end_pg_test = false;


//AC Vals test
float P_act_val = 100;
unsigned long AC_test_start_time = 0;
unsigned long AC_sample_sec = 0;
unsigned long AC_sample_count = 0;


// SYS_PWR_OFF test
int CURR_TEST_IDX = 0; // 0 for 12V, 1 for 5V, 2 for 3.3V
bool H_OR_L        = true; // true for high, false for low

float atx_test_vals[3] = {12.0f,5.0f,3.3f};
unsigned long ATX_sample_sec = 0;
unsigned long ATX_fault_trigger_time = 0;
unsigned long ATX_fault_hook_time    = 0;
float         ATX_out_of_spec_time = 0;
bool          fault = false;

struct Rails {
  float v12;
  float v5;
  float v3;
};

Rails RailVals = {12.0f, 5.0f, 3.3f}; 

int ATX_sample_count = 0;

// Pin assignments
#define I2C_SDA  8   // INPUT  Data pin
#define I2C_SCL  9   // INPUT  Clock pin
#define PWR_GOOD 40   // INPUT  Power Good
#define PWR_BAD  41   // OUTPUT Power Bad

// I2C input address
#define TELEM_V12_ADDR 0x40 
#define TELEM_V5_ADDR  0x44
#define TELEM_V3_ADDR  0x41


// INA219
#define INA_PWR_REG 0x03


// ACS37800
#define ACS_ADDR     0x50
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
  Serial.println("SYSTEM STARTING");

  pinMode(PWR_GOOD, INPUT_PULLUP);
  if(PG_TEST_ENABLE){
    Serial.println("pwr_good_test_running. PG_Signal HIGH");
  }
  
  pinMode(PWR_BAD,  OUTPUT);

  Wire.begin(I2C_SDA,I2C_SCL);

  if(SYS_AC_VALS_TEST_ENABLE){
    Serial.print("Reading from register: 0x");
    Serial.println(ACS_APWR_REG, HEX);
  }

  if(SYS_PWR_OFF_TEST_ENABLE){
    Serial.println("The ATX rail values are being read at:");
    Serial.print("12V addr: ");
    Serial.println(TELEM_V12_ADDR, HEX);
    Serial.print("5V addr: ");
    Serial.println(TELEM_V5_ADDR, HEX);
    Serial.print("3V addr: ");
    Serial.println(TELEM_V3_ADDR, HEX);

    Serial.println("SYS_PWR_OFF_TEST_ENABLE. sys_pwr_off set to LOW");
  }

  if(ATX_RAIL_TELEM_TEST_ENABLE){
    RailVals.v12 = 186;
    RailVals.v5 = 89;
    RailVals.v3 = 22;

  }

}

void pwr_good_test(){

  if(end_pg_test){
    return;
  }


  if(PG_test_start_time == 0){
    PG_test_start_time = millis();
  }

  if(PG_sample_sec == 0){
    PG_sample_sec = millis();
  }

  if(millis() - PG_sample_sec  >= 1000){
    Serial.print("PG Signal read: ");
    Serial.print(PG_sample_count);
    Serial.println(" times in the last 1 second.");
    PG_sample_sec = millis();
    PG_sample_count = 0;
  }
  else{
    PG_sample_count += 1;
  }

  if(millis() - PG_test_start_time >= 10000){
    PG_test_trigger_time = micros();
    PWR_GOOD_SIG = false;
    PG_test_hook_time = micros();
    unsigned long PG_resp_time = (PG_test_hook_time - PG_test_trigger_time);
    Serial.println("PG SIGNAL TRIGGERED LOW IN: ");
    Serial.println(PG_resp_time);
    Serial.print("MILLISECOND(S)");
    end_pg_test = true;
  }


}


void request_atx_rail_pwr_test(){

  if (Wire.requestFrom(TELEM_V12_ADDR, 2) != 2){
    RailVals.v12 = atx_test_vals[0];
  }
  if (Wire.requestFrom(TELEM_V5_ADDR, 2) != 2){
    RailVals.v5 = atx_test_vals[1];
  }  
  if (Wire.requestFrom(TELEM_V3_ADDR, 2) != 2){
    RailVals.v3 = atx_test_vals[2];
  }

  return;

}

bool check_atx_rail_fault(float v12, float v5, float v3){

  float v12_fault_amt_h = 12 * 1.05;
  float v12_fault_amt_l = 12 * 0.95;
  float v5_fault_amt_h = 5 * 1.05;
  float v5_fault_amt_l = 5 * 0.95;
  float v3_fault_amt_h = 3.3 * 1.05;
  float v3_fault_amt_l = 3.3 * 0.95;

  bool v12_fault = (v12 >= v12_fault_amt_h) || (v12 <=  v12_fault_amt_l);
  bool v5_fault = (v5 >= v5_fault_amt_h) || (v5 <= v5_fault_amt_l);
  bool v3_fault = (v3 >= v3_fault_amt_h) || (v3 <= v3_fault_amt_l);

  if(v12_fault || v5_fault || v3_fault){
    return true;
  }
  else{
    return false;
  }

}


void atx_vals_test(){

  if(fault){
    return;
  }

  if (ATX_sample_sec == 0) {
    ATX_sample_sec = millis();
    // ATX_sample_count = 0;
  }

  request_atx_rail_pwr_test();

  bool fault_check = check_atx_rail_fault(RailVals.v12, RailVals.v5, RailVals.v3);

  if(fault_check){
    delay(100);
    if (check_atx_rail_fault(RailVals.v12, RailVals.v5, RailVals.v3)){
      ATX_fault_trigger_time = micros();
      Serial.print("ATX RAIL(S) +/- 5% OUT OF SPEC FOR 100ms, pin 0x");
      Serial.print(PWR_BAD, HEX);
      Serial.println("set to HIGH, SHUTTING DOWN");
      digitalWrite(PWR_BAD, HIGH);
      ATX_fault_hook_time = micros();
      unsigned long resp_us = ATX_fault_hook_time - ATX_fault_trigger_time;

      Serial.print("PWR_BAD/sys_pwr_off set to HIGH in ");
      Serial.print(resp_us);
      Serial.println(" microsescond(s)");

          Serial.println("Latest rail values: ");
          Serial.print("12V: ");
          Serial.println(RailVals.v12);
          Serial.print("5V: ");
          Serial.println(RailVals.v5);
          Serial.print("3V: ");
          Serial.println(RailVals.v3);
      fault = true;
    }
  }

  if(millis() - ATX_sample_sec  >= 1000){
    

    // Random value to sub/add to voltages
    float adjust_val = random(0,1001) / 10000.0;

    if(H_OR_L){
      atx_test_vals[CURR_TEST_IDX] += adjust_val;
    }
    else{
      atx_test_vals[CURR_TEST_IDX] -= adjust_val;
    }

    Serial.println("Latest rail values: ");
    Serial.print("12V: ");
    Serial.println(RailVals.v12);
    Serial.print("5V: ");
    Serial.println(RailVals.v5);
    Serial.print("3V: ");
    Serial.println(RailVals.v3);
    
    
    // Serial.print("ATX Signal read: ");
    // Serial.print(ATX_sample_count);
    // Serial.println(" times in the last 1 second.");
    ATX_sample_sec = millis();

  }
  // else{
  //   ATX_sample_count += 1;
  // }







  // Serial.println("The ATX rails");

}

void ac_vals_test(){

  if(AC_test_start_time == 0){
    AC_test_start_time = millis();
  }

  if(AC_sample_sec == 0){
    AC_sample_sec = millis();
    AC_sample_count = 0;
  }

  Wire.beginTransmission(ACS_ADDR);
  Wire.write(ACS_APWR_REG);
  Wire.requestFrom(ACS_APWR_REG,2);

  if(Wire.endTransmission(false) == 2){
    Serial.println("AC power request successful, test values used in following logic");
  }

  AC_sample_count++;

  if(millis() - AC_sample_sec  >= 1000){
    Serial.print("sys_ac_vals read: ");
    Serial.print(AC_sample_count);
    Serial.println(" times in the last 1 second.");
    AC_sample_sec = millis();
    AC_sample_count = 0;

    Serial.print("P_act_val:");
    Serial.println(P_act_val);
    P_act_val += 50;

    AC_sample_sec = millis();
    AC_sample_count = 0;

  }


}

void atx_pwr_test(){
  if (ATX_sample_sec == 0){
    ATX_sample_sec = millis();
  }

  Wire.beginTransmission(TELEM_V12_ADDR);
  Wire.write(INA_PWR_REG);
  Wire.endTransmission(false);
  Wire.requestFrom(TELEM_V12_ADDR,2);
  RailVals.v12 += 0.0001;


  Wire.beginTransmission(TELEM_V5_ADDR);
  Wire.write(INA_PWR_REG);
  Wire.endTransmission(false);
  Wire.requestFrom(TELEM_V5_ADDR,2);
  RailVals.v5 += 0.0001;


  Wire.beginTransmission(TELEM_V3_ADDR);
  Wire.write(INA_PWR_REG);
  Wire.endTransmission(false);
  Wire.requestFrom(TELEM_V3_ADDR,2);
  RailVals.v3 += 0.0001;



  ATX_sample_count++;

  if(millis() - ATX_sample_sec  >= 1000){
    Serial.print("ATX rails power read: ");
    Serial.print(ATX_sample_count);
    Serial.println(" times in the last 1 second.");

    Serial.print("12V:");
    Serial.println(RailVals.v12);
    Serial.print("5V:");
    Serial.println(RailVals.v5);
    Serial.print("3V:");
    Serial.println(RailVals.v3);

    ATX_sample_sec = millis();
    ATX_sample_count = 0;

  }


}


void loop() {

  if(PG_TEST_ENABLE){
    pwr_good_test();
  
  }

  if(SYS_PWR_OFF_TEST_ENABLE){
    atx_vals_test();
  }

  if(SYS_AC_VALS_TEST_ENABLE){
    ac_vals_test();
  }

  if(ATX_RAIL_TELEM_TEST_ENABLE){
    atx_pwr_test();

  }
  




}

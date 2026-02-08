#include <Wire.h>


// Pin assignments
#define I2C_SDA  38   // INPUT  Data pin
#define I2C_SCL  39   // INPUT  Clock pin
#define PWR_GOOD 40   // INPUT  Power Good
#define PWR_BAD  41   // OUTPUT Power Bad

// I2C input address
#define TELEM_V12_ADDR 0x40 
#define TELEM_V3_ADDR  0x41
#define TELEM_V5_ADDR  0x44


// Global Variables
uint32_t boot_time = NULL;
uint32_t time_since_pwr_bad = NULL;
uint32_t time_since_last_measure = NULL;


void setup() {
  Serial.begin(115200);

  pinMode(PWR_GOOD, INPUT);
  pinMode(PWR_BAD,  OUTPUT);


  Wire.begin(I2C_SDA,I2C_SCL);
  if(!lox.begin()){
    Serial.println("Failed to detect I2C line, check connection. Suspending system...");
    while(1);
  }


}

void loop() {


}

#include <Wire.h>
#include <ACS37800.h>

#define ac_draw_addr 0x60

#define dc_12v_draw_addr 0x40
#define dc_5v_draw_addr 0x41
#define dc_33v_draw_addr 0x44


void setup() {
  Wire.begin();
  Serial.begin(115200);

}

void loop() {
  // put your main code here, to run repeatedly:



}

#include <Wire.h>


#define I2C_SDA 38
#define I2C_SCL 39
#define CHILD_ADDR 0x12

#define HL_PIN 40

uint32_t msgCounter = 0;
unsigned long lastSent = 0;

void setup(){

// Serial.begin(115200);
Serial.begin(460800);
delay(1000);
Serial.println("ESP32-Main Test");

Wire.begin(I2C_SDA, I2C_SCL);

pinMode(HL_PIN, OUTPUT);
digitalWrite(HL_PIN, LOW);

}

void loop(){
  //Count the time and message
  double now = millis();
  double sinceLast = (now - lastSent)/1000;

  lastSent = now;
  msgCounter++;

  //Build the data
  uint8_t txByte = (uint8_t)(msgCounter & 0xFF);

  //Send the data
  Wire.beginTransmission(CHILD_ADDR);

  Wire.write(txByte);
  uint8_t send_err = Wire.endTransmission();
    if(!send_err){
      Serial.print("Sent I2C data: "); 
      Serial.print(txByte);
      Serial.print(" in ");
      Serial.print(sinceLast);
      Serial.print(" Since last message");
      // Serial.print("\n");
    }
    // else{
    //   Serial.print("Trouble with sending message, might want to look into it. Err:");
    //   Serial.print(send_err);
    //   Serial.print('\n');
    // }

  //Recieve the data
  uint8_t rxByte = 0;

    //If there's a new message...
  if(Wire.requestFrom(CHILD_ADDR, (uint8_t)1) == 1){
    rxByte = Wire.read();
      Serial.print("\n\nRecieved I2C data: "); 
      Serial.print(rxByte);
      Serial.print(" in ");
      Serial.print(sinceLast);
      Serial.print(" Since last message");
  }


  digitalWrite(HL_PIN, HIGH);
  delay(10);
  digitalWrite(HL_PIN,LOW);

  Serial.print("sent HIGH\n"); 


  delay(400);
}
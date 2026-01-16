#include <Wire.h>

#define I2C_SDA 38
#define I2C_SCL 39
#define I2C_ADDR 0x12

#define HL_IN_PIN 40


volatile bool hasReceived = false;
volatile uint8_t lastReceivedByte = 0;
volatile bool pingEdge = false;

double timeSince_recieved = 0;
double last_recieved_time = 0;


void setup(){
  Serial.begin(460800);
  // Serial.begin(115200);
  delay(1000);
  Serial.println("ESP32-Verification Test");

  Wire.begin(I2C_ADDR, I2C_SDA, I2C_SCL, 100000);
  Wire.onReceive(onReceiveHandler);
  Wire.onRequest(onRequestHandler);

  pinMode(HL_IN_PIN, INPUT); 
  attachInterrupt(digitalPinToInterrupt(HL_IN_PIN), highSigHandler, CHANGE);

}


void loop(){

  if (hasReceived) {
    double now = millis();
    timeSince_recieved = (now - last_recieved_time) / 1000;
    noInterrupts();
    uint8_t v = lastReceivedByte;
    hasReceived = false;
    interrupts();

    Serial.print("Last I2C received byte = ");
    Serial.print(v);
    Serial.print(" in this many seconds: ");
    Serial.print(timeSince_recieved);
    Serial.print("\n");

    last_recieved_time = now;
  }

  if (pingEdge){
      noInterrupts();
      pingEdge = false;
      interrupts();
      Serial.println("High signal received!\n");

  }

  delay(500);

}

void onReceiveHandler(int numBytes) {
  while (Wire.available()) {
    uint8_t b = Wire.read();
    lastReceivedByte = b;
    hasReceived = true;
  }
}

void onRequestHandler() {
  // Simple echo: send back lastReceivedByte + 1
  uint8_t resp = lastReceivedByte + 100;
  Wire.write(resp);
}

void highSigHandler(){
  pingEdge = true;
}

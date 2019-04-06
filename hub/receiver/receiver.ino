#include <RCSwitch.h>

RCSwitch mySwitch = RCSwitch();
struct SensorsData {
  int serial;
  int moisture;
  int light;
  double temparature;
  int mask = 0b000;
};

SensorsData data;

void setup() {
  Serial.begin(9600);
  mySwitch.enableReceive(0);  // pin D2
}

void loop() {
  if (mySwitch.available()) {
    
    long value = mySwitch.getReceivedValue();
    Serial.print("data: ");
    Serial.println(value);
    
    int length = mySwitch.getReceivedBitlength();
    int protocol = mySwitch.getReceivedProtocol();
    
    if (value == 0 || length != 24 || protocol != 1) {
      Serial.print("Unknown data");
      mySwitch.resetAvailable();
      return;
    }

    data.serial = value / 10000;
    
    int sensorData = value % 10000;
    int sensorId = value / 1000 % 10;
    
    if (sensorId == 1 && data.mask == 0b000) {
      data.moisture = value % 1000;
      data.mask = 0b001;
    } else if (sensorId == 2 && data.mask == 0b001) {
      data.light = value % 1000;
      data.mask = 0b011;
    } else if (sensorId == 3 && data.mask == 0b011) {
      int rawTemp = value % 1000 - 300;
      data.temparature = rawTemp / 10.0;
      data.mask = 0b111;
    }
  }
  if (data.mask == 0b111) {

      Serial.print("serial: ");
      Serial.println(data.serial);
      Serial.print("moisture: ");
      Serial.println(data.moisture);
      Serial.print("light: ");
      Serial.println(data.light);
      Serial.print("temparature: ");
      Serial.println(data.temparature);
      
      data.mask = 0x000;
    }
    mySwitch.resetAvailable();
}

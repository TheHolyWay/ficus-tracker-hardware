#include <RCSwitch.h>
#include "receiver.h"

extern RCSwitch mySwitch = RCSwitch();

SensorsData data;

void recieveRCMessage() {
  if (mySwitch.available()) {
    
    long value = mySwitch.getReceivedValue();
    Serial.print("[RC] data: ");
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

      Serial.print("[RC] serial: ");
      Serial.println(data.serial);
      Serial.print("[RC] moisture: ");
      Serial.println(data.moisture);
      Serial.print("[RC] light: ");
      Serial.println(data.light);
      Serial.print("[RC] temparature: ");
      Serial.println(data.temparature);

      onRecieveRCMessage(data);
      
      data.mask = 0x000;
    }
    mySwitch.resetAvailable();
}

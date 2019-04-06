class SensorsData {
  public:
    int serial;
    int moisture;
    int light;
    double temparature;
    int mask = 0b000;
};

void onRecieveRCMessage(SensorsData &data);
void recieveRCMessage();

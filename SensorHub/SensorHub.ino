#include <Ticker.h>
#include <ArduinoJson.h>
#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>

#include "FS.h"

#define CONFIG_FILE "/config.json"
#define TEST_CONFIG_FILE "/test_config.json"
#define LED_PIN D4

ESP8266WiFiMulti WiFiMulti;
ESP8266WebServer server(80);
WiFiClient client;
Ticker httpCaller;
String serverAddr;
String token;
boolean flagToSentData = false;
boolean ledState = false;

class DataBundle {
  public:
    int serial;
    float temperature;
    int light;
    int soilMoisture;
};

class HubConfig{
  public:
    boolean loaded = false;
    boolean isTestConfig = false;
    String ssid;
    String password;
    String token;
    String serverAddress;
};

void ledTurnTo(boolean state){
  ledState = state;
  digitalWrite(LED_PIN,ledState?LOW:HIGH);
}

void toggleLed(){
  ledState = !ledState;
  digitalWrite(LED_PIN,ledState?LOW:HIGH);
}

DataBundle produceSomeData(){
    DataBundle data;

    data.serial=13;
    data.temperature=25+((float)(rand()%30))/10;
    data.light=70+rand()%10;
    data.soilMoisture=70+rand()%10;

    return data;
}

String serializeDataToString(DataBundle data){
  DynamicJsonDocument doc(1024);

  doc["serial"] = data.serial;
  doc["temperature"] = data.temperature;
  doc["light"] = data.light;
  doc["soilMoisture"] = data.soilMoisture;

  String outputStr = "";
  serializeJson(doc,outputStr);

  return outputStr;
}

void sendData(DataBundle data){
    ledTurnTo(false);
    HTTPClient http;

    Serial.print("[HTTP] begin...\n");
    String url = serverAddr+"/api/v1/hub/"+token+"/sensor";
    Serial.print("URL - ");
    Serial.println(url);
    if (http.begin(client, url)) {  // HTTP
      http.addHeader("Content-Type","application/json");
      int httpCode = http.POST(serializeDataToString(data));
      
      if (httpCode > 0) {
        // HTTP header has been send and Server response header has been handled
        Serial.printf("[HTTP] POST... code: %d\n", httpCode);

        // file found at server
        if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = http.getString();
          Serial.println(payload);
          ledTurnTo(true);
        }
      } else {
        Serial.printf("[HTTP] POST... failed, error: %s\n", http.errorToString(httpCode).c_str());
      }

      http.end();
    } else {
      Serial.printf("[HTTP} Unable to connect\n");
    }
}

String serializeHubConfig(HubConfig config){
  DynamicJsonDocument doc(1024);

  doc["ssid"] = config.ssid;
  doc["password"] = config.password;
  doc["token"] = config.token;
  doc["serverAddress"] = config.serverAddress;

  String outputStr = "";
  serializeJson(doc,outputStr);

  return outputStr;
}

HubConfig deserializeHubConfig(String configStr){
  DynamicJsonDocument jsonDoc(1024);
  deserializeJson(jsonDoc,configStr);
  
  HubConfig config;
  
  const char *ssidBuf = jsonDoc["ssid"];
  const char *passwordBuf = jsonDoc["password"];
  const char *tokenBuf = jsonDoc["token"];
  const char *serverAddressBuf = jsonDoc["serverAddress"];
  
  config.ssid = ssidBuf;
  config.password = passwordBuf;
  config.token = tokenBuf;
  config.serverAddress = serverAddressBuf;
  config.loaded = true;

  return config;
}

void confirmTestConfig(){
  if(SPIFFS.exists(TEST_CONFIG_FILE)){
    if(SPIFFS.exists(CONFIG_FILE))SPIFFS.remove(CONFIG_FILE);
    SPIFFS.rename(TEST_CONFIG_FILE,CONFIG_FILE);
  }
}

void rejectTestConfig(){
  if(SPIFFS.exists(TEST_CONFIG_FILE)){
    if(SPIFFS.exists(TEST_CONFIG_FILE))SPIFFS.remove(TEST_CONFIG_FILE);
    ESP.restart();
  }
}
boolean pingServer(String server_addr){
    Serial.println("Pinging server \""+server_addr+"\".");
    HTTPClient http;
    
    boolean res = false;
    if (http.begin(client, server_addr + "/api/v1/hub/ping")) {  // HTTP
      int httpCode = http.GET();
      Serial.print("HTTP code - ");
      Serial.println(httpCode);
      
      if (httpCode >= 200 && httpCode < 300) {
        res = true;
      }

      http.end();
    }else{
      Serial.println("Unknown HTTP error");
    }

    return res;
}

void storeHubConfig(HubConfig config){
  File f = SPIFFS.open(TEST_CONFIG_FILE, "w+");
  if (!f) {
      Serial.println("file open failed");
  } else {
    f.print(serializeHubConfig(config));
    f.close();
  }
}

HubConfig loadHubConfig(){
  File f = SPIFFS.open(TEST_CONFIG_FILE, "r");
  boolean isTestConfig = true;
  if (!f) {
      Serial.println("test config file open failed");
      f = SPIFFS.open(CONFIG_FILE, "r");
      isTestConfig = false;
  }
  
  if (!f) {
      Serial.println("file open failed");
      return HubConfig();
  } else {
      String contents = f.readString();
      
      Serial.print("File contents: ");
      Serial.println(contents);
      
      HubConfig config = deserializeHubConfig(contents);
      config.isTestConfig = true;
      f.close();
      return config;
  }
}

void handleRequestConfig() {
  if(server.method() == HTTP_GET){
    server.send(200, "application/json", serializeHubConfig(loadHubConfig()));
  }else{
    storeHubConfig(deserializeHubConfig(server.arg("plain")));
    server.send(200, "plain/text", "OK");
    ESP.reset();
  }
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";
  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }
  server.send(404, "text/plain", message);
}

void setup() {
  pinMode(LED_PIN, OUTPUT);
  Serial.begin(115200);
  // Serial.setDebugOutput(true);

  Serial.println();
  Serial.println();
  Serial.println();

  for (uint8_t t = 4; t > 0; t--) {
    Serial.printf("[SETUP] WAIT %d...\n", t);
    Serial.flush();
    delay(1000);
  }

  SPIFFS.begin();
  HubConfig config = loadHubConfig();
  if(config.loaded){
    WiFi.mode(WIFI_STA);
    if(config.password.length()==0){
      WiFi.begin(config.ssid);
      Serial.print("Using SSID: ");
      Serial.println(config.ssid);
    }else{
      WiFi.begin(config.ssid.c_str(),config.password.c_str());
      Serial.print("Using SSID: ");
      Serial.println(config.ssid);
      Serial.print("Password: ");
      Serial.println(config.password);
    }

    serverAddr=config.serverAddress;
    token=config.token;
    
    // Wait for connection
    int tryIterations = 0;
    while (WiFi.status() != WL_CONNECTED) {
      toggleLed();
      delay(500);
      Serial.print(".");
      if(++tryIterations>20){
        ledTurnTo(false);
        Serial.println("\nNot successful connect to WiFi network");
        rejectTestConfig();
        ESP.restart();
      }
    }
    ledTurnTo(false);

    Serial.println("Successfully connected to WiFi network");
    
    Serial.println("");
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());

    httpCaller.attach(10,madeSendDate);
  }else{
    Serial.println("Start access point...");
    WiFi.softAP("sensorHubAP");
  }
  
  server.on("/config", handleRequestConfig);
  server.onNotFound(handleNotFound);
  server.begin();

  if(config.isTestConfig && !pingServer(config.serverAddress)){
    rejectTestConfig();
  }else{
    ledTurnTo(true);
  }
  
  confirmTestConfig();
}

void loop() {
  server.handleClient();

  if(flagToSentData){
    flagToSentData = false;

    if ((WiFiMulti.run() == WL_CONNECTED)) {
     Serial.println("Try to send data");
     
     DataBundle data = produceSomeData();
     sendData(data);
    } else {
      Serial.println("Wifi yet not connected");
    }
  }
}

void madeSendDate() {
  flagToSentData=true;
}

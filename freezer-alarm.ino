//This can be used to output the date the code was compiled
const char compile_date[] = __DATE__ " " __TIME__;

/************ WIFI, OTA and MQTT INFORMATION (CHANGE THESE FOR YOUR SETUP) ******************/
//#define WIFI_SSID "" //enter your WIFI SSID
//#define WIFI_PASSWORD "" //enter your WIFI Password
//#define MQTT_SERVER "" // Enter your MQTT server address or IP.
//#define MQTT_USER "" //enter your MQTT username
//#define MQTT_PASSWORD "" //enter your password
#define MQTT_DEVICE "freezer-alarm" // Enter your MQTT device
#define MQTT_PORT 8883 // Enter your MQTT server port.
#define MQTT_SOCKET_TIMEOUT 120
#define FW_UPDATE_INTERVAL_SEC 24*3600
#define DOOR_UPDATE_INTERVAL_MS 5000
#define UPDATE_SERVER "http://192.168.100.15/firmware/"
#define FIRMWARE_VERSION "-1.15"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_CHEST_POSITION_TOPIC "sensor/freezer-alarm/chest-position"
#define MQTT_UPRIGHT_POSITION_TOPIC "sensor/freezer-alarm/upright-position"
#define MQTT_DISCOVERY_BINARY_SENSOR_PREFIX  "homeassistant/binary_sensor/"
#define MQTT_DISCOVERY_SENSOR_PREFIX  "homeassistant/sensor/"
#define HA_TELEMETRY                         "ha"

#define WATCHDOG_PIN 5       //  D1
#define DOOR_CHEST_PIN 13    // D7
#define DOOR_UPRIGHT_PIN 12  // D6

#include <ESP8266SSDP.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Ticker.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>
#include "credentials.h" // Place credentials for wifi and mqtt in this file
#include "certificates.h" // Place certificates for mqtt in this file
 
Ticker ticker_fw, tickerDoorState;

bool readyForFwUpdate = false;
bool readyForDoorUpdate = false;
bool registered = false;
WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "common.h"
 
void setup() {
 
  Serial.begin(115200);

  pinMode(WATCHDOG_PIN, OUTPUT);
  pinMode(DOOR_CHEST_PIN, INPUT_PULLUP); 
  pinMode(DOOR_UPRIGHT_PIN, INPUT_PULLUP); 

  setup_wifi();

  client.setServer(MQTT_SERVER, MQTT_PORT); //1883 is the port number you have forwared for mqtt messages. You will need to change this if you've used a different port 
  client.setCallback(callback); //callback is the function that gets called for a topic sub

  ticker_fw.attach_ms(FW_UPDATE_INTERVAL_SEC * 1000, fwTicker);
  tickerDoorState.attach_ms(DOOR_UPDATE_INTERVAL_MS, doorStateTickerFunc);

  checkForUpdates();
  resetWatchdog();
}

void loop() {
  
  //If MQTT client can't connect to broker, then reconnect
  if (!client.connected()) {
    reconnect();
  }

  if(readyForDoorUpdate) {
    readyForDoorUpdate = false;
    checkDoorState();
  }


  if(readyForFwUpdate) {
    readyForFwUpdate = false;
    checkForUpdates();
  }

  client.loop(); //the mqtt function that processes MQTT messages
  if (! registered) {
    registerTelemetry();
    updateTelemetry("Unknown");
    registered = true;
  }

}

void callback(char* p_topic, byte* p_payload, unsigned int p_length) {
  String strTopic;
  String payload;

  for (uint8_t i = 0; i < p_length; i++) {
    payload.concat((char)p_payload[i]);
  }

  strTopic = String((char*)p_topic);
  if (strTopic == MQTT_HEARTBEAT_TOPIC) {
    resetWatchdog();
    updateTelemetry(payload);
  }
}

void checkDoorState() {
  //Checks if the door state has changed, and MQTT pub the change
  String chest_position;
  String upright_position;
  
  if (digitalRead(DOOR_CHEST_PIN) == 0) {
    chest_position = "Closed";
  }
  else {
    chest_position = "Open"; 
  }  

  if (digitalRead(DOOR_UPRIGHT_PIN) == 0) {
    upright_position = "Closed";
  }
  else {
    upright_position = "Open"; 
  }  

  client.publish(MQTT_CHEST_POSITION_TOPIC, chest_position.c_str(), true);
  client.publish(MQTT_UPRIGHT_POSITION_TOPIC, upright_position.c_str(), true);
}

void doorStateTickerFunc() {
  readyForDoorUpdate = true;
}

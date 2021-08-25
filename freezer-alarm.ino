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
#define DOOR_UPDATE_INTERVAL_MS 500
#define FIRMWARE_VERSION "-2.01"

/****************************** MQTT TOPICS (change these topics as you wish)  ***************************************/
#define MQTT_HEARTBEAT_SUB "heartbeat/#"
#define MQTT_HEARTBEAT_TOPIC "heartbeat"
#define MQTT_UPDATE_REQUEST "update"
#define CHEST_FREEZER "chest_freezer"
#define CHEST_FREEZER_NAME "Chest Freezer"
#define UPRIGHT_FREEZER "upright_freezer"
#define UPRIGHT_FREEZER_NAME "Upright Freezer"
#define MQTT_DISCOVERY_BINARY_SENSOR_PREFIX  "homeassistant/binary_sensor/"
#define MQTT_DISCOVERY_SENSOR_PREFIX  "homeassistant/sensor/"
#define HA_TELEMETRY                         "ha"

#define WATCHDOG_PIN 5       // D1
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

String chestState = "";
String lastChestState = "";
String uprightState = "";
String lastUprightState = "";

WiFiClientSecure espClient;
PubSubClient client(espClient);

#include "common.h"
 
void setup() {
 
  Serial.begin(115200);

  pinMode(WATCHDOG_PIN, OUTPUT);
  pinMode(DOOR_CHEST_PIN, INPUT_PULLUP); 
  pinMode(DOOR_UPRIGHT_PIN, INPUT_PULLUP); 

  setup_wifi();

  IPAddress result;
  int err = WiFi.hostByName(MQTT_SERVER, result) ;
  if(err == 1){
        Serial.print("MQTT Server IP address: ");
        Serial.println(result);
        MQTTServerIP = result.toString();
  } else {
        Serial.print("Error code: ");
        Serial.println(err);
  }  

  client.setBufferSize(512);  
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
    createBinarySensors(CHEST_FREEZER, CHEST_FREEZER_NAME);
    createBinarySensors(UPRIGHT_FREEZER, UPRIGHT_FREEZER_NAME);
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
    if (payload.equals(String(MQTT_UPDATE_REQUEST))) {
      checkForUpdates();
    }            
  }
}

void checkDoorState() {
  //Checks if the door state has changed, and MQTT pub the change

  chestState = getCurrentState(DOOR_CHEST_PIN);
  if (chestState != lastChestState) {
    lastChestState = chestState;
    if(chestState == "Closed") {
      updateBinarySensor(CHEST_FREEZER, "OFF");
    }
    else {
      updateBinarySensor(CHEST_FREEZER, "ON");
    }
  }

  if (uprightState != lastUprightState) {
    lastUprightState = uprightState;
    if(uprightState == "Closed") {
      updateBinarySensor(UPRIGHT_FREEZER, "OFF");
    }
    else {
      updateBinarySensor(UPRIGHT_FREEZER, "ON"); 
    }
  }

}

String getCurrentState(int pin) {
  String state;
  int val;
  val = digitalRead(pin);
  if(val == LOW) {
    state = "Closed";
  }
  else {
    state = "Open";    
  }
  return state;
}

void doorStateTickerFunc() {
  readyForDoorUpdate = true;
}

void createBinarySensors(String sensor, String sensor_name) {
  String topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + sensor + "/config";
  String message = String("{\"name\": \"") + sensor_name +
                   String("\", \"state_topic\": \"") + String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + sensor +
                   String("/state\", \"device_class\": \"opening\"}");
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(message.c_str());

  client.publish(topic.c_str(), message.c_str(), true);  

}

void updateBinarySensor(String sensor, String state) {
  String topic = String(MQTT_DISCOVERY_BINARY_SENSOR_PREFIX) + sensor + "/state";
  
  Serial.print(F("MQTT - "));
  Serial.print(topic);
  Serial.print(F(" : "));
  Serial.println(state);
  client.publish(topic.c_str(), state.c_str(), true);

}

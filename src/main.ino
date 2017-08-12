#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <PubSubClient.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>

// Constants
// Network Constants
const char* autoconf_ssid  = "ESP8266_SCREEN";    //AP name for WiFi setup AP which your ESP will open when not able to connect to other WiFi
const char* autoconf_pwd   = "1l0v3b4dg3r5";            //AP password so noone else can connect to the ESP in case your router fails
const char* mqtt_server    = "192.168.1.60";  //MQTT Server IP, your home MQTT server eg Mosquitto on RPi, or some public MQTT
const int mqtt_port        = 1883;                  //MQTT Server PORT, default is 1883 but can be anything.
// MQTT Constants
const char* mqtt_devicestatus_set_topic              = "home/room/screen/devicestatus";
const char* mqtt_position_set_topic                  = "home/room/screen/position";
const char* mqtt_move_get_topic                      = "home/room/screen/move";
const char* mqtt_pingall_get_topic                   = "home/room/screen/pingall";

const int screenUp = 4 ;
const int screenDown = 5;
const int motorUp = 13;
const int motorDown = 12;
const int enable = 14;
//const int statLED = 13;

boolean activate = false;

void callback(char* topic,byte* payload, unsigned int length) {

  char c_payload[length];
  memcpy(c_payload, payload, length);
  c_payload[length] = '\0';

  String s_topic = String(topic);
  String s_payload = String(c_payload);

  if ( s_topic == mqtt_move_get_topic ) {

    if (s_payload == "1") {

      if (activate != true) {

        // Turn ON function will set last known brightness

        Serial.print("Switched on");
        activate = true;

      }

    } else if (s_payload == "0") {

      if (activate != false) {

        // Turn OFF function

        Serial.print("Switched off");
        activate = false;


      }

    }

  }
  //handle message arrived
}
unsigned long currentTime = 0;

boolean screenState = false; //false = down true = up

// Global
long lastReconnectAttempt = 0;

WiFiClient espClient;
PubSubClient client(espClient);

void setup_ota() {

  // Set OTA Password, and change it in platformio.ini
  ArduinoOTA.setPassword("ESP8266_PASSWORD");
  ArduinoOTA.onStart([]() {});
  ArduinoOTA.onEnd([]() {});
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {});
  ArduinoOTA.onError([](ota_error_t error) {
    if (error == OTA_AUTH_ERROR);          // Auth failed
    else if (error == OTA_BEGIN_ERROR);    // Begin failed
    else if (error == OTA_CONNECT_ERROR);  // Connect failed
    else if (error == OTA_RECEIVE_ERROR);  // Receive failed
    else if (error == OTA_END_ERROR);      // End failed
  });
  ArduinoOTA.begin();

}

boolean reconnect() {

  // MQTT reconnection function - NON BLOCKING!!

    // Create a random client ID
    String clientId = "ESP8266Client-";
    clientId += String(random(0xffff), HEX);

    // Attempt to connect - and set last will for devicestatus to disconnected
    if (client.connect(clientId.c_str(),mqtt_devicestatus_set_topic,0,false,"disconnected")) {

      // Once connected, publish an announcement...
      client.publish(mqtt_devicestatus_set_topic, "connected");
      Serial.print("Connected");
      // ... and resubscribe
      client.subscribe(mqtt_pingall_get_topic);
      client.subscribe(mqtt_move_get_topic);
    }

    return client.connected();

}
void setup() {
  pinMode(screenUp, INPUT_PULLUP);
  pinMode(screenDown, INPUT_PULLUP);
  pinMode(motorUp, OUTPUT);
  pinMode(motorDown, OUTPUT);
  pinMode(enable, OUTPUT);
  digitalWrite(enable, LOW); //make sure H-Bridge is off
  Serial.begin(115200);
  WiFiManager wifiManager;
  wifiManager.autoConnect(autoconf_ssid,autoconf_pwd);
  setup_ota();
  MDNS.begin("esp-screen");
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);
  lastReconnectAttempt = 0;
}

void loop() {
  if (activate == false && screenState == false) { //check to see if the button is pressed and the gate is closed
    digitalWrite(enable, HIGH); //enable h-bridge
    digitalWrite(motorUp, HIGH); //configure for CW rotation
    digitalWrite(motorDown, LOW);
    while(1){ //run motor until switch is tripped
      if (digitalRead(screenUp) == HIGH) { //check switch state
        screenState = true;
        client.publish(mqtt_position_set_topic , "0");
        digitalWrite(enable, LOW); //disable h-bridge
        digitalWrite(motorUp, LOW); //reset h-bridge config
        break;
      }
    }
  }
  if (activate == true && screenState == true) { //check to see if the button is pressed and the gate is open
    digitalWrite(enable, HIGH);
    digitalWrite(motorUp, LOW); //configure for CCW rotation
    digitalWrite(motorDown, HIGH);
    while(1){
      if (digitalRead(screenDown) == HIGH) {
        screenState = false;
        client.publish(mqtt_position_set_topic , "1");
        digitalWrite(enable, LOW);
        digitalWrite(motorDown, LOW);
        break;
      }
    }
  }
  // KEEP UP MQTT
  if (!client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (reconnect()) {
        lastReconnectAttempt = 0;
      }
    }

  } else {
    // Client connected
    client.loop();
  }

  // KEEP UP OTA
  ArduinoOTA.handle();

}

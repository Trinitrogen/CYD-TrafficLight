#include <WiFi.h>
#include <PubSubClient.h>
#include <TFT_eSPI.h>
#include "time.h"
#include "secrets.h"


// MQTT Broker
const char *mqtt_broker = MQTT_SERVER;
const char *mqtt_username = MQTT_USERNAME;
const char *mqtt_password = MQTT_PASSWORD;
const int mqtt_port = 1883;
const char *topic = "CYDTrafficLight";
long lastReconnectAttempt = 0;

//Time Management
const char* ntpServer = "pool.ntp.org";
const long  gmtOffset_sec = -21600;
const int   daylightOffset_sec = 3600;
char clocktime[12];
int count = 0;

// TFT Setup
int x = 0;
int y = 0;
uint32_t background_color = TFT_BLACK;
uint32_t text_color = TFT_WHITE;


WiFiClient wifi_client;
PubSubClient mqtt_client(wifi_client);
TFT_eSPI tft = TFT_eSPI();

unsigned long update_timer = millis();


void tft_logging(String message) {
  tft.setTextDatum(MC_DATUM);
  x = tft.width() / 2;
  y = y + 10;
  tft.drawString(message.c_str(),x,y,1);
}

void display_wifi(){
// Displays Wifi RSSI in the top corner
  String wifi = "Wifi: ";
  wifi = wifi + WiFi.RSSI();
  Serial.print("Wifi RSSI: ");
  Serial.println(WiFi.RSSI());
  tft.drawString(wifi,30,10,2);
}
void display_time(){
// Displays Time in the bottom center of screen
  update_local_time();
  tft.drawString(clocktime,tft.width()/2,tft.height()-10,2);
}
void display_count(){
// For troubleshooting, this will display the total number
// of MQTT updates
    tft.setTextDatum(MR_DATUM);
    tft.drawNumber(count,tft.width()-10, tft.height()-10,2);
    tft.setTextDatum(MC_DATUM);
}

void error_screen(){
  // If no MQTT update occurs in 10 minutes, set color to Purple
  update_timer = millis();
  x = tft.width() / 2;
  y = tft.height() /2;
  tft.setTextDatum(MC_DATUM);

  background_color = TFT_PURPLE;
  text_color = TFT_WHITE;

  tft.fillScreen(background_color);
  tft.setTextColor(text_color, background_color);
  tft.drawNumber(0,x,y,8);

  display_time();
}

void display_traffic(String message){
// Recieve Drive Time from MQTT as a String, convert
  count++;
  int drive_time = message.toInt();
  Serial.print("display_traffic string recieved (converted to int): ");
  Serial.println(drive_time);

  x = tft.width() / 2;
  y = tft.height() /2;
  tft.setTextDatum(MC_DATUM);

  if(drive_time <= 25){
    background_color = TFT_GREEN;
    text_color = TFT_BLACK;
  }
  else if(drive_time > 25 && drive_time <= 30){
    background_color = TFT_ORANGE;
    text_color = TFT_BLACK; 
  }
  else if(drive_time > 30){
    background_color = TFT_RED;
    text_color = TFT_WHITE; 
  }
  else {
    background_color = TFT_PURPLE;
    text_color = TFT_WHITE;
  }

  tft.fillScreen(background_color);
  tft.setTextColor(text_color, background_color);
  tft.drawNumber(drive_time,x,y,8);
  
  display_time();
  display_wifi();
  //display_count();
}


void update_local_time(){
  struct tm timeinfo;
  if(!getLocalTime(&timeinfo)){
    Serial.println("Failed to obtain time");
  }
  strftime(clocktime,12, "%r", &timeinfo);
  Serial.print("Current Time Stamp: ");
  Serial.println(clocktime);
}

void setup() {

  tft.init();
  tft.setRotation(1); //This is the display in landscape
  tft.fillScreen(background_color);
  tft.setTextColor(text_color, background_color);
  tft.setTextDatum(MC_DATUM);
  
  Serial.begin(115200);
  Serial.print("Connecting to WiFi..");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  String log_message = "Connecting to Wifi: ";
  log_message = String(log_message + WIFI_SSID);
  tft_logging(log_message);
  
  while (WiFi.status() != WL_CONNECTED) {
      delay(500);
      Serial.print(".");
    }
    Serial.print("\nConnected to the Wi-Fi network ");
    log_message = "WIFI Connected";
    tft_logging(log_message);
    Serial.println(WIFI_SSID);
    
    mqtt_client.setServer(mqtt_broker, mqtt_port);
    mqtt_client.setCallback(callback);
    log_message = "Connecting to MQTT Server: ";
    log_message = String(log_message + MQTT_SERVER + ":" + mqtt_port);
    tft_logging(log_message);
    
    while (!mqtt_client.connected()) {
        String mqtt_client_id = "CYDTrafficLight-";
        mqtt_client_id += String(WiFi.macAddress());
        if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.printf("MQTT client %s connectected to %s\n", mqtt_client_id.c_str(), mqtt_broker);
            log_message = "MQTT Connected";
            tft_logging(log_message);
        } else {
            Serial.print("Failed to connect to MQTT Broker with state: ");
            Serial.print(mqtt_client.state());
            delay(2000);
        }
    }
    // Publish and subscribe
    mqtt_client.publish(topic, "CYD TrafficLight Online");
    mqtt_client.subscribe(topic);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);
    update_local_time();
}

void callback(char *topic, byte *payload, unsigned int length) {
    String inString = "";
    Serial.print("Message arrived in topic: ");
    Serial.println(topic);
    Serial.print("Message: ");
    
    for (int i = 0; i < length; i++) {
        Serial.print((char) payload[i]);
        int inChar = payload[i];
        if (isDigit(inChar)) {
          // convert the incoming byte to a char and add it to the string:
          inString += (char)inChar;
        }
    }
    Serial.println("");
    Serial.print("Extracted String: ");
    Serial.println(inString);
    display_traffic(inString);
    Serial.print("Resetting Update Timer from: ");
    Serial.println(update_timer);
    update_timer = millis();
    Serial.println();
    Serial.println("-----------------------");
}

boolean mqtt_reconnect() {
    while (!mqtt_client.connected()) {
        String mqtt_client_id = "CYDTrafficLight-";
        mqtt_client_id += String(WiFi.macAddress());
        if (mqtt_client.connect(mqtt_client_id.c_str(), mqtt_username, mqtt_password)) {
            Serial.printf("MQTT client %s connectected to %s\n", mqtt_client_id.c_str(), mqtt_broker);
        } else {
            Serial.print("Failed to connect to MQTT Broker with state: ");
            Serial.print(mqtt_client.state());
            delay(2000);
        }
    }
    mqtt_client.publish(topic, "CYD TrafficLight Reconnected");
    mqtt_client.subscribe(topic);
    return mqtt_client.connected();
}

void loop() {
  unsigned long last_update = (millis() - update_timer);
  if (last_update > 600000) {
    Serial.print("ERROR: Last Update Was ");
    Serial.print((last_update / 1000)/60);
    Serial.println(" minutes ago");
    error_screen();
  }
  if (!mqtt_client.connected()) {
    long now = millis();
    if (now - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = now;
      // Attempt to reconnect
      if (mqtt_reconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } 
  else {
    mqtt_client.loop();
  }
}

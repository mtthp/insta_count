/** Instagram Followers Counter
 *
 * Displays your current subscriber count on a seven-segment display
 * this version supports up to 9999 followers
 * based on YouTube Subscriber Counter code by Becky Stern (http://www.instructables.com/member/bekathwia/)
 * at http://www.instructables.com/id/YouTube-Subscriber-Counter-With-ESP8266/
 *
 * @author Matthieu Petit <p.matthieu@me.com>
 * @date 05/05/2017
 */

// requires the following libraries, search in Library Manager or download from github :
#include <ESP8266HTTPClient.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>          //ESP8266 Core WiFi Library
#include <TM1637Display.h>        //https://github.com/avishorp/TM1637 TM1637 (LED Driver)

// user configuration file
#include "user_config.h"

#define SERIAL_RATE 115200

#define CLK D6 //Set the CLK pin connection to the TM1637 display
#define DIO D5 //Set the DIO pin connection to the TM1637 display

//      SEG_A
// SEG_F      SEG_B
//      SEG_G
// SEG_E      SEG_C
//      SEG_D
// booting
const uint8_t SEG_boot[] = {
  SEG_C | SEG_D | SEG_E | SEG_F | SEG_G,          // b
  SEG_C | SEG_D | SEG_E | SEG_G,                  // o
  SEG_C | SEG_D | SEG_E | SEG_G,                  // o
  SEG_D | SEG_E | SEG_F | SEG_G,                  // t
};

// connected to WiFi
const uint8_t SEG_conn[] = {
  SEG_D | SEG_E | SEG_G,                          // c
  SEG_C | SEG_D | SEG_E | SEG_G,                  // o
  SEG_C | SEG_E | SEG_G,                          // n
  SEG_C | SEG_E | SEG_G,                          // n
};

// HTTP Error
const uint8_t SEG_Err[] = {
  SEG_A | SEG_F | SEG_E | SEG_D | SEG_G,          // E
  SEG_E | SEG_G,                                  // r
  SEG_E | SEG_G,                                  // r
  0b00000000,
};

HTTPClient http;

TM1637Display display(CLK, DIO); //set up the 4-Digit Display

WiFiClient wifiClient;
PubSubClient mqttClient(MQTT_SERVER, MQTT_PORT, wifiClient);

DHT dht(DHTPIN, DHTTYPE);

unsigned long apiCallDelay = REPORT_INTERVAL * 1000; // delay in milliseconds between each api call 
unsigned long apiCallTime;   // last time api request has been done

const byte interruptPin = 0; // used to start the loading function

void setup() {
  Serial.begin(SERIAL_RATE);
  Serial.println("\nBooting...");

  display.setBrightness(0x0a); //set the diplay to maximum brightness
  display.setSegments(SEG_boot);

  wifiManager.setAPCallback(configModeCallback);
  wifiManager.autoConnect("InstaCount"); // AP-Name in case of not finding previous WiFi network

  //if you get here you have connected to the WiFi
  Serial.println("[WiFi] Connected !");
  display.setSegments(SEG_conn);

  // attach the loading function when starting HTTP calls
  pinMode(interruptPin, OUTPUT);
  attachInterrupt(interruptPin, loading, RISING);  

  ask_api();
}

void loop() {
  if (millis() > api_call_time + api_call_delay)  {
    api_call_time = millis();

    /* Disclaimer :
     * we could use this url (https://www.instagram.com/{username}/?__a=1) but then the StaticJsonBuffer 
     * would be too large to be handled by the Wemos D1 Mini, so we ask the API instead
     * If you have a device with enough memory, you can try the ask_user_profile() method instead
     */
    ask_api();
  }
}


void ask_api() {
  // Memory pool for JSON object tree.
  //
  // Inside the brackets, 200 is the size of the pool in bytes,
  // If the JSON object is more complex, you need to increase that value.
  StaticJsonBuffer<500> jsonBuffer;

  String url = "https://api.instagram.com/v1/users/";
  url.concat(USER_ID);
  url.concat("?access_token=");
  url.concat(API_ACCESS_TOKEN);

  Serial.print("[HTTPS] begin...\n");
  http.begin(url, INSTAGRAM_FINGERPRINT); //HTTPS
  
  Serial.print("[HTTPS] GET " + url + " ...\n");
  
  // start connection and send HTTP header
  int httpCode = http.GET();
  digitalWrite(interruptPin, HIGH);
  digitalWrite(interruptPin, LOW);
  // httpCode will be negative on error
  if (httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String result = http.getString();
      int size = result.length() + 1;
      char json[size];
      result.toCharArray(json, size);

      Serial.println("---------Data received---------");
      Serial.println(json);
      Serial.println("-------------------------------");
      // Root of the object tree.
      //
      // It's a reference to the JsonObject, the actual bytes are inside the
      // JsonBuffer with all the other nodes of the object tree.
      // Memory is freed when jsonBuffer goes out of scope.
      JsonObject& root = jsonBuffer.parseObject(json);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed, probably because the json buffer isnt enough");
        display.setSegments(SEG_Err);
      } else {
        long followed_by = root["data"]["counts"]["followed_by"];
        Serial.print("Number of followers is : ");
        Serial.println(followed_by);

        float followed_by_float = followed_by / 10.0; // rounding decimal
        display.showNumberDecEx(followed_by_float, 0x80 >> 1); //Display the numCounter value;
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      display.setSegments(SEG_Err);
    }
  } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      display.setSegments(SEG_Err);
  }

  
  http.end();
}


void ask_user_profile(){
  // Memory pool for JSON object tree.
  //
  // Inside the brackets, 200 is the size of the pool in bytes,
  // If the JSON object is more complex, you need to increase that value.
  StaticJsonBuffer<20000> jsonBuffer;
  String url = "https://www.instagram.com/";
  url.concat(USER_NAME);
  url.concat("/?__a=1");

  Serial.print("[HTTPS] begin...\n");
  http.begin(url, INSTAGRAM_FINGERPRINT); //HTTPS
  digitalWrite(interruptPin, HIGH);
  Serial.print("[HTTPS] GET " + url + " ...\n");
  // start connection and send HTTP header
  int httpCode = http.GET();
  digitalWrite(interruptPin, LOW);

  // httpCode will be negative on error
  if(httpCode > 0) {
    // HTTP header has been send and Server response header has been handled
    Serial.printf("[HTTPS] GET... code: %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String result = http.getString();
      int size = result.length() + 1;
      char json[size];
      result.toCharArray(json, size);

      Serial.println("---------Data received---------");
      Serial.println(json);
      Serial.println("-------------------------------");
      // Root of the object tree.
      //
      // It's a reference to the JsonObject, the actual bytes are inside the
      // JsonBuffer with all the other nodes of the object tree.
      // Memory is freed when jsonBuffer goes out of scope.
      JsonObject& root = jsonBuffer.parseObject(json);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("parseObject() failed, probably because the json buffer isnt enough");
      } else {
        long followed_by = root["user"]["followed_by"]["count"];
        Serial.print("Number of followers is : ");
        Serial.println(followed_by);

        float followed_by_float = followed_by / 10.0; // rounding decimal
        display.showNumberDecEx(followed_by_float, 0x80 >> 1); //Display the numCounter value;
      }
    } else {
      Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
      display.setSegments(SEG_Err);
    }
  } else {
    Serial.printf("[HTTPS] GET... failed, error: %s\n", http.errorToString(httpCode).c_str());
    display.setSegments(SEG_Err);
  }

  http.end();
}

// waiting for WiFi configuration
const uint8_t SEG_conf[] = {
  SEG_D | SEG_E | SEG_G,                          // c
  SEG_C | SEG_D | SEG_E | SEG_G,                  // o
  SEG_C | SEG_E | SEG_G,                          // n
  SEG_A | SEG_E | SEG_F | SEG_G,                  // f
};
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  display.setSegments(SEG_conf);

  Serial.println(WiFi.softAPIP());
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

void loading(){
  uint8_t path[][4] {
    { SEG_A, 0b00000000, 0b00000000, 0b00000000 },
    { 0b00000000, SEG_A, 0b00000000, 0b00000000 },
    { 0b00000000, 0b00000000, SEG_A, 0b00000000 },
    { 0b00000000, 0b00000000, 0b00000000, SEG_A },
    { 0b00000000, 0b00000000, 0b00000000, SEG_B },
    { 0b00000000, 0b00000000, 0b00000000, SEG_C },
    { 0b00000000, 0b00000000, 0b00000000, SEG_D },
    { 0b00000000, 0b00000000, SEG_D, 0b00000000 },
    { 0b00000000, SEG_D, 0b00000000, 0b00000000 },
    { SEG_D, 0b00000000, 0b00000000, 0b00000000 },
    { SEG_E, 0b00000000, 0b00000000, 0b00000000 },
    { SEG_F, 0b00000000, 0b00000000, 0b00000000 },
    { 0b00000000, 0b00000000, 0b00000000, 0b00000000 },
  };

  int length = sizeof(path) / sizeof(path[0]);
  for (int i = 0; i < length; i++){
    display.setSegments(path[i]);
    delay(100);
  }
}


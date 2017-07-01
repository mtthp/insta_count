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

#include <PubSubClient.h>         //MQTT support
#include <DHT.h>                  //to get Temperature and Humidity values from DHT22 sensor
#include <DHT_U.h>

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

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.printf("[WiFi] Connecting to %s", WIFI_SSID);
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print("...");
    loading();
  }
  Serial.println();

  //if you get here you have connected to the WiFi
  Serial.println("[WiFi] Connected !");
  display.setSegments(SEG_conn);
  delay(1000);

  while (!mqttClient.connected()) {
    Serial.printf("[MQTT] Attempting MQTT connection to %s...\n", MQTT_SERVER);
    // Attempt to connect
    // If you do not want to use a username and password, change next line to
    // if (mqttClient.connect("ESP8266Client")) {
    if (mqttClient.connect("ESP8266Client", MQTT_USER, MQTT_PASSWORD)) {
      Serial.println("[MQTT] Connected !");
    } else {
      Serial.print("[MQTT] failed, rc=");
      Serial.print(mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }

  // attach the loading function when starting HTTP calls
  pinMode(interruptPin, OUTPUT);
  attachInterrupt(interruptPin, loading, RISING);  

  act();
}

void loop() {
  if (millis() > apiCallTime + apiCallDelay)  {
    apiCallTime = millis();

    act();
  }
}

void act(){
  ask_api();
  readDHT();
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
    Serial.printf("[HTTPS] GET... code : %d\n", httpCode);

    // file found at server
    if(httpCode == HTTP_CODE_OK) {
      String result = http.getString();
      int size = result.length() + 1;
      char json[size];
      result.toCharArray(json, size);

      Serial.printf("[HTTPS] GET... result :  %s\n", json);
      // Root of the object tree.
      //
      // It's a reference to the JsonObject, the actual bytes are inside the
      // JsonBuffer with all the other nodes of the object tree.
      // Memory is freed when jsonBuffer goes out of scope.
      JsonObject& root = jsonBuffer.parseObject(json);

      // Test if parsing succeeds.
      if (!root.success()) {
        Serial.println("[ArduinoJSON] parseObject() failed, probably because the json buffer isnt enough");
        display.setSegments(SEG_Err);
      } else {
        long followed_by = root["data"]["counts"]["followed_by"];
        Serial.printf("[Instagram] Number of followers is : %d\n", followed_by);

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

void readDHT(){
  // Memory pool for JSON object tree.
  //
  // Inside the brackets, 200 is the size of the pool in bytes,
  // If the JSON object is more complex, you need to increase that value.
  StaticJsonBuffer<500> jsonBuffer;
  
  // Wait a few seconds between measurements.
  delay(2000);

  // Reading temperature or humidity takes about 250 milliseconds!
  // Sensor readings may also be up to 2 seconds 'old' (its a very slow sensor)
  float h = dht.readHumidity();
  // Read temperature as Celsius (the default)
  float t = dht.readTemperature();

  // Check if any reads failed and exit early (to try again).
  if (isnan(h) || isnan(t)) {
    Serial.println("[DHT22] Failed to read from DHT sensor!");
    return;
  }

  JsonObject& jsonValues = jsonBuffer.createObject();
  jsonValues["Humidity"] = h;
  jsonValues["Temperature"] = t;

  char charBuffer[50];
  jsonValues.printTo(charBuffer, sizeof(charBuffer));
  mqttClient.publish(MQTT_TOPIC, charBuffer);
  Serial.printf("[MQTT] Sending to topic %s : %s\n", MQTT_TOPIC, charBuffer);
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


/** 
 * user_config.h - user specific configuration for Instagram Followers Counter
 *
 * @author Matthieu Petit <p.matthieu@me.com>
 * @date 05/05/2017
 */

// Instagram Fingerprint (to use HTTPS)
#define INSTAGRAM_FINGERPRINT "5E:30:4C:7A:C6:91:69:CF:E0:05:79:5F:26:4D:80:C7:BF:F5:F5:4D"

// Instagram API key
// get yours: http://dmolsen.com/2013/04/05/generating-access-tokens-for-instagram/
#define API_ACCESS_TOKEN "your_access_token_here"

// Instagram User ID
// find yours: https://www.instagram.com/{username}/?__a=1
#define USER_ID "your_user_id_here"

// WiFi parameters
#define WIFI_SSID     "your_wifi_ssid"
#define WIFI_PASSWORD "your_wifi_password"

// MQTT parameters
#define MQTT_SERVER   "your_mqtt_host"
#define MQTT_PORT     1883
#define MQTT_USER     "your_mqtt_user"
#define MQTT_PASSWORD "your_mqtt_password"
#define MQTT_TOPIC    "your_topic/SENSOR"

// DHT parameters (temperature + humidity sensor)
#define DHTPIN D4
#define DHTTYPE DHT22

// number of seconds between each call
#define REPORT_INTERVAL 300

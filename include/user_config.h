#ifndef CONFIG_H
#define CONFIG_H

// WiFi Configuration
#define WIFI_SSID       "iot"
#define WIFI_PASSWORD   "goninjago"

// MQTT Configuration
#define MQTT_BROKER_URL      "mqtt://192.168.0.21:1883"
#define MQTT_USERNAME        "mqtt"
#define MQTT_PASSWORD        "kalle2222"
#define MQTT_TOPIC_BASE      "weather"
#define MQTT_TOPIC_SUBSCRIBE "weather/#"
#define MQTT_TOPIC_BASE_DEVICE MQTT_TOPIC_BASE
#define MQTT_QOS             1

// HomeAssistant MQTT Discovery Configuration
#define HA_DISCOVERY_PREFIX  "homeassistant"
#define HA_DEVICE_ID         "cydintosh"

// Weather display units (Mac Roman encoding, use hex escapes for special chars)
// ° = \xA1, use string concatenation: "\xA1" "C" means "°C"
// Examples: "\xA1" "C" = °C, "\xA1" "F" = °F, "K" = Kelvin
// NOTE: These are display labels; values are not converted.
#define WEATHER_TEMP_UNIT \
    "\xA1"                \
    "C"
#define WEATHER_WIND_UNIT "km/h"

#endif

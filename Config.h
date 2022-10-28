#ifndef _CONFIG
#define _CONFIG

#define SERVO_PIN D0
#define SENSOR_PIN A0
#define LIGHTLEVEL 60
#define ANALOGDELAY 250
#define DISPLAY_DIO D3
#define DISPLAY_CLK D5
#define BUTTON_PIN D1
#define DNSNAME "lupercalia"
#define MQTT_SERVER "pi4"
#define MQTT_PORT 1883
#define MQTT_CHANNEL_PUB "home/" DNSNAME "/state"
#define MQTT_CHANNEL_SUB "home/" DNSNAME "/#"
#define MQTT_CHANNEL_LOG "home/" DNSNAME "/log"
#define MQTT_USER "clockuser"
#define MQTT_PASSWORD "clockuser"
#define UPDATE_URL "http://pi4/cgi-bin/test.rb"
#define MQTT_MAX_PACKET_SIZE 20000

const char* ssids[] = {"Wifi", "Goes"};
const char* passs[] = {"In", "Here"};
const int wifiCount = 2;

#endif

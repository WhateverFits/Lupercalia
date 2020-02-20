#ifndef LupercaliaHeader
#define LupercaliaHeader
void mqttLog(const char* status);
void mqttPublish(const char* status);
void mqttCallback(char* topic, byte* payload, unsigned int length);
void outputString(String msg, bool showNow);
ESP8266WiFiMulti wifiMulti;

long lastTime = 0;
long lastTimeClock = 0;
long lastTimeAnalog = 0;
time_t utc, local;
int pos = 90;
int increment = 1;
long lastWiggle = 0;
bool connectedOnce = false;
bool messageRead = true;
bool hasMessage = false;

WiFiClient mqttWiFiClient;
String mqttClientId; 
long lastReconnectAttempt = 0; 

PubSubClient mqttClient(MQTT_SERVER, MQTT_PORT, mqttCallback, mqttWiFiClient);
DynamicJsonDocument mqttDoc(1024);

SH1106Wire display(0x3c, DISPLAY_DIO, DISPLAY_CLK);

Servo wiggler;

#endif

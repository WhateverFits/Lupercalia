#include "Config.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Servo.h>
#include <Time.h>
#include "SH1106Wire.h"
#include "OLEDDisplayUi.h"
#include "Lupercalia.h"

void onPressed() {
}

void onPressedForDuration() {
    Serial.println("Kill it");
}

static bool wiggling = false;
void startWiggle() {
  wiggling = true;
  messageRead = false;
  hasMessage = true;
}

void stopWiggle() {
  wiggling = false;
  pos = 90;
  wiggler.write(pos);
}

void doWiggle(long milliseconds) {
  if (wiggling) {
    if (milliseconds >= lastWiggle + 30 || milliseconds < lastWiggle) {
      wiggler.write(pos);
      if (pos == 75 || pos == 105) {
        increment *= -1;
      }

      pos += increment;
      lastWiggle = milliseconds;
    }
  }
}

void processLightSensor(long milliseconds) {
  // Handle the light sensor
  if (milliseconds >= lastTimeAnalog + ANALOGDELAY || milliseconds < lastTimeAnalog) {
    int lightValue = analogRead(SENSOR_PIN);
    Serial.printf("Light level: %d\n", lightValue);
    if (!messageRead && lightValue > LIGHTLEVEL) {
      Serial.println("I saw the light!");
      stopWiggle();
      display.display();
      messageRead = true;
    }

    if (hasMessage && messageRead && lightValue < LIGHTLEVEL) {
      Serial.println("Darkness, enveloping me.");
      outputString("", true);
      hasMessage = false;
    }

    lastTimeAnalog = milliseconds;
  }
}

void validateWiFi(long milliseconds) {
  // Update WiFi status. Take care of rollover
  if (milliseconds >= lastTimeClock + 1000 || milliseconds < lastTimeClock) {
    if (wifiMulti.run() != WL_CONNECTED) {
      Serial.println("Disconnected");
      outputString("Connecting", true);
      connectedOnce = false;
    } else {
      if (!connectedOnce) {
        Serial.print("Connected late to ");
        Serial.println(WiFi.SSID());
        outputString("", true);
      }
      connectedOnce = true;
    }

    lastTimeClock = milliseconds;
  }
}

void validateMqtt(long milliseconds) {
  if (!mqttClient.connected()) {
    if (milliseconds - lastReconnectAttempt > 5000) {
      lastReconnectAttempt = milliseconds;
      // Attempt to reconnect
      if (mqttReconnect()) {
        lastReconnectAttempt = 0;
      }
    }
  } else {
    mqttClient.loop();
  }
}

void drawPicture(String data, bool showNow = true) {
  Serial.println("Drawing picture");
  display.clear();
  for (int y = 0; y < 64; y++) {
    for (int x = 0; x < 128; x++) {
      if (data[x + y * 128] == '1') {
        display.setPixel(x,y);
      }
    }
  }
  
  if (showNow) {
    display.display();
  }
}

const String hexStringToBinString(String value) {
  String returnValue = "";
  for (int i = 0; i < value.length(); i++) {
    returnValue += hex_char_to_bin(value[i]);
  }
  return returnValue;
}

const String hex_char_to_bin(char c)
{
    // TODO handle default / error
    switch(toupper(c))
    {
        case '0': return "0000";
        case '1': return "0001";
        case '2': return "0010";
        case '3': return "0011";
        case '4': return "0100";
        case '5': return "0101";
        case '6': return "0110";
        case '7': return "0111";
        case '8': return "1000";
        case '9': return "1001";
        case 'A': return "1010";
        case 'B': return "1011";
        case 'C': return "1100";
        case 'D': return "1101";
        case 'E': return "1110";
        case 'F': return "1111";
        default: return "0000";
    }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Inside mqtt callback %d\n", length);
  String message = (char*)payload;
  Serial.println(message.length());
  Serial.println(message.substring(0, length));
  StaticJsonDocument<1000> doc;
  DeserializationError err= deserializeJson(doc, message.substring(0, length));
  String action = doc["Action"];
  Serial.print("Action: ");
  Serial.println(action);
  if (action == "Message") {
    outputString(doc["Value"], false);
    startWiggle();
  } else if (action == "Clear") {
    outputString("", true);
    stopWiggle();
  } else if (action == "Image") {
    Serial.print("Got image length: ");
    String imageValue = String((const char *)doc["Value"]);
    Serial.println(imageValue.length());
    if (imageValue.length() == (128*64/16)) {
      drawPicture(hexStringToBinString(imageValue), false);
      startWiggle();
    } else {
      outputString("Bad Image", true);
    }
  }
  if (action == "Update") {
    WiFiClient updateWiFiClient;
    t_httpUpdate_return ret = ESPhttpUpdate.update(updateWiFiClient, UPDATE_URL);
    switch (ret) {
      case HTTP_UPDATE_FAILED:
        Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
        break;

      case HTTP_UPDATE_NO_UPDATES:
        Serial.println("HTTP_UPDATE_NO_UPDATES");
        break;

      case HTTP_UPDATE_OK:
        Serial.println("HTTP_UPDATE_OK");
        break;
    }
  }
}

void mqttPublish(const char* status) {
  if (mqttClient.connected()) {
    mqttDoc["Status"] = status;
    char buffer[512];
    size_t n = serializeJson(mqttDoc, buffer);
    mqttClient.publish(MQTT_CHANNEL_PUB, buffer, true);
  }
}

boolean mqttReconnect() {
  char buf[100];
  mqttClientId.toCharArray(buf, 100);
  if (mqttClient.connect(buf, MQTT_USER, MQTT_PASSWORD)) {
    mqttClient.subscribe(MQTT_CHANNEL_SUB);
  }

  return mqttClient.connected();
}

String generateMqttClientId() {
  char buffer[4];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(buffer, "%02x%02x", macAddr[4], macAddr[5]);
  return "Lupercalia" + String(buffer);
}

void update_started() {
  Serial.println("CALLBACK:  HTTP update process started");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth()/2, display.getHeight()/2 - 10, "OTA Update");
  display.display();
}

void update_finished() {
  Serial.println("CALLBACK:  HTTP update process finished");
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth()/2, display.getHeight()/2, "Restart");
  display.display();
}

void update_progress(int progress, int total) {
  Serial.printf("CALLBACK:  HTTP update process at %d of %d bytes...\n", progress, total);
   display.drawProgressBar(4, 32, 120, 8, progress / (total / 100) );
   display.display();
}

void update_error(int err) {
  Serial.printf("CALLBACK:  HTTP update fatal error code %d\n", err);
  display.clear();
  display.setFont(ArialMT_Plain_10);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth()/2, display.getHeight()/2 - 10, "OTA error:" + String(err));
  display.display();
}

void outputString(String msg, bool showNow) {
  display.clear();
  display.setFont(ArialMT_Plain_10);
  //display.setFont(ArialMT_Plain_24);
  display.setTextAlignment(TEXT_ALIGN_CENTER_BOTH);
  display.drawString(display.getWidth()/2, display.getHeight()/2, msg);
  if (showNow) {
    display.display();
  }
}

int findWiFi() {
  // Connect to a WiFi network
  Serial.println("Scanning networks");
  int n = WiFi.scanNetworks();
  int found = -1;
  if (n == 0) {
    Serial.println("no networks found");
    outputString("No Net", true);
  }
  else
  {
    Serial.print(n);
    Serial.println(" networks found");
    for (int stored = 0; stored < wifiCount && found == -1; stored++) {
      for (int i = 0; i < n; ++i)
      {
        if (WiFi.SSID(i) == ssids[stored])
        {
          found = stored;
          Serial.print("Connecting to ");
          Serial.println(ssids[found]);
          outputString("Connecting", true);
          break;
        }
      }
    }
  }

  return found;
}

void connectWiFi(int found) {
  //Serial.print("Connecting to ");
  //Serial.println(ssids[found]);
  #ifdef DNSNAME
  WiFi.hostname(DNSNAME);
  #else
  char buffer[4];
  uint8_t macAddr[6];
  WiFi.macAddress(macAddr);
  sprintf(buffer, "%02x%02x", macAddr[4], macAddr[5]);
  WiFi.hostname("SunriseLight" + String(buffer));
  #endif
  WiFi.begin(ssids[found], passs[found]);

  bool first = true;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    if (first)
      outputString("O", true);
    else
      outputString("o", true);
    first != first;
  }

  outputString(String("Connected ") + ssids[found], true);

  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("Netmask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway: ");
  Serial.println(WiFi.gatewayIP());
}

void setupWiFi(){
  int found = -1;
  bool first = true;
  while (found == -1) {
    if (first)
      outputString("X", true);
    else
      outputString("x", true);
    first != first;
    found = findWiFi();
    if (found == -1) {
      delay(5000);
    }
  }

  connectWiFi(found);
}

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println();
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

  //setupWiFi();
  for (int i=0; i < wifiCount; i++) {
    wifiMulti.addAP(ssids[i], passs[i]);
  }

  Serial.println("Connecting");
  if (wifiMulti.run() == WL_CONNECTED) {
    Serial.print("Connected: ");
    Serial.println(WiFi.localIP());
    outputString("Connected", true);
    connectedOnce = true;
  }


  mqttClientId = generateMqttClientId();

  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);
  ESPhttpUpdate.onStart(update_started);
  ESPhttpUpdate.onEnd(update_finished);
  ESPhttpUpdate.onProgress(update_progress);
  ESPhttpUpdate.onError(update_error);
  wiggler.attach(SERVO_PIN);
  display.clear();
  display.display();
}

void loop() {
  // Get the time at the start of this loop
  long milliseconds = millis();

  doWiggle(milliseconds);
  processLightSensor(milliseconds);
  validateWiFi(milliseconds);
  validateMqtt(milliseconds);
}

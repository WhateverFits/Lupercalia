#include "Config.h"
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ESP8266httpUpdate.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Servo.h>
#include <TimeLib.h>
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

void doWiggle(unsigned long milliseconds) {
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

void processLightSensor(unsigned long milliseconds) {
  // Handle the light sensor
  if (milliseconds >= lastTimeAnalog + ANALOGDELAY || milliseconds < lastTimeAnalog) {
    int lightValue = analogRead(SENSOR_PIN);
    //Serial.printf("Light level: %d\n", lightValue);
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

bool validateWiFi(unsigned long milliseconds) {
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

  return connectedOnce;
}

void validateMqtt(unsigned long milliseconds) {
  if (!mqttClient.connected()) {
    if (milliseconds - lastReconnectAttempt > 5000 || lastReconnectAttempt == 0 || milliseconds < lastReconnectAttempt) {
      Serial.println("MQTT not connected");
      lastReconnectAttempt = milliseconds;
      Serial.println("MQTT reconnecting");
      // Attempt to reconnect
      if (mqttReconnect()) {
        Serial.println("MQTT reconnected");
      }
    }

    if (milliseconds - lastReconnectAttempt > 60000) {
      Serial.println("MQTT disconnecting WiFi");
      WiFi.disconnect();
      delay(500);
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

const String hexStringToBinString(String value, unsigned int length) {
  String returnValue = "";
  for (int i = 0; i < length; i++) {
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
  Serial.printf("Inside mqtt callback: %s\n", topic);
  Serial.println(length);

  String topicString = (char*)topic;
  topicString = topicString.substring(topicString.lastIndexOf('/')+1);

  Serial.print("Action: ");
  Serial.println(topicString);

  if (topicString == "message") {
    String message = (char*)payload;
    message = message.substring(0, length);
    Serial.println(message);
    outputString(message.c_str(), false);
    startWiggle();
  } else if (topicString == "clear") {
    outputString("", true);
    stopWiggle();
  } else if (topicString == "image") {
    if (length == (128*64/16)) {
      drawPicture(hexStringToBinString((char *)payload, length), false);
      startWiggle();
    } else {
      outputString("Bad Image", true);
    }
  }
  if (topicString == "update") {
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

void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println();
  Serial.println();
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);

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
  unsigned long milliseconds = millis();

  doWiggle(milliseconds);
  processLightSensor(milliseconds);
  if (validateWiFi(milliseconds))
    validateMqtt(milliseconds);
}

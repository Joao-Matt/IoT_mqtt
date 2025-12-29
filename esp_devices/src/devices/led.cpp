// LED device: listens for RGB commands on the LED topic and drives PWM pins.
// It supports common-anode inversion via LED_COMMON_ANODE and PWM config via LEDC_*.
// LED_TOPIC expects payloads like "r,g,b" or "nan" to blink blue.
// Status is published to esp/<client_id>/status on reconnect.
// State variables track blink timing and subscription status.
// This file is selected only in the esp32_LED environment.
#include "device.h"
#include "common.h"

#ifndef LED_PIN_R
#define LED_PIN_R 25
#endif

#ifndef LED_PIN_G
#define LED_PIN_G 26
#endif

#ifndef LED_PIN_B
#define LED_PIN_B 27
#endif

#ifndef LEDC_FREQ
#define LEDC_FREQ 5000
#endif

#ifndef LEDC_RES_BITS
#define LEDC_RES_BITS 8
#endif

#ifndef LED_COMMON_ANODE
#define LED_COMMON_ANODE 0
#endif

namespace {
const char LED_TOPIC[] = "LED";
const int LEDC_CH_R = 0;
const int LEDC_CH_G = 1;
const int LEDC_CH_B = 2;

char topicStatus[64];
bool ledNanMode = false;
bool ledBlinkOn = false;
bool statusPublished = false;
bool ledSubscribed = false;
uint32_t lastLedBlinkMs = 0;

void setupTopics()
{
  snprintf(topicStatus, sizeof(topicStatus), "esp/%s/status", MQTT_CLIENT_ID);
}

int clampByte(int value)
{
  if (value < 0) return 0;
  if (value > 255) return 255;
  return value;
}

void setRgb(int r, int g, int b)
{
  r = clampByte(r);
  g = clampByte(g);
  b = clampByte(b);

#if LED_COMMON_ANODE
  r = 255 - r;
  g = 255 - g;
  b = 255 - b;
#endif

  ledcWrite(LEDC_CH_R, r);
  ledcWrite(LEDC_CH_G, g);
  ledcWrite(LEDC_CH_B, b);
}

void setLedNanMode(bool enabled)
{
  ledNanMode = enabled;
  if (!enabled) {
    ledBlinkOn = false;
  }
}

void handleLedBlink(uint32_t now)
{
  if (!ledNanMode) return;

  if (now - lastLedBlinkMs >= 500) {
    lastLedBlinkMs = now;
    ledBlinkOn = !ledBlinkOn;
    if (ledBlinkOn) {
      setRgb(0, 0, 255);
    } else {
      setRgb(0, 0, 0);
    }
  }
}

void ensureMqttConnected()
{
  if (!mqttClient.connected()) {
    if (!connectMqttTimed()) return;
    statusPublished = false;
    ledSubscribed = false;
  }

  if (!statusPublished) {
    mqttClient.publish(topicStatus, "online", true);
    statusPublished = true;
  }

  if (!ledSubscribed) {
    mqttClient.subscribe(LED_TOPIC);
    ledSubscribed = true;
  }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, LED_TOPIC) != 0) return;

  char buf[32];
  size_t n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  const char *text = buf;
  while (*text && isspace((unsigned char)*text)) {
    text++;
  }

  if (payloadIsNan(text)) {
    setLedNanMode(true);
    return;
  }

  int r = 0;
  int g = 0;
  int b = 0;
  if (sscanf(text, "%d,%d,%d", &r, &g, &b) == 3 ||
      sscanf(text, "%d , %d , %d", &r, &g, &b) == 3) {
    setLedNanMode(false);
    setRgb(r, g, b);
  } else {
    setLedNanMode(true);
  }
}
} // namespace

void deviceSetup()
{
  Serial.begin(115200);
  delay(50);

  ledcSetup(LEDC_CH_R, LEDC_FREQ, LEDC_RES_BITS);
  ledcSetup(LEDC_CH_G, LEDC_FREQ, LEDC_RES_BITS);
  ledcSetup(LEDC_CH_B, LEDC_FREQ, LEDC_RES_BITS);
  ledcAttachPin(LED_PIN_R, LEDC_CH_R);
  ledcAttachPin(LED_PIN_G, LEDC_CH_G);
  ledcAttachPin(LED_PIN_B, LEDC_CH_B);
  setRgb(0, 0, 0);

  setupTopics();
  setupMqttServer();
  mqttClient.setCallback(onMqttMessage);

  Serial.printf("\nBoot: ESP32 MQTT station: %s (LED)\n", MQTT_CLIENT_ID);

  bool w = connectWifiTimed();
  Serial.printf("WiFi: %s\n", w ? "connected" : "not connected");
  if (w) {
    ensureMqttConnected();
    Serial.printf("MQTT: %s\n", mqttClient.connected() ? "connected" : "not connected");
  }
}

void deviceLoop()
{
  uint32_t now = millis();

  static uint32_t lastConnTry = 0;
  if (now - lastConnTry > 1000) {
    lastConnTry = now;
    if (WiFi.status() != WL_CONNECTED) connectWifiTimed();
    if (WiFi.status() == WL_CONNECTED) ensureMqttConnected();
  }

  if (mqttClient.connected()) mqttClient.loop();
  handleLedBlink(now);
}

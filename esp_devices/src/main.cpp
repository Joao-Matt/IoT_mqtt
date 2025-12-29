#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ctype.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

#ifndef HAS_DHT
#define HAS_DHT 0
#endif

#ifndef HAS_HCSR04
#define HAS_HCSR04 0
#endif

#ifndef HAS_LED
#define HAS_LED 0
#endif

#if HAS_DHT
#include <DHT.h>
#endif

// -------- WiFi / MQTT --------
#ifndef WIFI_SSID
#define WIFI_SSID "YOUR_WIFI_SSID"
#endif

#ifndef WIFI_PASS
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#endif

#ifndef MQTT_HOST
#define MQTT_HOST "192.168.1.174"
#endif

#ifndef MQTT_PORT
#define MQTT_PORT 1883
#endif

#ifndef MQTT_CLIENT_ID
#define MQTT_CLIENT_ID "esp32_station"
#endif

#ifndef WIFI_CONN_TIMEOUT_MS
#define WIFI_CONN_TIMEOUT_MS 10000
#endif

#ifndef MQTT_CONN_TIMEOUT_MS
#define MQTT_CONN_TIMEOUT_MS 5000
#endif

// Publish/measure periods
#ifndef DHT_READ_PERIOD_MS
#define DHT_READ_PERIOD_MS 2000
#endif

#ifndef DISTANCE_READ_PERIOD_MS
#define DISTANCE_READ_PERIOD_MS 100
#endif

#ifndef PUB_PERIOD_MS
#define PUB_PERIOD_MS 500
#endif

#if HAS_DHT
// -------- DHT sensor --------
#ifndef DHTPIN
#define DHTPIN 4
#endif

#ifndef DHTTYPE
#define DHTTYPE DHT11
#endif

DHT dht(DHTPIN, DHTTYPE);
#endif

#if HAS_HCSR04
// -------- HC-SR04 pins --------
#ifndef TRIG_PIN
#define TRIG_PIN 5
#endif

#ifndef ECHO_PIN
#define ECHO_PIN 18
#endif
#endif

#if HAS_LED
// -------- RGB LED pins --------
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
#endif

// -------- Globals --------
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

uint32_t lastPubMs = 0;

float lastDistanceCm = NAN;
float lastTemperatureC = NAN;
float lastHumidity = NAN;

#if HAS_DHT
uint32_t lastDhtMs = 0;
#endif

#if HAS_HCSR04
uint32_t lastDistanceMs = 0;
#endif

char topicStatus[64];
char topicDistance[64];
char topicTemperature[64];
char topicHumidity[64];

void setupTopics()
{
  snprintf(topicStatus, sizeof(topicStatus), "esp/%s/status", MQTT_CLIENT_ID);
  snprintf(topicDistance, sizeof(topicDistance), "esp/%s/distance_cm", MQTT_CLIENT_ID);
  snprintf(topicTemperature, sizeof(topicTemperature), "esp/%s/temperature_c", MQTT_CLIENT_ID);
  snprintf(topicHumidity, sizeof(topicHumidity), "esp/%s/humidity", MQTT_CLIENT_ID);
}

void publishFloat(const char *topic, float value)
{
  char msg[32];
  if (isnan(value)) {
    snprintf(msg, sizeof(msg), "nan");
  } else {
    snprintf(msg, sizeof(msg), "%.2f", value);
  }
  mqttClient.publish(topic, msg);
}

#if HAS_LED
const char LED_TOPIC[] = "LED";
const int LEDC_CH_R = 0;
const int LEDC_CH_G = 1;
const int LEDC_CH_B = 2;

bool ledNanMode = false;
bool ledBlinkOn = false;
uint32_t lastLedBlinkMs = 0;

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

bool payloadIsNan(const char *text)
{
  if (!text || !*text) return true;
  char c0 = (char)tolower((unsigned char)text[0]);
  char c1 = (char)tolower((unsigned char)text[1]);
  char c2 = (char)tolower((unsigned char)text[2]);
  return (c0 == 'n' && c1 == 'a' && c2 == 'n');
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
#endif

// -------- WiFi / MQTT helpers --------
bool connectWifiTimed()
{
  if (WiFi.status() == WL_CONNECTED) return true;

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  uint32_t t0 = millis();
  while (WiFi.status() != WL_CONNECTED && (millis() - t0) < WIFI_CONN_TIMEOUT_MS) {
    delay(200);
  }
  return WiFi.status() == WL_CONNECTED;
}

bool connectMqttTimed()
{
  if (mqttClient.connected()) return true;

  uint32_t t0 = millis();
  while (!mqttClient.connected() && (millis() - t0) < MQTT_CONN_TIMEOUT_MS) {
    mqttClient.connect(MQTT_CLIENT_ID);
    if (!mqttClient.connected()) delay(250);
  }

  if (mqttClient.connected()) {
    mqttClient.publish(topicStatus, "online", true);
#if HAS_LED
    mqttClient.subscribe(LED_TOPIC);
#endif
  }
  return mqttClient.connected();
}

#if HAS_HCSR04
// -------- Sensor read --------
float readDistanceCm()
{
  // Trigger HC-SR04
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Echo pulse (timeout 30ms)
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  // duration==0 => timeout / out of range
  if (duration <= 0) return NAN;

  // Convert to cm (speed of sound ~343 m/s)
  // ~29.1 us/cm roundtrip
  float distance = duration / 29.1f / 2.0f;

  // basic sanity clamp (HC-SR04 typical range ~2-400 cm)
  if (distance < 2.0f || distance > 400.0f) return NAN;

  return distance;
}
#endif

// -------- Setup --------
void setup()
{
  Serial.begin(115200);
  delay(50);

#if HAS_HCSR04
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
#endif

#if HAS_LED
  ledcSetup(LEDC_CH_R, LEDC_FREQ, LEDC_RES_BITS);
  ledcSetup(LEDC_CH_G, LEDC_FREQ, LEDC_RES_BITS);
  ledcSetup(LEDC_CH_B, LEDC_FREQ, LEDC_RES_BITS);
  ledcAttachPin(LED_PIN_R, LEDC_CH_R);
  ledcAttachPin(LED_PIN_G, LEDC_CH_G);
  ledcAttachPin(LED_PIN_B, LEDC_CH_B);
  setRgb(0, 0, 0);
#endif

#if HAS_DHT
  dht.begin();
#endif

  setupTopics();
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
#if HAS_LED
  mqttClient.setCallback(onMqttMessage);
#endif

  Serial.printf("\nBoot: ESP32 MQTT station: %s\n", MQTT_CLIENT_ID);

  bool w = connectWifiTimed();
  Serial.printf("WiFi: %s\n", w ? "connected" : "not connected");
  if (w) {
    bool m = connectMqttTimed();
    Serial.printf("MQTT: %s\n", m ? "connected" : "not connected");
  }
}

// -------- Loop --------
void loop()
{
  uint32_t now = millis();

  // Non-blocking reconnect attempts
  static uint32_t lastConnTry = 0;
  if (now - lastConnTry > 1000) {
    lastConnTry = now;
    if (WiFi.status() != WL_CONNECTED) connectWifiTimed();
    if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) connectMqttTimed();
  }

  if (mqttClient.connected()) mqttClient.loop();

#if HAS_LED
  handleLedBlink(now);
#endif

#if HAS_HCSR04
  if (now - lastDistanceMs >= DISTANCE_READ_PERIOD_MS) {
    lastDistanceMs = now;
    lastDistanceCm = readDistanceCm();
    if (isnan(lastDistanceCm)) {
      Serial.println("distance_cm=nan");
    } else {
      Serial.printf("distance_cm=%.2f\n", lastDistanceCm);
    }
  }
#endif

#if HAS_DHT
  if (now - lastDhtMs >= DHT_READ_PERIOD_MS) {
    lastDhtMs = now;
    lastTemperatureC = dht.readTemperature();
    lastHumidity = dht.readHumidity();

    if (isnan(lastTemperatureC) || isnan(lastHumidity)) {
      Serial.println("temperature_c/humidity=nan");
    } else {
      Serial.printf("temperature_c=%.2f humidity=%.2f\n", lastTemperatureC, lastHumidity);
    }
  }
#endif

  if (mqttClient.connected() && (now - lastPubMs) >= PUB_PERIOD_MS) {
    lastPubMs = now;

#if HAS_HCSR04
    publishFloat(topicDistance, lastDistanceCm);
#endif

#if HAS_DHT
    publishFloat(topicTemperature, lastTemperatureC);
    publishFloat(topicHumidity, lastHumidity);
#endif
  }
}

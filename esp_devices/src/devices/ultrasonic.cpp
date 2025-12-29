// Ultrasonic device: reads HC-SR04 and publishes distance over MQTT.
// It subscribes to the Temperature station to adjust speed of sound.
// Key config macros: TRIG_PIN/ECHO_PIN, HCSR04_TIMEOUT_US, DISTANCE_READ_PERIOD_MS.
// It publishes to esp/<client_id>/distance_cm and prints calculation details to Serial.
// State variables include lastDistanceCm, lastEchoDurationUs, and speedOfSoundMps.
// This file is selected only in the esp32_Ultrasonic environment.
#include "device.h"
#include "common.h"

#ifndef TRIG_PIN
#define TRIG_PIN 5
#endif

#ifndef ECHO_PIN
#define ECHO_PIN 18
#endif

#ifndef HCSR04_TIMEOUT_US
#define HCSR04_TIMEOUT_US 30000
#endif

#ifndef TEMP_SOURCE_STATION
#define TEMP_SOURCE_STATION "Temperature"
#endif

#ifndef TEMP_SOURCE_METRIC
#define TEMP_SOURCE_METRIC "temperature_c"
#endif

#ifndef DISTANCE_READ_PERIOD_MS
#define DISTANCE_READ_PERIOD_MS 50
#endif

#ifndef DISTANCE_LOG_PERIOD_MS
#define DISTANCE_LOG_PERIOD_MS 200
#endif

namespace {
char topicStatus[64];
char topicDistance[64];
char topicTempIn[64];

float lastDistanceCm = NAN;
float lastTemperatureC = NAN;
float speedOfSoundMps = 343.0f;
float speedCmPerUs = 0.0343f;
long lastEchoDurationUs = 0;

uint32_t lastDistanceMs = 0;
uint32_t lastDistanceLogMs = 0;
bool tempSubscribed = false;

void setupTopics()
{
  snprintf(topicStatus, sizeof(topicStatus), "esp/%s/status", MQTT_CLIENT_ID);
  snprintf(topicDistance, sizeof(topicDistance), "esp/%s/distance_cm", MQTT_CLIENT_ID);
  snprintf(topicTempIn, sizeof(topicTempIn), "esp/%s/%s", TEMP_SOURCE_STATION, TEMP_SOURCE_METRIC);
}

void updateSpeedFromTemp(float tempC)
{
  // Speed of sound in air ~= 331.3 + 0.606 * T(C) m/s
  speedOfSoundMps = 331.3f + (0.606f * tempC);
  speedCmPerUs = speedOfSoundMps / 10000.0f;
  lastTemperatureC = tempC;
  Serial.printf(
      "temp_c=%.2f -> speed_mps=%.2f (speed_cm_per_us=%.5f)\n",
      tempC,
      speedOfSoundMps,
      speedCmPerUs);
}

void ensureMqttConnected()
{
  if (!mqttClient.connected()) {
    if (!connectMqttTimed()) return;
    tempSubscribed = false;
  }

  if (!tempSubscribed) {
    mqttClient.publish(topicStatus, "online", true);
    mqttClient.subscribe(topicTempIn);
    tempSubscribed = true;
  }
}

void onMqttMessage(char *topic, byte *payload, unsigned int length)
{
  if (strcmp(topic, topicTempIn) != 0) return;

  char buf[32];
  size_t n = length < sizeof(buf) - 1 ? length : sizeof(buf) - 1;
  memcpy(buf, payload, n);
  buf[n] = '\0';

  const char *text = buf;
  while (*text && isspace((unsigned char)*text)) {
    text++;
  }

  float tempC = NAN;
  if (!payloadIsNan(text)) {
    tempC = strtof(text, nullptr);
  }

  if (isnan(tempC)) {
    lastTemperatureC = NAN;
    speedOfSoundMps = 343.0f;
    speedCmPerUs = speedOfSoundMps / 10000.0f;
    Serial.println("temp_c=nan -> using default speed_mps=343.0");
  } else {
    updateSpeedFromTemp(tempC);
  }
}

float readDistanceCm()
{
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, HCSR04_TIMEOUT_US);
  lastEchoDurationUs = duration;

  if (duration <= 0) return NAN;

  float distance = (duration * speedCmPerUs) / 2.0f;
  if (distance < 2.0f || distance > 400.0f) return NAN;

  return distance;
}

void logDistance(uint32_t now)
{
  if (now - lastDistanceLogMs < DISTANCE_LOG_PERIOD_MS) return;
  lastDistanceLogMs = now;

  if (isnan(lastDistanceCm)) {
    Serial.println("distance_cm=nan");
    return;
  }

  Serial.printf(
      "distance_cm=%.2f duration_us=%ld temp_c=%.2f speed_mps=%.2f speed_cm_us=%.5f calc=dur*speed_cm_us/2\n",
      lastDistanceCm,
      lastEchoDurationUs,
      lastTemperatureC,
      speedOfSoundMps,
      speedCmPerUs);
}
} // namespace

void deviceSetup()
{
  Serial.begin(115200);
  delay(50);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);

  setupTopics();
  setupMqttServer();
  mqttClient.setCallback(onMqttMessage);

  Serial.printf("\nBoot: ESP32 MQTT station: %s (Ultrasonic)\n", MQTT_CLIENT_ID);

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

  if (now - lastDistanceMs >= DISTANCE_READ_PERIOD_MS) {
    lastDistanceMs = now;
    lastDistanceCm = readDistanceCm();
    logDistance(now);
  }

  if (mqttClient.connected()) {
    publishFloat(topicDistance, lastDistanceCm);
  }
}

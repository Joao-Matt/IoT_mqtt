// Temperature device: reads DHT11 and publishes temperature/humidity over MQTT.
// It uses DHTPIN/DHTTYPE macros plus DHT_READ_PERIOD_MS and PUB_PERIOD_MS.
// Topics are esp/<client_id>/temperature_c and esp/<client_id>/humidity.
// A retained status message is published on esp/<client_id>/status.
// State variables include lastTemperatureC, lastHumidity, and timing counters.
// This file is selected only in the esp32_Temperature environment.
#include "device.h"
#include "common.h"
#include <DHT.h>

#ifndef DHTPIN
#define DHTPIN 4
#endif

#ifndef DHTTYPE
#define DHTTYPE DHT11
#endif

#ifndef DHT_READ_PERIOD_MS
#define DHT_READ_PERIOD_MS 1000
#endif

#ifndef PUB_PERIOD_MS
#define PUB_PERIOD_MS 1000
#endif

namespace {
DHT dht(DHTPIN, DHTTYPE);

char topicStatus[64];
char topicTemperature[64];
char topicHumidity[64];

float lastTemperatureC = NAN;
float lastHumidity = NAN;

uint32_t lastDhtMs = 0;
uint32_t lastPubMs = 0;
bool statusPublished = false;

void setupTopics()
{
  snprintf(topicStatus, sizeof(topicStatus), "esp/%s/status", MQTT_CLIENT_ID);
  snprintf(topicTemperature, sizeof(topicTemperature), "esp/%s/temperature_c", MQTT_CLIENT_ID);
  snprintf(topicHumidity, sizeof(topicHumidity), "esp/%s/humidity", MQTT_CLIENT_ID);
}

void ensureMqttConnected()
{
  if (!mqttClient.connected()) {
    if (!connectMqttTimed()) return;
    statusPublished = false;
  }

  if (!statusPublished) {
    mqttClient.publish(topicStatus, "online", true);
    statusPublished = true;
  }
}
} // namespace

void deviceSetup()
{
  Serial.begin(115200);
  delay(50);

  dht.begin();
  setupTopics();
  setupMqttServer();

  Serial.printf("\nBoot: ESP32 MQTT station: %s (Temperature)\n", MQTT_CLIENT_ID);

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

  if (mqttClient.connected() && (now - lastPubMs) >= PUB_PERIOD_MS) {
    lastPubMs = now;
    publishFloat(topicTemperature, lastTemperatureC);
    publishFloat(topicHumidity, lastHumidity);
  }
}

#include "common.h"

WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

void setupMqttServer()
{
  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
}

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

  return mqttClient.connected();
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

bool payloadIsNan(const char *text)
{
  if (!text || !*text) return true;
  char c0 = (char)tolower((unsigned char)text[0]);
  char c1 = (char)tolower((unsigned char)text[1]);
  char c2 = (char)tolower((unsigned char)text[2]);
  return (c0 == 'n' && c1 == 'a' && c2 == 'n');
}

#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include <ctype.h>

#if __has_include("secrets.h")
#include "secrets.h"
#endif

// -------- WiFi / MQTT defaults --------
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

extern WiFiClient wifiClient;
extern PubSubClient mqttClient;

void setupMqttServer();
bool connectWifiTimed();
bool connectMqttTimed();
void publishFloat(const char *topic, float value);
bool payloadIsNan(const char *text);

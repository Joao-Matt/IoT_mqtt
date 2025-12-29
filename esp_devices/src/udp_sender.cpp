// UDP sender: tiny helper for streaming IMU data off the ESP.
// UDP_REMOTE_IP and UDP_REMOTE_PORT select the destination.
// setupUDP initializes the remote IP and checks WiFi state.
// sendIMUData formats pitch/roll and sends a UDP packet.
// It intentionally drops data if WiFi is down to keep loop non-blocking.
// This keeps IMU device code clean and focused on sensor math.
#include "udp_sender.h"
#include "common.h"
#include <WiFiUdp.h>

#ifndef UDP_REMOTE_IP
#define UDP_REMOTE_IP "192.168.1.174"
#endif

#ifndef UDP_REMOTE_PORT
#define UDP_REMOTE_PORT 9000
#endif

namespace {
WiFiUDP udp;
IPAddress remoteIp;
bool udpReady = false;
}

void setupUDP()
{
  if (!remoteIp.fromString(UDP_REMOTE_IP)) {
    Serial.println("UDP: invalid remote IP");
    return;
  }

  if (!connectWifiTimed()) {
    Serial.println("UDP: WiFi not connected");
  }

  udpReady = true;
}

void sendIMUData(float pitch, float roll)
{
  if (!udpReady || WiFi.status() != WL_CONNECTED) {
    return;
  }

  char msg[64];
  snprintf(msg, sizeof(msg), "pitch=%.2f,roll=%.2f", pitch, roll);

  udp.beginPacket(remoteIp, UDP_REMOTE_PORT);
  udp.write(reinterpret_cast<const uint8_t *>(msg), strlen(msg));
  udp.endPacket();
}

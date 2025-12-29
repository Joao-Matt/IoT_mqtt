// UDP sender interface used by the IMU device.
// setupUDP() prepares the destination address and validates settings.
// sendIMUData(pitch, roll) transmits the formatted payload.
// This abstraction keeps network code separate from sensor math.
// UDP is chosen for low-latency, best-effort streaming.
// The implementation lives in esp_devices/src/udp_sender.cpp.
#pragma once

void setupUDP();
void sendIMUData(float pitch, float roll);

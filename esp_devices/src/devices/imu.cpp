// IMU device: reads MPU6050 over I2C and computes pitch/roll.
// It uses a complementary filter (SimpleKalman) to fuse gyro + accel.
// Data is sent via UDP using udp_sender to a configurable IP/port.
// Key state variables are pitch, roll, lastMicros, and lastPrint.
// This file does not publish MQTT; it streams via UDP for low latency.
// This file is selected only in the esp32_IMU environment.
#include "device.h"
#include "common.h"
#include <Wire.h>
#include "udp_sender.h"
#include "SimpleKalmanFilter.h"

const int MPU_ADDR = 0x68;

namespace {
SimpleKalman kPitch;
SimpleKalman kRoll;

float pitch = 0.0f;
float roll = 0.0f;
uint32_t lastMicros = 0;
uint32_t lastPrint = 0;
bool udpReady = false;
}

void deviceSetup()
{
  Serial.begin(115200);
  Wire.begin();

  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x6B);
  Wire.write(0);
  Wire.endTransmission();

  lastMicros = micros();

  setupUDP();
  udpReady = true;
  Serial.println("IMU setup complete.");
}

void deviceLoop()
{
  uint32_t nowMicros = micros();
  float dt = (nowMicros - lastMicros) * 1e-6f;
  lastMicros = nowMicros;
  if (dt <= 0.0f) {
    dt = 1e-3f;
  }

  int16_t axRaw, ayRaw, azRaw, gxRaw, gyRaw, gzRaw;
  Wire.beginTransmission(MPU_ADDR);
  Wire.write(0x3B);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)MPU_ADDR, (uint8_t)14, (uint8_t)true);

  if (Wire.available() < 14) {
    delay(10);
    return;
  }

  axRaw = Wire.read() << 8 | Wire.read();
  ayRaw = Wire.read() << 8 | Wire.read();
  azRaw = Wire.read() << 8 | Wire.read();
  Wire.read();
  Wire.read(); // Skip temp
  gxRaw = Wire.read() << 8 | Wire.read();
  gyRaw = Wire.read() << 8 | Wire.read();
  gzRaw = Wire.read() << 8 | Wire.read();

  const float ACC_SCALE = 9.81f / 16384.0f;
  const float GYRO_SCALE = 1.0f / 131.0f;

  float ax = axRaw * ACC_SCALE;
  float ay = ayRaw * ACC_SCALE;
  float az = azRaw * ACC_SCALE;
  float gx = gxRaw * GYRO_SCALE;
  float gy = gyRaw * GYRO_SCALE;
  float gz = gzRaw * GYRO_SCALE;

  (void)gz;

  float pitchAcc = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / PI;
  float rollAcc = atan2f(ay, az) * 180.0f / PI;

  pitch = kPitch.update(gx, pitchAcc, dt);
  roll = kRoll.update(gy, rollAcc, dt);

  if (millis() - lastPrint >= 500) {
    Serial.printf("Pitch: %.2f  Roll: %.2f\r\n", pitch, roll);
    lastPrint = millis();
  }

  if (udpReady) {
    sendIMUData(pitch, roll);
  }

  delay(10);
}

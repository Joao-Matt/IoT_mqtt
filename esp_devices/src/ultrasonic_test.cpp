// Standalone HC-SR04 test to isolate wiring and sensor health.
// Uses TRIG_PIN/ECHO_PIN and prints raw echo duration and distance.
// If echo_us=0, the sensor is not responding or wiring/power is wrong.
// GPIO loopback mode can test ESP pins without the sensor.
// This sketch avoids MQTT so debugging is purely hardware-focused.
// Use this when the main ultrasonic firmware only returns NaN.
#include <Arduino.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

// Set to 1 to run a GPIO loopback test (jumper TRIG_PIN -> ECHO_PIN).
#define GPIO_LOOPBACK_TEST 0

void setup()
{
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  digitalWrite(TRIG_PIN, LOW);
  Serial.println("Ultrasonic test: starting");
#if GPIO_LOOPBACK_TEST
  Serial.println("GPIO loopback test: connect TRIG to ECHO (no sensor).");
#endif
}

void loop()
{
#if GPIO_LOOPBACK_TEST
  digitalWrite(TRIG_PIN, HIGH);
  delay(200);
  int highRead = digitalRead(ECHO_PIN);
  digitalWrite(TRIG_PIN, LOW);
  delay(200);
  int lowRead = digitalRead(ECHO_PIN);
  Serial.printf("loopback high=%d low=%d\n", highRead, lowRead);
  return;
#endif

  // Trigger HC-SR04
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout 30ms
  if (duration <= 0) {
    Serial.println("echo_us=0");
    delay(100);
    return;
  }

  // Sound speed ~343 m/s -> ~29.1 us/cm round-trip
  float distance = duration / 29.1f / 2.0f;
  Serial.printf("echo_us=%ld distance_cm=%.2f\n", duration, distance);

  delay(100);
}

// Standalone HC-SR04 test sketch for basic distance measurement.
// Triggers the sensor, measures echo pulse, and prints distance.
// Uses TRIG_PIN and ECHO_PIN constants for wiring reference.
// No MQTT or temperature compensation, just raw distance output.
// Use this for quick sensor checks before integrating with MQTT code.
// Expect values to update roughly every 100 ms.
#include <Arduino.h>

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;

void setup()
{
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
}

void loop()
{
  // Trigger HC-SR04
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  // Get echo pulse
  long duration = pulseIn(ECHO_PIN, HIGH, 30000); // Timeout 30ms

  // Convert to distance (cm):
  // Sound speed ~343 m/s -> ~29.1 us/cm round-trip
  float distance = duration / 29.1f / 2.0f;

  Serial.println(distance);

  delay(100);
}

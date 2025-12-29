// Standalone RGB LED test to validate wiring and pin assignment.
// Cycles through red, green, blue, white, and off states.
// Uses PIN_R/PIN_G/PIN_B macros for the RGB channels.
// No MQTT or sensors are involved; purely hardware verification.
// If colors are inverted, the LED may be common-anode.
// Use this file when debugging LED brightness or wiring issues.
#include <Arduino.h>

#define PIN_R 25
#define PIN_G 26
#define PIN_B 27

void setup()
{
  pinMode(PIN_R, OUTPUT);
  pinMode(PIN_G, OUTPUT);
  pinMode(PIN_B, OUTPUT);
}

void loop()
{
  // Red
  digitalWrite(PIN_R, HIGH);
  digitalWrite(PIN_G, LOW);
  digitalWrite(PIN_B, LOW);
  delay(1000);

  // Green
  digitalWrite(PIN_R, LOW);
  digitalWrite(PIN_G, HIGH);
  digitalWrite(PIN_B, LOW);
  delay(1000);

  // Blue
  digitalWrite(PIN_R, LOW);
  digitalWrite(PIN_G, LOW);
  digitalWrite(PIN_B, HIGH);
  delay(1000);

  // White
  digitalWrite(PIN_R, HIGH);
  digitalWrite(PIN_G, HIGH);
  digitalWrite(PIN_B, HIGH);
  delay(1000);

  // Off
  digitalWrite(PIN_R, LOW);
  digitalWrite(PIN_G, LOW);
  digitalWrite(PIN_B, LOW);
  delay(1000);
}

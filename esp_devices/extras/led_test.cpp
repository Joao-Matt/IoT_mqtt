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

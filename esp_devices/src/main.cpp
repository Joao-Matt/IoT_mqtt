#include <Arduino.h>
#include "device.h"

// Entry point: delegate to the selected device implementation.
void setup()
{
  deviceSetup();
}

void loop()
{
  deviceLoop();
}

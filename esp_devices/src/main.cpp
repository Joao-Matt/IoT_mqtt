// This file is the thin entry point for all ESP device builds.
// It forwards setup/loop to the device-specific implementation selected per env.
// The chosen device file is compiled via platformio.ini src_filter rules.
// There is no device state stored here; each device owns its own variables.
// This keeps project structure clean and easier to reason about for interviews.
// Add a new device by implementing deviceSetup/deviceLoop and updating src_filter.
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

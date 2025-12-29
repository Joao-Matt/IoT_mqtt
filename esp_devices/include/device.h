// Simple interface each device implementation must provide.
// deviceSetup() runs once at boot for sensor init and network setup.
// deviceLoop() runs continuously for sampling and publishing.
// The main.cpp file calls these functions without knowing device details.
// This keeps the entrypoint minimal and each device self-contained.
// New devices just implement these two functions in their own file.
#pragma once

void deviceSetup();
void deviceLoop();

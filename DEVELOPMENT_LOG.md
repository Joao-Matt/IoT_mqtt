# Development Log

This file summarizes how the project was built, what changed over time, and the main bugs/debugging steps that shaped the final design.

## 1. Starting Point

The project started from a single Arduino sketch for one ESP32:

- Connected to WiFi.
- Connected to an MQTT broker on the Raspberry Pi.
- Read an HC-SR04 ultrasonic distance sensor.
- Published distance to MQTT.
- Printed distance readings to the serial monitor.

The first goal was to move that sketch into a PlatformIO Arduino framework project so it could be uploaded and maintained more cleanly.

## 2. PlatformIO Setup

The initial PlatformIO environment used:

- Board: `esp32doit-devkit-v1`
- Platform: `espressif32`
- Framework: `arduino`
- MQTT library: `knolleary/PubSubClient@^2.8`

One early issue was serial monitor garbage. The cause was a PlatformIO config typo:

```ini
serial_monitor_speed = 115200
```

PlatformIO ignored that option and opened the monitor at `9600`, while the ESP code used `Serial.begin(115200)`. The fix was:

```ini
monitor_speed = 115200
```

After that, the serial monitor decoded the ESP output correctly.

## 3. First ESP: Ultrasonic MQTT Publisher

The first working firmware was the ultrasonic station:

- Trigger pin: GPIO `5`
- Echo pin: GPIO `18`
- Reads HC-SR04 pulse width.
- Converts echo time to distance.
- Publishes distance to `esp/Ultrasonic/distance_cm`.
- Publishes retained status to `esp/Ultrasonic/status`.

Originally the station name/topic had inconsistent spelling (`Ulstrasonic`). This caused topic confusion on the Raspberry Pi side. The project was later standardized to:

- Environment: `esp32_Ultrasonic`
- MQTT client ID: `Ultrasonic`
- Topic prefix: `esp/Ultrasonic/...`

## 4. Second ESP: Temperature/DHT

A second ESP32 was added for a DHT sensor:

- DHT pin: GPIO `4`
- Sensor type: `DHT11`
- Reads temperature and humidity.
- Publishes at `1 Hz`.
- Publishes:
  - `esp/Temperature/temperature_c`
  - `esp/Temperature/humidity`
  - `esp/Temperature/status`

This required adding the DHT library to only the temperature environment:

```ini
lib_deps =
  knolleary/PubSubClient@^2.8
  adafruit/DHT sensor library@^1.4.4
```

The DHT frequency was kept low because DHT11 sensors are slow and unreliable when read too frequently.

## 5. Raspberry Pi MQTT Hub

The Raspberry Pi script was built to act as the central MQTT hub:

- Connects to Mosquitto on `localhost:1883`.
- Subscribes to `esp/#`.
- Tracks ESP status topics.
- Shows which devices are online.
- Lets the user choose which topic to display.
- Publishes LED commands to the `LED` topic.

Interactive commands were added over time:

- `list` - show discovered topics.
- `devices` - show ESP device status.
- `show <n|topic|station metric>` - choose a topic to display.
- `current` - show current selected topic.
- `led` - show last LED command.
- `led on|off` - enable or disable LED publish logging.
- `source imu|us` - choose whether LED follows IMU or ultrasonic distance.
- `q` - stop showing the current topic but keep the program running.
- `exit` - stop the program.

The Pi script was uploaded to:

```text
/home/jacksparrow/code/new_mqtt_try.py
```

The Raspberry Pi IP used during development was:

```text
192.168.1.174
```

No credentials are stored in the repo.

## 6. Third ESP: RGB LED

An RGB LED ESP was added:

- Red pin: GPIO `25`
- Green pin: GPIO `26`
- Blue pin: GPIO `27`
- MQTT client ID: `LED`
- Subscribes to topic: `LED`

The Pi publishes LED commands as payloads like:

```text
255,0,0
0,255,0
nan
```

The LED behavior:

- Valid `r,g,b` payload sets the LED color.
- `nan` makes the LED blink blue.
- Shorter distance maps closer to red.
- Farther distance maps closer to green.
- Later, the color mapping was improved to a fuller spectrum for a brighter, more impressive visual effect.

One important debugging discovery was that the LED test sketch worked, but the main code did not appear to. The cause was not the LED code itself. The wrong firmware/env or port was being checked, and later a wiring issue was confirmed. The ESP was actually on `COM14`, not `COM13`.

## 7. Multi-Environment ESP Project

The project was refactored into one PlatformIO project with multiple environments:

- `esp32_Ultrasonic`
- `esp32_Temperature`
- `esp32_LED`
- `esp32_IMU`
- `esp32_Ultrasonic_Test`

The final structure keeps one shared entry point:

```cpp
void setup() {
  deviceSetup();
}

void loop() {
  deviceLoop();
}
```

Each environment selects its device implementation using `src_filter`:

```ini
src_filter =
  +<main.cpp>
  +<common.cpp>
  +<devices/led.cpp>
```

This keeps the repo as one proper project while still allowing each ESP role to have separate code.

## 8. Shared WiFi/MQTT Code

Common ESP behavior was moved into shared helpers:

- WiFi connection.
- MQTT connection.
- Status publishing.
- Topic construction.
- Reconnect handling.

This reduced repeated code between the ESP devices and made the firmware easier to track.

Secrets were moved out of source code and into:

```text
esp_devices/include/secrets.h
```

That file is ignored by Git. A template exists as:

```text
esp_devices/include/secrets.h.example
```

## 9. Build Flag Bug

One major PlatformIO bug came from putting build flags in the wrong place.

The broken behavior looked like this:

```text
Library Manager: Installing -D DISTANCE_READ_PERIOD_MS
Library Manager: Installing -D HCSR04_TIMEOUT_US
Library Manager: Installing "Temperature"
```

PlatformIO interpreted build flags as library dependencies because they were under `lib_deps`.

The fix was to move them under `build_flags`:

```ini
build_flags =
  -D MQTT_CLIENT_ID=\"Ultrasonic\"
  -D DISTANCE_READ_PERIOD_MS=50
  -D HCSR04_TIMEOUT_US=30000
```

This fixed the accidental installation of unrelated libraries like `NanoPlayBoard`, which caused AVR-only include errors such as:

```text
fatal error: avr/io.h: No such file or directory
```

## 10. MQTT Topic Debugging

Several topic problems were found and fixed:

- `Ulstrasonic` spelling did not match `Ultrasonic`.
- Manual `mosquitto_pub` tests helped prove the Pi could publish to `LED`.
- Pi-side LED publish logs were added so commands could be seen clearly.
- ESP status topics helped confirm which ESPs were online.
- The Pi was changed so the user could select topics by number instead of typing the full topic every time.

The final topic convention is:

```text
esp/<DeviceName>/<MetricName>
```

Examples:

```text
esp/Temperature/temperature_c
esp/Temperature/humidity
esp/Ultrasonic/distance_cm
esp/LED/status
```

The LED command topic intentionally remains short:

```text
LED
```

## 11. Temperature-Compensated Ultrasonic Distance

The ultrasonic ESP was changed to listen to the DHT temperature topic:

```text
esp/Temperature/temperature_c
```

It then calculates speed of sound based on air temperature:

```text
speed_mps = 331.3 + 0.606 * temp_c
```

Then distance is calculated from echo time:

```text
distance_cm = echo_us * (speed_mps / 10000) / 2
```

This made the ultrasonic measurement more physically correct than using a fixed `343 m/s`.

The serial monitor was updated to show:

- Received temperature.
- Calculated speed of sound.
- Echo duration.
- Distance result.

## 12. Ultrasonic Timing Debugging

The ultrasonic read rate was initially pushed very high. It was later reduced to `20 Hz`:

```ini
-D DISTANCE_READ_PERIOD_MS=50
```

This is more realistic for HC-SR04 because the sensor needs time for the sound pulse to travel and settle. Reading too fast can cause bad readings or repeated `NaN`.

## 13. HC-SR04 Hardware Debugging

The ultrasonic sensor later returned only:

```text
echo_us=0
```

An isolated test environment was created:

```ini
[env:esp32_Ultrasonic_Test]
src_filter =
  -<*>
  +<ultrasonic_test.cpp>
```

The test firmware printed raw echo values and helped separate hardware problems from MQTT/application problems.

Debug steps discussed:

- Confirm trigger pin toggles.
- Confirm echo pin is wired correctly.
- Confirm power and ground.
- Confirm current/power availability.
- Use a GPIO loopback test to check whether the ESP input pin still works.
- Check that HC-SR04 Echo is not connected directly at `5V` to the ESP32 input.

Important hardware note:

```text
HC-SR04 Echo is often 5V. ESP32 GPIO is 3.3V only.
Use a voltage divider or level shifter for Echo.
```

Direct 5V into an ESP32 input can damage the GPIO.

## 14. IMU Device

An IMU environment was added from working MPU6050 code:

- Environment: `esp32_IMU`
- Reads MPU6050 over I2C using `Wire`.
- Calculates pitch and roll.
- Sends pitch/roll to the Raspberry Pi using UDP.
- Prints pitch and roll to the serial monitor.

The code uses a class named `SimpleKalman`, but in this project it behaves more like a lightweight complementary-style filter than a full formal Kalman filter.

The IMU sends UDP packets to the Pi, defaulting to:

```text
192.168.1.174:9000
```

The Pi can use IMU pitch as the LED color source:

```text
source imu
```

Or it can use ultrasonic distance:

```text
source us
```

## 15. MQTT vs UDP Decision

MQTT is used for sensor/status messages where reliability and topic structure matter:

- Device status.
- Temperature.
- Humidity.
- Distance.
- LED commands.

UDP is used for IMU pitch/roll because it is lightweight and suitable for fast stream-style data where the newest sample matters more than guaranteed delivery.

This created a useful contrast in the project:

- MQTT: structured, broker-based, easier to inspect and route.
- UDP: low overhead, direct, good for fast real-time-ish updates.

## 16. Raspberry Pi LED Source Selection

The Pi was changed so LED color can be driven by either:

- Ultrasonic distance.
- IMU pitch.

The program asks/accepts commands to switch source. This lets the same LED device react to different data streams without reflashing the LED ESP.

This is a good design decision because the LED ESP stays simple:

- It only knows how to receive colors.
- It does not need to know where the data came from.
- The Pi acts as the decision layer.

## 17. Git and Documentation

The project was initialized as a Git repo and connected to:

```text
https://github.com/Joao-Matt/IoT_mqtt.git
```

Documentation was added/updated:

- Root `README.md`
- `rpi_hub/README.md`
- Device architecture section
- Technology explanation
- Build/upload commands
- Raspberry Pi commands
- Secrets/configuration notes

Interview preparation files were created locally, then removed from the pushed repo and added to `.gitignore`:

- `INTERVIEW_QA.md`
- `INTERVIEW_CODE_TECH_QA.md`

They exist locally for practice but are intentionally not part of the public repo.

## 18. Final Architecture

High-level flow:

```text
[Temperature ESP] -> MQTT -> [Pi Hub]
        |
        +-> MQTT temperature -> [Ultrasonic ESP]

[Ultrasonic ESP] -> MQTT distance -> [Pi Hub]

[IMU ESP] -> UDP pitch/roll -> [Pi Hub]

[Pi Hub] -> MQTT LED command -> [LED ESP]
```

The Raspberry Pi is the central controller:

- Watches device status.
- Displays selected topics.
- Combines sensor data.
- Decides what LED command to send.

The ESPs are specialized workers:

- Temperature ESP measures environment.
- Ultrasonic ESP measures distance.
- IMU ESP streams motion.
- LED ESP receives color commands.

## 19. Main Lessons From Debugging

The biggest practical lessons were:

- Serial garbage often means baud/config mismatch.
- PlatformIO option names matter.
- `lib_deps` and `build_flags` must stay separate.
- MQTT topic spelling must be exact.
- Device IDs should be stable and consistent.
- Status topics make distributed debugging much easier.
- Test sketches are useful when hardware behavior is uncertain.
- Hardware issues can look like software bugs.
- ESP32 inputs must be protected from 5V signals.
- A single multi-env PlatformIO project is easier to present and maintain than several disconnected sketches.

## 20. Current Project State

The project now contains:

- A multi-env ESP32 PlatformIO project.
- Separate firmware implementation per ESP role.
- Shared ESP WiFi/MQTT helpers.
- Standalone sensor test sketches.
- Raspberry Pi MQTT/UDP hub.
- LED control based on ultrasonic distance or IMU pitch.
- Documentation for setup, architecture, commands, and technologies.

The result is a small distributed IoT system with multiple ESP32 devices, a Raspberry Pi broker/controller, MQTT messaging, UDP streaming, hardware debugging tools, and Git-backed documentation.

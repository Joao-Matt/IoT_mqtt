# Interview Q&A (Project: IoT_mqtt_1)

Below are sample interview questions and recommended answers based on this project.

## 1) What problem were you solving, and what does the system do end-to-end?
**Recommended answer:**  
I built a small IoT system that fuses sensor data from multiple ESP32 devices and visualizes it in real time. One ESP measures distance (HC-SR04), another measures temperature/humidity (DHT11), a third drives an RGB LED, and an IMU device streams pitch/roll. A Raspberry Pi runs an MQTT broker and a hub script that subscribes to sensors, publishes LED commands, and provides an interactive view of topics.

## 2) Why MQTT for some data and UDP for the IMU stream?
**Recommended answer:**  
MQTT gives reliable delivery and a clean topic structure, which is ideal for sensor values and device status. The IMU stream is high-rate and tolerant to packet loss, so UDP is a better low-latency, low-overhead choice. Dropped IMU packets are acceptable because new data arrives quickly.

## 3) Walk me through the architecture and data flow.
**Recommended answer:**  
Each ESP publishes to `esp/<station>/<metric>` or consumes commands like `LED`. The Pi runs Mosquitto and a Python hub. The Temperature ESP publishes temperature/humidity. The Ultrasonic ESP subscribes to temperature to adjust speed of sound and publishes distance. The hub listens to `esp/#`, prints device status, and sends LED colors based on selected source (ultrasonic or IMU).

## 4) How did you structure the code for maintainability?
**Recommended answer:**  
I used one PlatformIO project with separate device implementations (`esp_devices/src/devices/*`) and a shared handler/common layer (`common.cpp`, `common.h`). Each environment selects its device file via `src_filter`. This avoids duplication but keeps each device logic isolated and readable.

## 5) What are the MQTT topics and payloads?
**Recommended answer:**  
Topics are `esp/<station>/status`, `esp/<station>/distance_cm`, `esp/<station>/temperature_c`, and `esp/<station>/humidity`. Payloads are numeric strings (e.g., `"23.45"`) or `"nan"` for invalid readings. LED commands use the `LED` topic with `r,g,b` strings like `"255,0,0"`.

## 6) How do you handle reconnects and outages?
**Recommended answer:**  
ESP devices periodically retry WiFi and MQTT connections without blocking. Status messages are published as retained on reconnect. The Pi hub tolerates missing messages and can switch between input sources.

## 7) How do you choose sampling and publish rates?
**Recommended answer:**  
I decoupled measurement and publish intervals. DHT runs at 1 Hz because it is slow. Ultrasonic reads at 20 Hz for responsiveness. Publishing can be less frequent than sampling to reduce traffic.

## 8) How did you debug sensor failures?
**Recommended answer:**  
I isolated the HC-SR04 with a standalone test sketch and added raw echo time logging. This made it clear whether the issue was wiring, power, or logic. I also added a GPIO loopback test to verify ESP pins.

## 9) How do you adjust the speed of sound?
**Recommended answer:**  
The ultrasonic ESP subscribes to temperature and uses `speed_mps = 331.3 + 0.606 * temp_c`. Distance is computed as `(duration_us * speed_cm_us) / 2`. The device logs the temperature and speed used.

## 10) Why a complementary filter instead of a true Kalman filter?
**Recommended answer:**  
A complementary filter is simpler, lightweight, and good enough for basic pitch/roll stabilization. A full Kalman filter would require tuning noise models and has higher complexity. For a job-ready demo, I kept it stable and readable.

## 11) How do you map sensor data to LED colors?
**Recommended answer:**  
The Pi maps distance or IMU pitch to a color spectrum using HSV-to-RGB conversion. The hue range and brightness are configurable, so the LED shows a smooth gradient rather than just red/green.

## 12) How do you keep secrets out of version control?
**Recommended answer:**  
I added a `secrets.h` include that is ignored by Git and provided a `secrets.h.example`. WiFi and broker settings are kept local to avoid leaking credentials.

## 13) What improvements would you make for production?
**Recommended answer:**  
I would add MQTT authentication/TLS, replace `pulseIn` with a non-blocking timing method, add structured logging, and provide a proper configuration layer (env files or a small config service).  

Why and how this improves the system:

- **MQTT authentication + TLS**: protects credentials and sensor data from eavesdropping and unauthorized access.  
  How: enable TLS on Mosquitto, use username/password or client certificates, and configure PubSubClient/Paho with SSL.
- **Non-blocking ultrasonic timing**: `pulseIn` blocks the loop and can delay WiFi/MQTT tasks.  
  How: use interrupts or a state machine with `micros()` to capture echo duration; this keeps the loop responsive.
- **Structured logging**: consistent logs make debugging and monitoring easier.  
  How: log JSON or tagged lines with timestamps and device IDs, and centralize logs on the Pi.
- **Configuration layer**: avoids hardcoding IPs and thresholds in code.  
  How: use `.env` or config files on the Pi, and `secrets.h` or build flags on ESPs.
- **Health checks/metrics**: detect sensor failures early.  
  How: add heartbeat topics and error counters; alert if data stops or becomes invalid.
- **OTA updates**: easier field maintenance.  
  How: add OTA support to ESP32 so firmware can be updated without USB access.

## 14) How do you validate system behavior end-to-end?
**Recommended answer:**  
I test each device in isolation, then run the Pi hub and verify topic flow with `mosquitto_sub` and `mosquitto_pub`. I also verify that device status topics update and that LED commands are logged and visible.

## 15) What’s a key challenge you solved?
**Recommended answer:**  
The biggest challenge was coordinating multiple sensors, protocols, and timing constraints. The refactor into a shared handler plus per-device implementations made the system easier to reason about and test.

Additional challenges from this project and how they were solved:

- **Serial garbage output**: baud mismatch in PlatformIO monitor.  
  Fix: switched to `monitor_speed = 115200`.
- **Wrong firmware on the wrong ESP**: multiple boards on different COM ports.  
  Fix: explicit `--upload-port` and distinct env names per device.
- **MQTT topic mismatch (Ultrasonic spelling)**: `Ulstrasonic` vs `Ultrasonic`.  
  Fix: standardized `MQTT_CLIENT_ID` and topics in config.
- **Library installs failing**: build flags mistakenly placed under `lib_deps`.  
  Fix: moved flags to `build_flags`.
- **LED not responding**: topic payload format or wrong firmware.  
  Fix: enforced `r,g,b` payloads and verified the LED ESP env upload.
- **LED brightness looked dim**: only red/green used.  
  Fix: switched to full-spectrum HSV mapping for richer color output.
- **Ultrasonic returning `nan`**: sensor echo not detected.  
  Fix: created a standalone test env and added raw echo logging to isolate wiring.
- **ESP include path issues**: wrong include style.  
  Fix: changed to `#include "device.h"` and refactored file layout.
- **Need for clearer code ownership per device**: single file was hard to track.  
  Fix: split into per-device implementations with a shared handler/common layer.

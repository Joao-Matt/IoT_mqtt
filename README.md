# IoT_mqtt_1

ESP32 + Raspberry Pi MQTT project with three ESP roles:

- **Ultrasonic**: HC-SR04 distance, temperature-compensated speed of sound.
- **Temperature**: DHT11 temperature/humidity.
- **LED**: RGB LED controlled by MQTT.

The Pi subscribes to ESP topics and can publish LED colors.

## Layout

- `esp_devices/` PlatformIO project for ESP32 firmware.
- `rpi_hub/` Raspberry Pi MQTT listener/publisher.
- `esp_devices/extras/` standalone test sketches (not built by default).

## ESP firmware (one codebase, multiple envs)

All ESPs share a common handler (`esp_devices/src/main.cpp`) and a common MQTT/WiFi layer
(`esp_devices/src/common.cpp`). Each device has its own implementation file under
`esp_devices/src/devices/`, selected per environment via `src_filter`.

## Device architecture

```
[Temperature ESP] --(esp/Temperature/temperature_c)--> [MQTT Broker] --> [Ultrasonic ESP]
       |                                                        |
       |(esp/Temperature/humidity)                               |(esp/Ultrasonic/distance_cm)
       v                                                        v
  [MQTT Broker] <------------------------------- [Raspberry Pi hub] --(LED)--> [LED ESP]
                                          ^
                                          | UDP (pitch/roll)
                                       [IMU ESP]
```

### Environments

- `esp32_Ultrasonic`
  - `MQTT_CLIENT_ID="Ultrasonic"`
  - Source: `esp_devices/src/devices/ultrasonic.cpp`
  - Subscribes to `esp/Temperature/temperature_c`
  - Publishes:
    - `esp/Ultrasonic/status` (retained)
    - `esp/Ultrasonic/distance_cm`

- `esp32_Temperature`
  - `MQTT_CLIENT_ID="Temperature"`
  - Source: `esp_devices/src/devices/temperature.cpp`
  - Publishes:
    - `esp/Temperature/status` (retained)
    - `esp/Temperature/temperature_c`
    - `esp/Temperature/humidity`

- `esp32_LED`
  - `MQTT_CLIENT_ID="LED"`
  - Source: `esp_devices/src/devices/led.cpp`
  - Subscribes to:
    - `LED` (payload `r,g,b` or `nan` for blue blink)

- `esp32_IMU`
  - `MQTT_CLIENT_ID="IMU"`
  - Source: `esp_devices/src/devices/imu.cpp`
  - Reads MPU6050 over I2C and sends pitch/roll over UDP.

### Timing

- DHT reads at **1 Hz** (`DHT_READ_PERIOD_MS=1000`)
- Ultrasonic reads at **20 Hz** (`DISTANCE_READ_PERIOD_MS=50`)
  - Echo timeout is `HCSR04_TIMEOUT_US=30000` (normal range)

### Speed of sound calculation

Ultrasonic ESP updates the speed of sound from the Temperature ESP:

```
speed_mps = 331.3 + 0.606 * temp_c
distance_cm = duration_us * (speed_mps / 10000) / 2
```

It prints the temperature and speed each time it updates, and logs distance
with the current speed in the serial monitor.

## Technologies used

- **ESP32 + Arduino framework**: device firmware and GPIO control.
- **PlatformIO**: build/upload per device environment.
- **PubSubClient (ESP32)**: MQTT publish/subscribe on microcontrollers.
- **Paho MQTT (Python)**: MQTT client on Raspberry Pi.
- **Mosquitto broker**: MQTT server on the Pi (`localhost:1883`).
- **DHT sensor library**: DHT11 temperature/humidity readings.
- **MQTT topics**: structured as `esp/<station>/<metric>` plus `LED`.
- **MPU6050 (I2C)**: IMU source for pitch/roll (Ultrasonic uses MQTT, IMU uses UDP).

## Secrets (WiFi/MQTT)

Create a local `esp_devices/include/secrets.h` (not committed):

```
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define MQTT_HOST "192.168.1.174"
#define MQTT_PORT 1883
```

Template: `esp_devices/include/secrets.h.example`

## IMU UDP settings

The IMU device sends UDP packets to the Pi. Defaults can be overridden at build time:

```
-D UDP_REMOTE_IP="192.168.1.174"
-D UDP_REMOTE_PORT=9000
```

## Build and upload (ESP32)

From `esp_devices/`:

```
pio run -e esp32_Temperature -t upload
pio run -e esp32_Ultrasonic -t upload
pio run -e esp32_LED -t upload
pio run -e esp32_IMU -t upload
```

## Raspberry Pi hub

Main script: `rpi_hub/src/main.py`

- Subscribes to `esp/#`
- Shows device status (`esp/<station>/status`)
- Lets you choose which topic to display
- Publishes LED colors based on distance (spectrum gradient)

Interactive commands:

- `list` - show topics
- `devices` - show ESPs and status
- `show <n|topic|station metric>` - select a topic
- `led` - show last LED command sent
- `led on|off` - toggle LED publish logging
- `q` - stop showing the selected topic
- `exit` - quit the program

Run on the Pi:

```
python3 /home/jacksparrow/code/new_mqtt_try.py
```

## Extras

Test sketches (standalone):

- `esp_devices/extras/led_test.cpp`
- `esp_devices/extras/ultrasonic_test.cpp`
- `esp_devices/extras/dht_test.cpp`

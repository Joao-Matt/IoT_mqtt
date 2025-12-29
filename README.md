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

All ESPs build from `esp_devices/src/main.cpp`, controlled by env flags.

### Environments

- `esp32_Ultrasonic`
  - `MQTT_CLIENT_ID="Ultrasonic"`
  - `HAS_HCSR04=1`
  - Subscribes to `esp/Temperature/temperature_c`
  - Publishes:
    - `esp/Ultrasonic/status` (retained)
    - `esp/Ultrasonic/distance_cm`

- `esp32_Temperature`
  - `MQTT_CLIENT_ID="Temperature"`
  - `HAS_DHT=1`
  - Publishes:
    - `esp/Temperature/status` (retained)
    - `esp/Temperature/temperature_c`
    - `esp/Temperature/humidity`

- `esp32_LED`
  - `MQTT_CLIENT_ID="LED"`
  - `HAS_LED=1`
  - Subscribes to:
    - `LED` (payload `r,g,b` or `nan` for blue blink)

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

## Secrets (WiFi/MQTT)

Create a local `esp_devices/include/secrets.h` (not committed):

```
#define WIFI_SSID "YOUR_WIFI_SSID"
#define WIFI_PASS "YOUR_WIFI_PASSWORD"
#define MQTT_HOST "192.168.1.174"
#define MQTT_PORT 1883
```

Template: `esp_devices/include/secrets.h.example`

## Build and upload (ESP32)

From `esp_devices/`:

```
pio run -e esp32_Temperature -t upload
pio run -e esp32_Ultrasonic -t upload
pio run -e esp32_LED -t upload
```

## Raspberry Pi hub

Main script: `rpi_hub/src/main.py`

- Subscribes to `esp/#`
- Shows device status (`esp/<station>/status`)
- Lets you choose which topic to display
- Publishes LED colors based on distance

Interactive commands:

- `list` - show topics
- `devices` - show ESPs and status
- `show <n|topic|station metric>` - select a topic
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

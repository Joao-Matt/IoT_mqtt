# rpi_hub

MQTT hub script for the Raspberry Pi. It subscribes to ESP topics, tracks device status, and can publish LED colors based on distance.

## File

- `src/main.py` main entrypoint.

## Config (inside `src/main.py`)

- `BROKER_IP`, `BROKER_PORT` MQTT broker location (defaults to `localhost:1883`).
- `LED_TOPIC` topic used to control the LED ESP.
- `DISTANCE_SOURCE_STATION` station name used for distance (`Ultrasonic`).
- `LED_MIN_CM` / `LED_MAX_CM` map distance to red/green.

## Run

```
python3 /home/jacksparrow/code/new_mqtt_try.py
```

## Commands (interactive)

- `list` show topics discovered so far
- `devices` show ESP devices and status
- `show <n|topic|station metric>` select a topic to display
- `current` show current selection
- `led` show the last LED payload sent
- `led on|off` toggle LED publish logging
- `q` stop showing the current topic
- `exit` quit

## Notes

- The script only prints payloads for the selected topic.
- It prints ESP status changes when it sees `esp/<station>/status`.
- LED commands are published to the `LED` topic and logged when enabled.

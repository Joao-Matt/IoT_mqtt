import time
import paho.mqtt.client as mqtt

# -------- MQTT config --------
BROKER_IP = "localhost"  # broker runs on this Pi
BROKER_PORT = 1883

TOPIC = "esp/#"
CLIENT_ID = "rpi4_mqtt_sub"

LED_TOPIC = "LED"
DISTANCE_SOURCE_STATION = "Ulstrasonic"
LED_MIN_CM = 5.0
LED_MAX_CM = 200.0
LED_PUBLISH_INTERVAL_SEC = 1.0

latest_by_station = {}
last_led_payload = None
last_led_publish_ts = 0.0


# -------- Callbacks --------
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("MQTT connected successfully")
        client.subscribe(TOPIC)
        print(f"Subscribed to: {TOPIC}")
    else:
        print(f"MQTT connection failed, reason_code={reason_code}")


def _format_snapshot(ts):
    station_parts = []
    for station in sorted(latest_by_station.keys()):
        metrics = latest_by_station.get(station, {})
        if metrics:
            metric_str = " ".join(
                f"{key}={value}" for key, value in sorted(metrics.items())
            )
            station_parts.append(f"{station}: {metric_str}")
        else:
            station_parts.append(f"{station}: (no data)")

    return f"[{ts}] " + " | ".join(station_parts)


def _clamp(value, low, high):
    return max(low, min(high, value))


def _distance_to_led_payload(distance_cm):
    if distance_cm is None:
        return "nan"

    if LED_MAX_CM <= LED_MIN_CM:
        return "nan"

    t = (distance_cm - LED_MIN_CM) / (LED_MAX_CM - LED_MIN_CM)
    t = _clamp(t, 0.0, 1.0)

    red = round(255 * (1.0 - t))
    green = round(255 * t)
    blue = 0
    return f"{red},{green},{blue}"


def on_message(client, userdata, msg):
    global last_led_payload
    global last_led_publish_ts
    payload = msg.payload.decode("utf-8", errors="replace")
    parts = msg.topic.split("/")

    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    if len(parts) >= 3 and parts[0] == "esp":
        station = parts[1]
        metric = "/".join(parts[2:])
        latest_by_station.setdefault(station, {})[metric] = payload
        print(_format_snapshot(ts))

        if station == DISTANCE_SOURCE_STATION and metric == "distance_cm":
            payload_clean = payload.strip()
            if payload_clean.lower() == "nan":
                led_payload = "nan"
            else:
                try:
                    distance_cm = float(payload_clean)
                except ValueError:
                    distance_cm = None
                led_payload = _distance_to_led_payload(distance_cm)

            now_monotonic = time.monotonic()
            publish_due = (
                last_led_publish_ts == 0.0
                or (now_monotonic - last_led_publish_ts) >= LED_PUBLISH_INTERVAL_SEC
            )
            if led_payload != last_led_payload or publish_due:
                client.publish(LED_TOPIC, led_payload)
                last_led_payload = led_payload
                last_led_publish_ts = now_monotonic
    else:
        print(f"[{ts}] {msg.topic} -> {payload}")


# -------- Main --------
def main():
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=CLIENT_ID,
    )

    client.on_connect = on_connect
    client.on_message = on_message

    print(f"Connecting to MQTT broker at {BROKER_IP}:{BROKER_PORT} ...")
    client.connect(BROKER_IP, BROKER_PORT, keepalive=60)

    try:
        client.loop_forever()
    except KeyboardInterrupt:
        print("\nDisconnecting...")
        client.disconnect()


if __name__ == "__main__":
    main()

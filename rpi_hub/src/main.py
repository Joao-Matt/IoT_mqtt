import re
import time
import paho.mqtt.client as mqtt

# -------- MQTT config --------
BROKER_IP = "localhost"  # broker runs on this Pi
BROKER_PORT = 1883

TOPIC = "esp/#"
CLIENT_ID = "rpi4_mqtt_sub"

LED_TOPIC = "LED"
DISTANCE_SOURCE_STATION = "Ultrasonic"
LED_MIN_CM = 5.0
LED_MAX_CM = 200.0
LED_PUBLISH_INTERVAL_SEC = 1.0

latest_by_station = {}
last_led_payload = None
last_led_publish_ts = 0.0
topics_seen = set()
stations_seen = set()
station_status = {}
selected_topic = None
initial_topic_list_printed = False


# -------- Callbacks --------
# Handle initial MQTT connection and subscribe to wildcard topic.
def on_connect(client, userdata, flags, reason_code, properties=None):
    if reason_code == 0:
        print("MQTT connected successfully")
        client.subscribe(TOPIC)
        print(f"Subscribed to: {TOPIC}")
        print("Type 'list' to see topics, 'devices' to see ESPs.")
        print("Use 'show <n|topic|station metric>' to select.")
    else:
        print(f"MQTT connection failed, reason_code={reason_code}")


# Clamp a numeric value between low/high limits.
def _clamp(value, low, high):
    return max(low, min(high, value))


# Convert distance in cm to an RGB payload string for the LED.
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


# Process incoming MQTT messages and update state/LED publishing.
def on_message(client, userdata, msg):
    global last_led_payload
    global last_led_publish_ts
    global selected_topic
    global initial_topic_list_printed
    payload = msg.payload.decode("utf-8", errors="replace")
    parts = msg.topic.split("/")

    ts = time.strftime("%Y-%m-%d %H:%M:%S")
    is_new_topic = msg.topic not in topics_seen
    topics_seen.add(msg.topic)
    if is_new_topic and not initial_topic_list_printed:
        _print_topics()
        initial_topic_list_printed = True
    if len(parts) >= 3 and parts[0] == "esp":
        station = parts[1]
        metric = "/".join(parts[2:])
        latest_by_station.setdefault(station, {})[metric] = payload
        stations_seen.add(station)
        if metric == "status":
            previous_status = station_status.get(station)
            if previous_status != payload:
                station_status[station] = payload
                print(f"[{ts}] ESP {station} status={payload}")

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

        if selected_topic and msg.topic == selected_topic:
            print(f"[{ts}] {msg.topic} -> {payload}")
    else:
        if selected_topic and msg.topic == selected_topic:
            print(f"[{ts}] {msg.topic} -> {payload}")


# Print all topics seen so far with index numbers.
def _print_topics():
    topic_list = sorted(topics_seen)
    if not topic_list:
        print("No topics seen yet.")
        return []

    print("Topics:")
    for index, topic in enumerate(topic_list, start=1):
        print(f"  {index}) {topic}")
    print("Use 'list' again to refresh as new topics appear.")
    return topic_list


# Print ESP devices and their last known status.
def _print_devices():
    if not stations_seen:
        print("No ESP devices seen yet.")
        return

    print("ESP devices:")
    for station in sorted(stations_seen):
        status = station_status.get(station, "unknown")
        print(f"  {station} ({status})")


# Match a partial name against a list of candidates.
def _match_name(value, candidates):
    value_lower = value.lower()
    exact = [c for c in candidates if c.lower() == value_lower]
    if len(exact) == 1:
        return exact[0], None

    prefix = [c for c in candidates if c.lower().startswith(value_lower)]
    if len(prefix) == 1:
        return prefix[0], None
    if len(prefix) > 1:
        return None, prefix
    return None, []


# Print available metrics for a given station.
def _print_station_metrics(station):
    metrics = sorted(latest_by_station.get(station, {}).keys())
    if not metrics:
        print(f"No metrics seen yet for {station}.")
        return

    print(f"Metrics for {station}:")
    for metric in metrics:
        print(f"  - {metric}")


# Interactive command loop for selecting and viewing topics.
def _prompt_loop():
    global selected_topic
    print("Commands: list, devices, show <n|topic|station metric>, current, q, exit")
    while True:
        try:
            command = input("> ").strip()
        except (EOFError, KeyboardInterrupt):
            break

        if not command:
            continue

        if command == "list":
            _print_topics()
        elif command == "devices":
            _print_devices()
        elif command == "q":
            selected_topic = None
            print("Topic output paused.")
        elif command.startswith("show"):
            args = command.split(maxsplit=1)
            if len(args) < 2:
                print("Usage: show <n|topic|station metric>")
                continue

            choice = args[1].strip()
            topic_list = sorted(topics_seen)
            if choice.isdigit():
                index = int(choice)
                if 1 <= index <= len(topic_list):
                    selected_topic = topic_list[index - 1]
                    print(f"Selected topic: {selected_topic}")
                else:
                    print("Invalid topic number.")
            else:
                if choice in topics_seen:
                    selected_topic = choice
                    print(f"Selected topic: {selected_topic}")
                else:
                    parts = [p for p in re.split(r"[\\s/]+", choice) if p]
                    if not parts:
                        print("Topic not seen yet. Use 'list' to see available topics.")
                        continue

                    stations = sorted(stations_seen or latest_by_station.keys())
                    station, station_matches = _match_name(parts[0], stations)
                    if station is None:
                        if station_matches:
                            print("Ambiguous station. Matches: " + ", ".join(station_matches))
                        else:
                            print("Unknown station. Use 'devices' or 'list'.")
                        continue

                    if len(parts) == 1:
                        _print_station_metrics(station)
                        continue

                    metrics = sorted(latest_by_station.get(station, {}).keys())
                    metric, metric_matches = _match_name(parts[1], metrics)
                    if metric is None:
                        if metric_matches:
                            print("Ambiguous metric. Matches: " + ", ".join(metric_matches))
                            continue
                        if metrics:
                            print("Unknown metric. Use 'show <station>' or 'list'.")
                            continue
                        metric = parts[1]

                    selected_topic = f"esp/{station}/{metric}"
                    print(f"Selected topic: {selected_topic}")
        elif command == "current":
            if selected_topic:
                print(f"Current topic: {selected_topic}")
            else:
                print("No topic selected.")
        elif command == "exit":
            break
        else:
            print("Unknown command. Try: list, devices, show <n|topic>, current, q, exit")


# -------- Main --------
# Main entrypoint: connect to MQTT and start prompt loop.
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
        client.loop_start()
        _prompt_loop()
    except KeyboardInterrupt:
        pass
    finally:
        print("\nDisconnecting...")
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()

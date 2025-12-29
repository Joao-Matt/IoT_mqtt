import re
import socket
import threading
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
LED_HUE_MIN = 0.0
LED_HUE_MAX = 240.0
LED_BRIGHTNESS = 1.0

IMU_UDP_BIND = "0.0.0.0"
IMU_UDP_PORT = 9000
IMU_PITCH_MIN = -45.0
IMU_PITCH_MAX = 45.0

latest_by_station = {}
last_led_payload = None
last_led_publish_ts = 0.0
led_log_enabled = True
led_source = "ultrasonic"
latest_imu = {"pitch": None, "roll": None}
latest_imu_ts = 0.0
topics_seen = set()
stations_seen = set()
station_status = {}
selected_topic = None
initial_topic_list_printed = False


# -------- Callbacks --------
# Ask user which sensor should drive the LED mapping.
def _prompt_led_source():
    while True:
        try:
            choice = input("LED source (imu/us) [us]: ").strip().lower()
        except EOFError:
            print("No stdin available, defaulting LED source to ultrasonic.")
            return "ultrasonic"
        if not choice or choice in ("us", "ultrasonic", "u"):
            return "ultrasonic"
        if choice in ("imu", "i"):
            return "imu"
        print("Please enter 'imu' or 'us'.")


# Listen for IMU UDP packets and publish LED updates when enabled.
def _start_imu_listener(client):
    def _imu_thread():
        sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        sock.bind((IMU_UDP_BIND, IMU_UDP_PORT))
        while True:
            data, _addr = sock.recvfrom(256)
            text = data.decode("utf-8", errors="replace").strip()
            pitch, roll = _parse_imu_payload(text)
            if pitch is None or roll is None:
                continue
            _handle_imu_update(client, pitch, roll)

    thread = threading.Thread(target=_imu_thread, daemon=True)
    thread.start()


# Parse "pitch=..,roll=.." into floats.
def _parse_imu_payload(text):
    match = re.findall(r"([-+]?[0-9]*\.?[0-9]+)", text)
    if len(match) < 2:
        return None, None
    try:
        pitch = float(match[0])
        roll = float(match[1])
    except ValueError:
        return None, None
    return pitch, roll


# Publish LED updates using IMU data if IMU mode is active.
def _handle_imu_update(client, pitch, roll):
    global latest_imu
    global latest_imu_ts
    global led_source
    global last_led_payload
    global last_led_publish_ts
    global led_log_enabled

    latest_imu = {"pitch": pitch, "roll": roll}
    latest_imu_ts = time.monotonic()
    if led_source != "imu":
        return

    led_payload = _imu_to_led_payload(pitch, roll)
    if led_payload != last_led_payload:
        client.publish(LED_TOPIC, led_payload)
        last_led_payload = led_payload
        last_led_publish_ts = latest_imu_ts
        if led_log_enabled:
            ts = time.strftime("%Y-%m-%d %H:%M:%S")
            print(f"[{ts}] LED publish -> {led_payload}")

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
    hue = LED_HUE_MIN + (LED_HUE_MAX - LED_HUE_MIN) * t
    red, green, blue = _hsv_to_rgb(hue, 1.0, LED_BRIGHTNESS)
    return f"{red},{green},{blue}"


def _imu_to_led_payload(pitch, roll):
    if pitch is None or roll is None:
        return "nan"

    t = (pitch - IMU_PITCH_MIN) / (IMU_PITCH_MAX - IMU_PITCH_MIN)
    t = _clamp(t, 0.0, 1.0)
    hue = LED_HUE_MIN + (LED_HUE_MAX - LED_HUE_MIN) * t
    red, green, blue = _hsv_to_rgb(hue, 1.0, LED_BRIGHTNESS)
    return f"{red},{green},{blue}"


def _hsv_to_rgb(hue, saturation, value):
    hue = hue % 360.0
    saturation = _clamp(saturation, 0.0, 1.0)
    value = _clamp(value, 0.0, 1.0)

    c = value * saturation
    x = c * (1.0 - abs((hue / 60.0) % 2 - 1.0))
    m = value - c

    if hue < 60.0:
        r, g, b = c, x, 0.0
    elif hue < 120.0:
        r, g, b = x, c, 0.0
    elif hue < 180.0:
        r, g, b = 0.0, c, x
    elif hue < 240.0:
        r, g, b = 0.0, x, c
    elif hue < 300.0:
        r, g, b = x, 0.0, c
    else:
        r, g, b = c, 0.0, x

    return (
        round((r + m) * 255),
        round((g + m) * 255),
        round((b + m) * 255),
    )


# Process incoming MQTT messages and update state/LED publishing.
def on_message(client, userdata, msg):
    global last_led_payload
    global last_led_publish_ts
    global selected_topic
    global initial_topic_list_printed
    global led_log_enabled
    global led_source
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

        if (
            led_source == "ultrasonic"
            and station == DISTANCE_SOURCE_STATION
            and metric == "distance_cm"
        ):
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
                if led_log_enabled:
                    print(f"[{ts}] LED publish -> {led_payload}")

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
    print(
        "Commands: list, devices, show <n|topic|station metric>, current, led, source, q, exit"
    )
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
        elif command == "led":
            if last_led_payload is None:
                print("No LED payload sent yet.")
            else:
                print(f"Last LED payload: {last_led_payload}")
        elif command.startswith("led "):
            value = command.split(maxsplit=1)[1].strip().lower()
            if value == "on":
                led_log_enabled = True
                print("LED publish logging enabled.")
            elif value == "off":
                led_log_enabled = False
                print("LED publish logging disabled.")
            else:
                print("Usage: led on | led off")
        elif command.startswith("source"):
            args = command.split(maxsplit=1)
            if len(args) < 2:
                print("Usage: source imu | source us")
                continue
            choice = args[1].strip().lower()
            if choice in ("imu", "i"):
                led_source = "imu"
                print("LED source set to IMU.")
            elif choice in ("us", "ultrasonic", "u"):
                led_source = "ultrasonic"
                print("LED source set to ultrasonic.")
            else:
                print("Usage: source imu | source us")
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
    global led_source
    client = mqtt.Client(
        mqtt.CallbackAPIVersion.VERSION2,
        client_id=CLIENT_ID,
    )

    client.on_connect = on_connect
    client.on_message = on_message

    led_source = _prompt_led_source()
    print(f"Connecting to MQTT broker at {BROKER_IP}:{BROKER_PORT} ...")
    client.connect(BROKER_IP, BROKER_PORT, keepalive=60)

    try:
        client.loop_start()
        _start_imu_listener(client)
        _prompt_loop()
    except KeyboardInterrupt:
        pass
    finally:
        print("\nDisconnecting...")
        client.loop_stop()
        client.disconnect()


if __name__ == "__main__":
    main()

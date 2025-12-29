# Interview Q&A (Code + Tech Deep Dive)

Focused questions about algorithms, libraries, and implementation details.

## 1) What does `common.cpp` do on the ESP?
**Answer:**  
Think of `common.cpp` as the shared toolbox for all ESP devices. Step by step:
- It creates the **WiFi client** and **MQTT client** once, so every device uses the same network code.
- `connectWifiTimed()` tries to connect to WiFi and gives up after a timeout so the code does not get stuck forever.
- `connectMqttTimed()` connects to the MQTT broker the same way, with a timeout.
- `setupMqttServer()` tells the MQTT client which broker IP/port to use.
- `publishFloat()` turns a number into a string and publishes it (MQTT only sends text).
- `payloadIsNan()` checks if a received string is `"nan"` and treats it as an invalid value.

This keeps every device file small and focused on its **sensor logic**, not networking.

## 2) How do you structure the ESP code?
**Answer:**  
The structure is designed so each ESP device has its own file:
1) `esp_devices/src/main.cpp` is the **main entry point**. It calls:
   - `deviceSetup()` once at boot
   - `deviceLoop()` repeatedly
2) Each device has its own file under `esp_devices/src/devices/`:
   - `ultrasonic.cpp`
   - `temperature.cpp`
   - `led.cpp`
   - `imu.cpp`
3) PlatformIO’s `src_filter` chooses **which device file** to compile for each environment.

This way, each ESP gets only the code it needs, and it is easier to track.

## 3) How is ultrasonic distance computed?
**Answer:**  
Step by step:
1) The ESP sends a 10 microsecond pulse to the TRIG pin.
2) The sensor sends an ultrasonic wave, then raises ECHO pin HIGH until it hears the echo.
3) `pulseIn()` measures how long ECHO stayed HIGH. This is the **round‑trip time**.
4) Distance = (time * speed) / 2 because the sound goes to the object **and back**.
5) Speed is converted into **cm per microsecond** so the math matches the time units.

## 4) How is the speed of sound corrected?
**Answer:**  
Sound travels faster in warmer air. So we do:
1) The Temperature ESP publishes current temperature (Celsius).
2) The Ultrasonic ESP subscribes to that temperature topic.
3) It uses a formula:
   - `speed_mps = 331.3 + 0.606 * temp_c`
4) That speed is converted into cm/us and used in distance calculation.

This makes distance more accurate in hot or cold rooms.

## 5) What does the “Kalman” filter do here?
**Answer:**  
In simple terms, the filter **combines two sensors**:
- The gyroscope is smooth but drifts over time.
- The accelerometer is stable long‑term but noisy.

The code uses a **complementary filter** (not a full Kalman). It does:
1) Predict angle from gyro: `angle + gyro * dt`
2) Correct with accelerometer angle
3) Blend using `alpha` (for example 0.98):
   - `new_angle = 0.98 * gyro_estimate + 0.02 * accel_angle`

So you get a stable angle without complex math.

## 6) Which libraries are used and why?
**Answer:**  
Each library does one job:
- `PubSubClient`: MQTT messages on ESP32 (publish/subscribe).
- `Wire`: I2C communication for MPU6050.
- `DHT sensor library`: handles DHT11 timing and data decode.
- `WiFi`: connects the ESP32 to your router.
- `WiFiUDP`: sends IMU data by UDP packets.

We use these to avoid writing low‑level drivers from scratch.

## 7) How does the LED color mapping work?
**Answer:**  
Step by step:
1) Take a **distance** or **pitch** value.
2) Normalize it into a 0.0–1.0 range.
3) Map that range into a **hue** (0 = red, 120 = green, 240 = blue).
4) Convert HSV → RGB.
5) Publish `r,g,b` to the LED topic.

This makes the LED smoothly change color instead of jumping.

## 8) How do you avoid flooding MQTT with LED commands?
**Answer:**  
The Pi only sends a new LED command when:
- The color actually changed, **or**
- A minimum time interval passed (heartbeat).

This keeps the network quiet and prevents spamming the LED device.

## 9) What are the MQTT topic conventions?
**Answer:**  
Topics are structured like folders:
- `esp/<station>/<metric>`

So for example:
- `esp/Ultrasonic/distance_cm`
- `esp/Temperature/temperature_c`
- `esp/Temperature/humidity`
- `esp/LED/status`

The LED control uses a simple topic named `LED`.

## 10) How do you handle reconnects?
**Answer:**  
The main loop checks connection state regularly:
1) If WiFi is disconnected → try reconnect.
2) If MQTT is disconnected → try reconnect.
3) After MQTT connects → publish `status=online` (retained).
4) Re‑subscribe to needed topics (LED or temperature).

This keeps devices alive even if WiFi drops.

## 11) What’s the purpose of `ultrasonic_test.cpp`?
**Answer:**  
It is a **hardware debugging tool**:
- No MQTT.
- No other sensors.
- Just trig/echo and serial output.

It prints:
- `echo_us` (raw pulse time)
- `distance_cm` (computed)

If `echo_us=0`, then wiring or sensor power is wrong.

## 12) How does the IMU send data?
**Answer:**  
The IMU device:
1) Reads raw accelerometer + gyro data.
2) Computes pitch/roll.
3) Formats a string like `pitch=12.34,roll=-5.67`.
4) Sends it in a UDP packet to the Pi on port 9000.

UDP is used because it is fast and low overhead.

## 13) What happens if IMU packets drop?
**Answer:**  
If a UDP packet is lost:
- The Pi just waits for the next one.
- The LED may hold the last color briefly.
- There is no crash because each packet is independent.

## 14) Why use `src_filter` in PlatformIO?
**Answer:**  
`src_filter` tells PlatformIO:
- Which files to compile for each environment.

So the Ultrasonic build does not include IMU or LED code.
This makes builds faster and avoids unused code in the binary.

## 15) How would you convert the complementary filter into a true Kalman filter?
**Answer:**  
A true Kalman filter needs:
1) A **state model** (angles, gyro bias).
2) A **covariance matrix** that estimates uncertainty.
3) **Process noise** (how much the model can drift).
4) **Measurement noise** (how noisy sensors are).

In practice, you would:
- Use a Kalman/EKF library.
- Replace `SimpleKalman` with a Kalman filter class.
- Collect real sensor data and tune noise parameters.

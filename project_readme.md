# Autonomous Dual-ESP32-S3 Mapping & Obstacle-Avoidance Robot

This repository contains the embedded firmware for a dual-microcontroller autonomous mobile robot built on **ESP32-S3** boards. The system separates autonomous navigation and obstacle avoidance from geo-tagged stereo/panoramic visual data collection.

---

## System Architecture

The robot operates using two independent ESP32-S3 nodes:

1. **Vision & Telemetry Node (`ESP32_S3_CAM_GPS.ino`)**:
   - Acquires real-time GPS coordinates.
   - Rotates a pan-servo to capture **Right** ($10^\circ$) and **Left** ($170^\circ$) JPEG images per session.
   - Uploads the geo-tagged images via HTTP `POST` to a remote FastAPI backend server over Wi-Fi.
2. **Navigation & Motor Control Node (`navigation.ino`)**:
   - Executes a periodic stop-and-go exploration cycle (2 seconds forward, 3 seconds pause) to allow clear, motion-blur-free image capture.
   - Monitors front clearance using an HC-SR04 ultrasonic sensor mounted on a scanning servo and executes autonomous obstacle avoidance maneuvers.

---

## Hardware & Pinout Configuration

### 1. Vision & Telemetry Node (`ESP32_S3_CAM_GPS.ino`)
*Requires an ESP32-S3 board with an OV3666 camera module (PSRAM strongly recommended for VGA resolution).*

| Component | Pin Name / Function | ESP32-S3 GPIO |
| :--- | :--- | :--- |
| **GPS Module (UART1)** | `GPS_RX_PIN` | GPIO 1 |
| | `GPS_TX_PIN` | GPIO 2 |
| **Pan Servo** | `SERVO_PIN` | GPIO 14 |
| **Camera (OV3666)** | `XCLK` | GPIO 15 |
| | `SIOD` (SDA) / `SIOC` (SCL) | GPIO 4 / GPIO 5 |
| | `VSYNC` / `HREF` / `PCLK` | GPIO 6 / GPIO 7 / GPIO 13 |
| | `Y2` – `Y9` (Data Bus) | GPIO 11, 9, 8, 10, 12, 18, 17, 16 |
| | `PWDN` / `RESET` | `-1` (Unused) |

### 2. Navigation Node (`navigation.ino`)

| Component | Pin Name / Function | ESP32-S3 GPIO |
| :--- | :--- | :--- |
| **L298N Motor Driver** | `ENA` (Left PWM) | GPIO 2 |
| | `IN1` / `IN2` | GPIO 3 / GPIO 4 |
| | `IN3` / `IN4` | GPIO 5 / GPIO 6 |
| | `ENB` (Right PWM) | GPIO 7 |
| **HC-SR04 Ultrasonic** | `TRIG_PIN` | GPIO 12 |
| | `ECHO_PIN` | GPIO 13 |
| **Scanner Servo** | `SERVO_PIN` | GPIO 18 |

---

## Software Dependencies

Install the following libraries via the Arduino Library Manager or PlatformIO before compiling:

- **ESP32 Board Package** (Espressif Systems, v2.0.x or v3.0.x)
- **`esp_camera`** (Included with the ESP32 core)
- **`WiFi` & `HTTPClient`** (Included with the ESP32 core)
- **[TinyGPSPlus](https://github.com/mikalhart/TinyGPSPlus)** by Mikal Hart (for NMEA GPS parsing)
- **[ESP32Servo](https://github.com/madhephaestus/ESP32Servo)** by Kevin Harrington

---

## Configuration & Setup

### 1. Configure Network & Backend (`ESP32_S3_CAM_GPS.ino`)
Update the Wi-Fi credentials and your FastAPI server's local IPv4 address at the top of `ESP32_S3_CAM_GPS.ino`:

```cpp
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* SERVER_URL = "http://<YOUR_SERVER_IP>:8000/upload";
```

> **Note:** Ensure **PSRAM** (`OPI PSRAM`) is enabled in your Arduino IDE **Tools** menu. If PSRAM is not detected, the firmware automatically falls back to `FRAMESIZE_QQVGA` in internal DRAM.

### 2. Tune Navigation Parameters (`navigation.ino`)
You can calibrate motor speed, detection thresholds, and turning durations in `navigation.ino`:

- `MOTOR_SPEED`: PWM duty cycle (`0–255`, default `220`).
- `OBSTACLE_DISTANCE`: Trigger threshold in centimeters (default `30` cm).
- `FORWARD_TIME` / `STOP_TIME`: Movement burst (`2000` ms) and stationary imaging window (`3000` ms).
- `TURN_TIME` / `TURN_180_TIME`: Duration in milliseconds for $\approx 90^\circ$ and $\approx 180^\circ$ turns (default `900` ms).

---

## API Payload Specification

For each capture session, `ESP32_S3_CAM_GPS.ino` locks onto a single valid GPS fix (`age < 5000 ms`) and sends two sequential HTTP `POST` requests (`direction=right` and `direction=left`) with raw JPEG binary data (`Content-Type: image/jpeg`) and query parameters:

```http
POST /upload?latitude=30.1798000&longitude=66.9750000&session_id=session_1&direction=right HTTP/1.1
Content-Type: image/jpeg
```

| Query Parameter | Type | Description |
| :--- | :--- | :--- |
| `latitude` | `float` | GPS latitude formatted to 7 decimal places |
| `longitude` | `float` | GPS longitude formatted to 7 decimal places |
| `session_id` | `string` | Sequential identifier grouping the left/right pair (`session_1`, `session_2`, ...) |
| `direction` | `string` | Camera orientation (`right` or `left`) |

---

## Operational Workflow

1. **Initialization**: Both ESP32-S3 boards boot at `115200` baud. The camera node verifies PSRAM, initializes UART1 (`9600` baud) for GPS, homes its servo to $10^\circ$, initializes the OV3666 sensor, and connects to Wi-Fi.
2. **Navigation Cycle**: The rover drives forward for 2 seconds and halts for 3 seconds. If an object comes within `30 cm` at any point, the rover stops, reverses for `350 ms`, scans right ($10^\circ$) and left ($170^\circ$), turns toward the clearer path (or executes a $180^\circ$ turn if both sides are blocked), and resets its movement timer.
3. **Capture & Upload Cycle**: Continuously in the background, the camera node waits for a fresh GPS coordinate, captures and uploads the right-facing image (up to 3 retry attempts), pans to $170^\circ$, captures and uploads the left-facing image, and returns to $10^\circ$.
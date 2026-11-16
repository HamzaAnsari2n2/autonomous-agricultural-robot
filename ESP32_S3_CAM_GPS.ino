#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <TinyGPSPlus.h>
#include <ESP32Servo.h>

// =================================================
// 1. WIFI SETTINGS
// =================================================

// Enter your Wi-Fi details here
const char* WIFI_SSID = "Redmi 13C";
const char* WIFI_PASSWORD = "hamza1112";

// Enter your laptop's current IPv4 address
const char* SERVER_URL = "http://10.38.49.5:8000/upload";

// =================================================
// 2. GPS SETTINGS
// =================================================

#define GPS_RX_PIN 1
#define GPS_TX_PIN 2

HardwareSerial GPSSerial(1);
TinyGPSPlus gps;

// =================================================
// 3. SERVO SETTINGS
// =================================================

#define SERVO_PIN 14

Servo cameraServo;

// Adjust these angles according to your camera mounting
const int RIGHT_ANGLE = 10;
const int LEFT_ANGLE  = 170;

// =================================================
// 4. CAMERA PIN CONFIGURATION - OV3666
// Keep your existing camera pin configuration
// =================================================

#define PWDN_GPIO_NUM  -1
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM  15
#define SIOD_GPIO_NUM  4
#define SIOC_GPIO_NUM  5

#define Y9_GPIO_NUM    16
#define Y8_GPIO_NUM    17
#define Y7_GPIO_NUM    18
#define Y6_GPIO_NUM    12
#define Y5_GPIO_NUM    10
#define Y4_GPIO_NUM    8
#define Y3_GPIO_NUM    9
#define Y2_GPIO_NUM    11

#define VSYNC_GPIO_NUM 6
#define HREF_GPIO_NUM  7
#define PCLK_GPIO_NUM  13

// =================================================
// 5. SESSION SETTINGS
// =================================================

unsigned long sessionCounter = 0;

// =================================================
// 6. PSRAM AND MEMORY DIAGNOSTICS
// =================================================

void printMemoryInformation() {

  Serial.println();
  Serial.println("========== ESP32-S3 MEMORY ==========");

  Serial.print("PSRAM found: ");
  Serial.println(psramFound() ? "YES" : "NO");

  Serial.print("PSRAM size: ");
  Serial.print(ESP.getPsramSize());
  Serial.println(" bytes");

  Serial.print("Free PSRAM: ");
  Serial.print(ESP.getFreePsram());
  Serial.println(" bytes");

  Serial.print("Free heap: ");
  Serial.print(ESP.getFreeHeap());
  Serial.println(" bytes");

  Serial.println("=====================================");
  Serial.println();
}

// =================================================
// 7. INITIALIZE CAMERA WITH PSRAM
// =================================================

bool initializeCamera() {

  camera_config_t config = {};

  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;

  // Camera data pins
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;

  // Camera control pins
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;

  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;

  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;

  // Camera settings
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  config.fb_count = 1;

  // Select camera memory settings
  if (psramFound()) {

    Serial.println("PSRAM detected.");
    Serial.println("Using PSRAM for camera frame buffer.");

    config.frame_size = FRAMESIZE_VGA;
    config.jpeg_quality = 12;

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_LATEST;

  } else {

    Serial.println("WARNING: PSRAM not detected.");
    Serial.println("Using internal RAM.");
    Serial.println("Camera resolution reduced to QQVGA.");

    config.frame_size = FRAMESIZE_QQVGA;
    config.jpeg_quality = 20;

    config.fb_location = CAMERA_FB_IN_DRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  }

  Serial.println();
  Serial.println("Initializing camera...");

  esp_err_t result = esp_camera_init(&config);

  if (result != ESP_OK) {

    Serial.printf(
      "Camera initialization failed: 0x%x\n",
      result
    );

    Serial.print("Free heap after camera error: ");
    Serial.println(ESP.getFreeHeap());

    Serial.print("Free PSRAM after camera error: ");
    Serial.println(ESP.getFreePsram());

    return false;
  }

  Serial.println("Camera initialized successfully.");

  printMemoryInformation();

  return true;
}

// =================================================
// 8. CONNECT TO WIFI
// =================================================

void connectWiFi() {

  Serial.println();
  Serial.println("Connecting to WiFi...");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  while (WiFi.status() != WL_CONNECTED) {

    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.println("WiFi connected.");

  Serial.print("ESP32 IP address: ");
  Serial.println(WiFi.localIP());
}

// =================================================
// 9. GET GPS COORDINATES
// =================================================

bool getGPSCoordinates(
  double &latitude,
  double &longitude
) {

  Serial.println();
  Serial.println("Waiting for valid GPS location...");

  while (true) {

    while (GPSSerial.available()) {
      gps.encode(GPSSerial.read());
    }

    if (
      gps.location.isValid() &&
      gps.location.age() < 5000
    ) {

      latitude = gps.location.lat();
      longitude = gps.location.lng();

      Serial.println("GPS location received.");

      Serial.print("Latitude: ");
      Serial.println(latitude, 7);

      Serial.print("Longitude: ");
      Serial.println(longitude, 7);

      return true;
    }

    delay(10);
  }
}

// =================================================
// 10. CAPTURE IMAGE
// =================================================

camera_fb_t* captureImage(const char* direction) {

  Serial.println();
  Serial.print("Capturing ");
  Serial.print(direction);
  Serial.println(" image...");

  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {

    Serial.println("Camera capture failed.");

    return nullptr;
  }

  Serial.print("Image size: ");
  Serial.print(fb->len);
  Serial.println(" bytes");

  return fb;
}

// =================================================
// 11. UPLOAD IMAGE TO FASTAPI
// =================================================

bool uploadImage(
  camera_fb_t* fb,
  const char* direction,
  double latitude,
  double longitude,
  String sessionID
) {

  if (!fb) {
    return false;
  }

  if (WiFi.status() != WL_CONNECTED) {

    Serial.println("WiFi disconnected. Reconnecting...");
    connectWiFi();
  }

  String url = String(SERVER_URL);

  url += "?latitude=" + String(latitude, 7);
  url += "&longitude=" + String(longitude, 7);
  url += "&session_id=" + sessionID;
  url += "&direction=" + String(direction);

  Serial.println();
  Serial.print("Uploading ");
  Serial.print(direction);
  Serial.println(" image...");

  bool uploaded = false;

  for (int attempt = 1; attempt <= 3; attempt++) {

    Serial.print("Upload attempt: ");
    Serial.println(attempt);

    HTTPClient http;

    http.setTimeout(15000);

    if (!http.begin(url)) {

      Serial.println("Could not start HTTP connection.");

      delay(1500);
      continue;
    }

    http.addHeader("Content-Type", "image/jpeg");

    int httpCode = http.POST(fb->buf, fb->len);

    if (httpCode > 0) {

      Serial.print("HTTP response: ");
      Serial.println(httpCode);

      if (httpCode >= 200 && httpCode < 300) {

        Serial.print(direction);
        Serial.println(" image uploaded successfully.");

        uploaded = true;
      }

    } else {

      Serial.print("Upload error: ");
      Serial.println(http.errorToString(httpCode));
    }

    http.end();

    if (uploaded) {
      break;
    }

    delay(1500);
  }

  return uploaded;
}

// =================================================
// 12. COMPLETE CAMERA CYCLE
// =================================================

void runCameraCycle() {

  sessionCounter++;

  String sessionID = "session_" + String(sessionCounter);

  Serial.println();
  Serial.println("================================");
  Serial.println("STARTING NEW CAMERA CYCLE");
  Serial.println("================================");

  Serial.print("Session ID: ");
  Serial.println(sessionID);

  // STEP 1: Get one GPS coordinate
  // Both images will use this same coordinate.

  double latitude;
  double longitude;

  if (!getGPSCoordinates(latitude, longitude)) {

    Serial.println("GPS location unavailable.");
    return;
  }

  // STEP 2: Move camera to RIGHT position

  Serial.println();
  Serial.println("Moving camera to RIGHT position...");

  cameraServo.write(RIGHT_ANGLE);
  delay(800);

  // STEP 3: Capture RIGHT image

  camera_fb_t* rightImage = captureImage("RIGHT");

  bool rightUploaded = false;

  if (rightImage) {

    rightUploaded = uploadImage(
      rightImage,
      "right",
      latitude,
      longitude,
      sessionID
    );

    esp_camera_fb_return(rightImage);
  }

  // STEP 4: Rotate camera to LEFT position

  Serial.println();
  Serial.println("Rotating camera to LEFT position...");

  cameraServo.write(LEFT_ANGLE);
  delay(1000);

  // STEP 5: Capture LEFT image

  camera_fb_t* leftImage = captureImage("LEFT");

  bool leftUploaded = false;

  if (leftImage) {

    leftUploaded = uploadImage(
      leftImage,
      "left",
      latitude,
      longitude,
      sessionID
    );

    esp_camera_fb_return(leftImage);
  }

  // STEP 6: Return camera to RIGHT position

  Serial.println();
  Serial.println("Returning camera to RIGHT position...");

  cameraServo.write(RIGHT_ANGLE);
  delay(1000);

  // STEP 7: Show upload results

  Serial.println();
  Serial.println("========== UPLOAD RESULTS ==========");

  Serial.print("Right image: ");
  Serial.println(rightUploaded ? "SUCCESS" : "FAILED");

  Serial.print("Left image: ");
  Serial.println(leftUploaded ? "SUCCESS" : "FAILED");

  if (rightUploaded && leftUploaded) {

    Serial.println();
    Serial.println("BOTH IMAGES UPLOADED SUCCESSFULLY.");

    Serial.print("Session ID: ");
    Serial.println(sessionID);

    Serial.print("Latitude: ");
    Serial.println(latitude, 7);

    Serial.print("Longitude: ");
    Serial.println(longitude, 7);

  } else {

    Serial.println();
    Serial.println("WARNING: CAMERA CYCLE INCOMPLETE.");
    Serial.println("Check WiFi, FastAPI and upload errors.");
  }

  Serial.println("================================");
}

// =================================================
// 13. SETUP
// =================================================

void setup() {

  Serial.begin(115200);
  delay(1000);

  Serial.println();
  Serial.println("ESP32-S3-CAM SYSTEM STARTING");

  // STEP 1: Check PSRAM and available memory

  printMemoryInformation();

  // STEP 2: Start GPS UART

  GPSSerial.begin(
    9600,
    SERIAL_8N1,
    GPS_RX_PIN,
    GPS_TX_PIN
  );

  Serial.println("GPS UART initialized.");

  // STEP 3: Initialize servo

  cameraServo.setPeriodHertz(50);

  cameraServo.attach(
    SERVO_PIN,
    500,
    2400
  );

  cameraServo.write(RIGHT_ANGLE);

  delay(1000);

  Serial.println("Servo initialized.");

  // STEP 4: Initialize camera

  if (!initializeCamera()) {

    Serial.println();
    Serial.println("CAMERA INITIALIZATION FAILED.");
    Serial.println("Check PSRAM settings and camera connections.");

    while (true) {
      delay(1000);
    }
  }

  // STEP 5: Connect WiFi

  connectWiFi();

  Serial.println();
  Serial.println("ESP32-S3-CAM READY.");
}

// =================================================
// 14. MAIN LOOP
// =================================================

void loop() {

  runCameraCycle();

  // Short pause before the next camera cycle.
  // This is NOT the robot's movement timer.

  delay(1000);
}

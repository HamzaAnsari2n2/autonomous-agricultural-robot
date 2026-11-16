
#include <ESP32Servo.h>

// ---------- PIN CONNECTIONS ----------

// Servo
#define SERVO_PIN 18

// HC-SR04 ultrasonic sensor
#define TRIG_PIN 12
#define ECHO_PIN 13

// L298N motor driver
#define ENA 2
#define IN1 3
#define IN2 4
#define IN3 5
#define IN4 6
#define ENB 7

// ---------- SETTINGS ----------

const float OBSTACLE_DISTANCE = 25.0;

const int CENTER_ANGLE = 90;
const int RIGHT_ANGLE = 30;
const int LEFT_ANGLE = 150;

const unsigned long FORWARD_TIME = 5000;
const unsigned long STOP_TIME = 3000;
const unsigned long TURN_TIME = 700;

Servo scannerServo;

// ---------- ULTRASONIC SENSOR ----------

float getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  long duration = pulseIn(ECHO_PIN, HIGH, 30000);

  if (duration == 0) {
    return 400.0;
  }

  return duration * 0.0343 / 2.0;
}

// ---------- MOTOR CONTROL ----------

void moveForward() {
  digitalWrite(ENA, HIGH);
  digitalWrite(ENB, HIGH);

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
}

void stopRobot() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void turnRight() {
  // Left motor forward, right motor backward
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  delay(TURN_TIME);
  stopRobot();
}

void turnLeft() {
  // Left motor backward, right motor forward
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  delay(TURN_TIME);
  stopRobot();
}

// ---------- OBSTACLE AVOIDANCE ----------

void avoidObstacle() {
  stopRobot();

  Serial.println("Obstacle detected. Scanning...");

  // Scan right
  scannerServo.write(RIGHT_ANGLE);
  delay(700);
  float rightDistance = getDistance();

  // Scan left
  scannerServo.write(LEFT_ANGLE);
  delay(700);
  float leftDistance = getDistance();

  // Return sensor to center
  scannerServo.write(CENTER_ANGLE);
  delay(300);

  Serial.print("Right distance: ");
  Serial.print(rightDistance);
  Serial.println(" cm");

  Serial.print("Left distance: ");
  Serial.print(leftDistance);
  Serial.println(" cm");

  bool rightClear = rightDistance > OBSTACLE_DISTANCE;
  bool leftClear = leftDistance > OBSTACLE_DISTANCE;

  if (rightClear && (!leftClear || rightDistance >= leftDistance)) {
    Serial.println("Turning right");
    turnRight();
  }
  else if (leftClear) {
    Serial.println("Turning left");
    turnLeft();
  }
  else {
    Serial.println("Both sides blocked. Staying stopped.");

    // Wait until a side becomes clear
    while (true) {
      delay(500);

      scannerServo.write(RIGHT_ANGLE);
      delay(600);
      rightDistance = getDistance();

      scannerServo.write(LEFT_ANGLE);
      delay(600);
      leftDistance = getDistance();

      scannerServo.write(CENTER_ANGLE);
      delay(300);

      rightClear = rightDistance > OBSTACLE_DISTANCE;
      leftClear = leftDistance > OBSTACLE_DISTANCE;

      if (rightClear || leftClear) {
        break;
      }
    }

    if (rightClear && (!leftClear || rightDistance >= leftDistance)) {
      turnRight();
    } else {
      turnLeft();
    }
  }

  Serial.println("Obstacle avoided. Restarting cycle.");
}

// ---------- SETUP ----------

void setup() {
  Serial.begin(115200);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  stopRobot();

  scannerServo.setPeriodHertz(50);
  scannerServo.attach(SERVO_PIN, 500, 2400);
  scannerServo.write(CENTER_ANGLE);

  delay(1000);

  Serial.println("Agriculture Robot Started");
}

// ---------- MAIN LOOP ----------

void loop() {
  // STEP 1: Move forward for 5 seconds
  Serial.println("Moving forward for 5 seconds");

  unsigned long startTime = millis();
  moveForward();

  while (millis() - startTime < FORWARD_TIME) {
    float distance = getDistance();

    Serial.print("Front distance: ");
    Serial.print(distance);
    Serial.println(" cm");

    if (distance <= OBSTACLE_DISTANCE) {
      stopRobot();

      avoidObstacle();

      // Restart a fresh 5-second forward cycle
      return;
    }

    delay(100);
  }

  // STEP 2: Stop for 3 seconds and do nothing
  stopRobot();
  Serial.println("Stopped for 3 seconds");

  delay(STOP_TIME);

  // loop() automatically starts the next cycle
}

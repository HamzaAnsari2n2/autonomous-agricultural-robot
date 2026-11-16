
#include <ESP32Servo.h>

// =====================================
// L298N MOTOR DRIVER PINS
// =====================================

#define ENA 2
#define IN1 3
#define IN2 4
#define IN3 5
#define IN4 6
#define ENB 7

// =====================================
// HC-SR04 ULTRASONIC SENSOR
// =====================================

#define TRIG_PIN 12
#define ECHO_PIN 13

// =====================================
// SERVO MOTOR
// =====================================

#define SERVO_PIN 18

Servo scanServo;

// =====================================
// SETTINGS
// =====================================

const int MOTOR_SPEED = 220;
const int OBSTACLE_DISTANCE = 30;

const int SERVO_RIGHT = 10;
const int SERVO_CENTER = 90;
const int SERVO_LEFT = 170;

// Normal movement cycle
const unsigned long FORWARD_TIME = 2000;
const unsigned long STOP_TIME = 3000;

// Obstacle avoidance timing
const unsigned long REVERSE_TIME = 350;
const unsigned long TURN_TIME = 900;
const unsigned long TURN_180_TIME = 900;

// =====================================
// MOVEMENT STATES
// =====================================

enum CycleState {
  MOVING_FORWARD,
  STOPPING
};

CycleState currentState = MOVING_FORWARD;

unsigned long cycleStart = 0;

// =====================================
// MOTOR FUNCTIONS
// =====================================

void stopCar() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
}

void forward() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

void backward() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

// Original turnRight command physically turns LEFT
void turnRight() {
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

// Original turnLeft command physically turns RIGHT
void turnLeft() {
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);

  analogWrite(ENA, MOTOR_SPEED);
  analogWrite(ENB, MOTOR_SPEED);
}

// =====================================
// ULTRASONIC DISTANCE
// =====================================

// Returns -1 if no valid echo is received
int getDistance() {
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(3);

  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);

  unsigned long duration =
    pulseIn(ECHO_PIN, HIGH, 25000);

  if (duration == 0) {
    return -1;
  }

  return duration * 0.034 / 2;
}

// =====================================
// SERVO SCANNING
// =====================================

int scanRight() {
  Serial.println("Scanning RIGHT at 10 degrees");

  scanServo.write(SERVO_RIGHT);
  delay(500);

  int rightDistance = getDistance();

  Serial.print("Right distance: ");
  Serial.print(rightDistance);
  Serial.println(" cm");

  scanServo.write(SERVO_CENTER);
  delay(300);

  return rightDistance;
}

int scanLeft() {
  Serial.println("Scanning LEFT at 170 degrees");

  scanServo.write(SERVO_LEFT);
  delay(500);

  int leftDistance = getDistance();

  Serial.print("Left distance: ");
  Serial.print(leftDistance);
  Serial.println(" cm");

  scanServo.write(SERVO_CENTER);
  delay(300);

  return leftDistance;
}

// =====================================
// RESET MOVEMENT CYCLE
// =====================================

void resetCycle() {
  stopCar();

  currentState = MOVING_FORWARD;
  cycleStart = millis();

  Serial.println("Obstacle avoidance completed");
  Serial.println("Starting fresh 2-second forward cycle");
}

// =====================================
// OBSTACLE AVOIDANCE
// =====================================

void avoidObstacle() {

  Serial.println();
  Serial.println("OBSTACLE DETECTED!");

  // Step 1: Stop
  stopCar();
  delay(200);

  // Step 2: Reverse
  Serial.println("Reversing for 350 ms");

  backward();
  delay(REVERSE_TIME);

  stopCar();
  delay(200);

  // Step 3: Scan RIGHT first
  int rightDistance = scanRight();

  // Step 4: Scan LEFT second
  int leftDistance = scanLeft();

  // Invalid readings are treated as blocked
  bool rightClear =
    rightDistance > OBSTACLE_DISTANCE;

  bool leftClear =
    leftDistance > OBSTACLE_DISTANCE;

  Serial.println("Comparing both sides...");

  Serial.print("Right distance: ");
  Serial.println(rightDistance);

  Serial.print("Left distance: ");
  Serial.println(leftDistance);

  // =================================
  // BOTH SIDES BLOCKED
  // =================================

  if (!rightClear && !leftClear) {

    Serial.println("Both sides blocked");
    Serial.println("Turning approximately 180 degrees");

    // Physical LEFT turn
    turnRight();

    delay(TURN_180_TIME);
  }

  // =================================
  // ONLY RIGHT SIDE CLEAR
  // =================================

  else if (rightClear && !leftClear) {

    Serial.println("More space on RIGHT");
    Serial.println("Turning physically RIGHT");

    // Swapped command: physically turns RIGHT
    turnLeft();

    delay(TURN_TIME);
  }

  // =================================
  // ONLY LEFT SIDE CLEAR
  // =================================

  else if (leftClear && !rightClear) {

    Serial.println("More space on LEFT");
    Serial.println("Turning physically LEFT");

    // Swapped command: physically turns LEFT
    turnRight();

    delay(TURN_TIME);
  }

  // =================================
  // BOTH SIDES CLEAR
  // =================================

  else {

    if (rightDistance > leftDistance) {

      Serial.println("More space on RIGHT");

      turnLeft();
    }

    else if (leftDistance > rightDistance) {

      Serial.println("More space on LEFT");

      turnRight();
    }

    else {

      Serial.println("Equal distances - choosing RIGHT");

      turnLeft();
    }

    delay(TURN_TIME);
  }

  // =================================
  // STOP AFTER TURNING
  // =================================

  stopCar();

  scanServo.write(SERVO_CENTER);
  delay(300);

  // Start fresh 2-second forward cycle
  resetCycle();
}

// =====================================
// SETUP
// =====================================

void setup() {

  Serial.begin(115200);

  pinMode(ENA, OUTPUT);
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);

  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);

  scanServo.setPeriodHertz(50);
  scanServo.attach(SERVO_PIN, 500, 2400);

  // Servo faces FRONT at startup
  scanServo.write(SERVO_CENTER);
  delay(700);

  stopCar();

  currentState = MOVING_FORWARD;
  cycleStart = millis();

  Serial.println();
  Serial.println("ESP32-S3 ROBOT STARTED");
  Serial.println("Servo facing FRONT");
  Serial.println("Forward: 2 seconds");
  Serial.println("Stop: 3 seconds");
}

// =====================================
// MAIN LOOP
// =====================================

void loop() {

  // Keep servo facing FRONT
  // Check only front distance
  int frontDistance = getDistance();

  if (frontDistance != -1) {

    Serial.print("Front distance: ");
    Serial.print(frontDistance);
    Serial.println(" cm");

    // Obstacle detected within 20 cm
    if (frontDistance <= OBSTACLE_DISTANCE) {

      avoidObstacle();
      return;
    }
  }

  else {

    Serial.println("Front sensor: No valid echo");
  }

  unsigned long elapsed = millis() - cycleStart;

  // =================================
  // FORWARD FOR 2 SECONDS
  // =================================

  if (currentState == MOVING_FORWARD) {

    if (elapsed < FORWARD_TIME) {

      forward();
    }

    else {

      stopCar();

      currentState = STOPPING;
      cycleStart = millis();

      Serial.println("Forward completed");
      Serial.println("Stopping for 3 seconds");
    }
  }

  // =================================
  // STOP FOR 3 SECONDS
  // =================================

  else if (currentState == STOPPING) {

    stopCar();

    if (elapsed >= STOP_TIME) {

      currentState = MOVING_FORWARD;
      cycleStart = millis();

      Serial.println("Stop completed");
      Serial.println("Moving forward for 2 seconds");
    }
  }

  delay(50);
}

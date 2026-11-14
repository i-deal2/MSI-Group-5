// Motor pin definitions
const int EN1 = 5;  // PWM pin for speed control Motor 1
const int EN2 = 6;  // PWM pin for speed control Motor 2
const int DIR1 = 4; // Direction Motor 1
const int DIR2 = 7; // Direction Motor 2

void setup() {
  pinMode(EN1, OUTPUT);
  pinMode(EN2, OUTPUT);
  pinMode(DIR1, OUTPUT);
  pinMode(DIR2, OUTPUT);
}

void loop() {
  // Set directions (adjust HIGH/LOW if a motor spins the wrong way)
  digitalWrite(DIR1, LOW);
  digitalWrite(DIR2, HIGH);

  // --- STEP 1: SPEED 255 (100% Speed) ---
  // Motor 1 runs first, Motor 2 stays off
  analogWrite(EN1, 255);
  analogWrite(EN2, 0);
  delay(2000);

  // Motor 1 stops, Motor 2 runs next
  analogWrite(EN1, 0);
  analogWrite(EN2, 255);
  delay(2000);

  // --- STEP 2: SPEED 150 (~59% Speed) ---
  // Motor 1 runs at 150
  analogWrite(EN1, 150);
  analogWrite(EN2, 0);
  delay(2000);

  // Motor 2 runs at 150
  analogWrite(EN1, 0);
  analogWrite(EN2, 150);
  delay(2000);

  // --- STEP 3: SPEED 75 (~29% Speed) ---
  // Motor 1 runs at 75
  analogWrite(EN1, 75);
  analogWrite(EN2, 0);
  delay(2000);

  // Motor 2 runs at 75
  analogWrite(EN1, 0);
  analogWrite(EN2, 75);
  delay(2000);

  // --- STOP BOTH MOTORS ---
  analogWrite(EN1, 0);
  analogWrite(EN2, 0);
  delay(2000);
}

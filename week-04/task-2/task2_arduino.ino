#include <Wire.h>
#include <MPU6050.h>
#include <Servo.h>

#define SERVO_PIN 9
#define GREEN_LED 4
#define RED_LED 3

MPU6050 mpu;
Servo servo;

unsigned long lastMpuTime = 0;
const int mpuInterval = 50; // Send motion data every 50ms (~20Hz)

void setup() {
  Serial.begin(9600);
  Wire.begin();
  
  mpu.initialize();
  
  servo.attach(SERVO_PIN);
  servo.write(90); // Default locked position (90 degrees)
  
  pinMode(GREEN_LED, OUTPUT); // Green LED
  pinMode(RED_LED, OUTPUT);   // Red LED
  
  // Initial state: Locked (Red ON, Green OFF)
  digitalWrite(GREEN_LED, LOW);
  digitalWrite(RED_LED, HIGH);

  if (!mpu.testConnection()) {
    Serial.println("MPU_ERROR");
  }
}

void loop() {
  // 1. Handle incoming commands from Python ('A' = Access Granted, 'D' = Access Denied)
  if (Serial.available() > 0) {
    char command = Serial.read();
    if (command == 'A') {
      servo.write(180);              // Unlock servo
      digitalWrite(GREEN_LED, HIGH); // Green LED ON
      digitalWrite(RED_LED, LOW);    // Red LED OFF
    } else if (command == 'D') {
      servo.write(90);               // Lock servo
      digitalWrite(GREEN_LED, LOW);  // Green LED OFF
      digitalWrite(RED_LED, HIGH);   // Red LED ON
    }
  }

  // 2. Periodically stream MPU6050 accelerometer data
  unsigned long currentTime = millis();
  if (currentTime - lastMpuTime >= mpuInterval) {
    lastMpuTime = currentTime;
    int16_t ax, ay, az, gx, gy, gz;
    mpu.getMotion6(&ax, &ay, &az, &gx, &gy, &gz);
    
    Serial.print("ACC:");
    Serial.print(ax);
    Serial.print(",");
    Serial.println(ay);
  }
}

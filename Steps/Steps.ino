#include <Wire.h>
#include <MPU6050.h>

MPU6050 mpu;

unsigned long lastStepTime = 0;
int stepCount = 0;
const float stepThreshold = 1.2; // Adjust based on sensitivity
const int debounceDelay = 300;   // Minimum time between steps (ms)

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }

  Serial.println("Step Counter Initialized");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);

  // Convert raw to 'g' force
  float az_g = az / 16384.0;

  unsigned long currentTime = millis();

  if (az_g > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    stepCount++;
    lastStepTime = currentTime;
    Serial.print("Step detected! Total: ");
    Serial.println(stepCount);
  }

  delay(10); // Sampling delay
}
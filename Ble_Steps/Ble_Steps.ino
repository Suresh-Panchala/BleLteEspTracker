#include <Wire.h>
#include <MPU6050.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>

// MPU6050 setup
MPU6050 mpu;

// BLE UUIDs (custom 128-bit)
#define ACTIVITY_SERVICE_UUID     "12345678-1234-1234-1234-1234567890ab"
#define ACTIVITY_CHAR_UUID        "12345678-1234-1234-1234-1234567890ac"
#define BATTERY_SERVICE_UUID      "abcdefab-cdef-abcd-efab-cdefabcdefab"
#define BATTERY_CHAR_UUID         "abcdefab-cdef-abcd-efab-cdefabcdefac"

BLECharacteristic *activityChar;
BLECharacteristic *batteryChar;

// Step detection variables
unsigned long lastStepTime = 0;
int stepCount = 0;
const float stepThreshold = 1.2; // g-force threshold
const int debounceDelay = 300;   // ms between steps

// Activity tracking
int activeMinutes = 0;
int restMinutes = 0;
unsigned long lastActivityCheck = 0;

// Battery simulation
int batteryPercent = 76;
bool charging = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  if (!mpu.testConnection()) {
    Serial.println("MPU6050 connection failed");
    while (1);
  }

  // BLE setup
  BLEDevice::init("ESP32_StepTracker");
  BLEServer *pServer = BLEDevice::createServer();

  BLEService *activityService = pServer->createService(ACTIVITY_SERVICE_UUID);
  BLEService *batteryService  = pServer->createService(BATTERY_SERVICE_UUID);

  activityChar = activityService->createCharacteristic(
    ACTIVITY_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  batteryChar = batteryService->createCharacteristic(
    BATTERY_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  activityChar->addDescriptor(new BLE2902());
  batteryChar->addDescriptor(new BLE2902());

  activityService->start();
  batteryService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(ACTIVITY_SERVICE_UUID);
  pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
  BLEDevice::startAdvertising();

  Serial.println("BLE + MPU6050 Step Tracker Started");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;

  unsigned long currentTime = millis();

  // Step detection
  if (az_g > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    stepCount++;
    lastStepTime = currentTime;
    Serial.print("Step: ");
    Serial.println(stepCount);
  }

  // Activity tracking every minute
  if (currentTime - lastActivityCheck >= 60000) {
    if (stepCount > 0) {
      activeMinutes++;
    } else {
      restMinutes++;
    }
    lastActivityCheck = currentTime;
  }

  // BLE sync every 10 seconds
  static unsigned long lastSyncTime = 0;
  if (currentTime - lastSyncTime >= 10000) {
    lastSyncTime = currentTime;

    String timestamp = "2025-08-31T13:14:00Z"; // Replace with RTC or millis-based time
    String activityJSON = "{\"steps\":" + String(stepCount) +
                          ",\"active_minutes\":" + String(activeMinutes) +
                          ",\"rest_minutes\":" + String(restMinutes) +
                          ",\"timestamp\":\"" + timestamp + "\"}";

    String batteryJSON = "{\"battery_percent\":" + String(batteryPercent) +
                         ",\"charging\":" + String(charging ? "true" : "false") +
                         ",\"timestamp\":\"" + timestamp + "\"}";

    activityChar->setValue(activityJSON.c_str());
    batteryChar->setValue(batteryJSON.c_str());

    activityChar->notify();
    batteryChar->notify();

    Serial.println("BLE Sync:");
    Serial.println(activityJSON);
    Serial.println(batteryJSON);
  }

  delay(10); // Sampling delay
}
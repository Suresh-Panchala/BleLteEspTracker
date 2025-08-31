#include <Wire.h>
#include <MPU6050.h>
#include <HardwareSerial.h>
#define TINY_GSM_MODEM_BG96
#include <TinyGsmClient.h>
#include <PubSubClient.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#define ble_lte  23

// MPU6050
MPU6050 mpu;

// EC200U on Serial2
HardwareSerial ec200u(2);

TinyGsm modem(ec200u);
TinyGsmClient client(modem);
PubSubClient mqtt(client);

// BLE UUIDs
#define ACTIVITY_SERVICE_UUID     "12345678-1234-1234-1234-1234567890ab"
#define ACTIVITY_CHAR_UUID        "12345678-1234-1234-1234-1234567890ac"
#define LOCATION_SERVICE_UUID     "abcdefab-cdef-abcd-efab-cdefabcdefab"
#define LOCATION_CHAR_UUID        "abcdefab-cdef-abcd-efab-cdefabcdefac"
#define BATTERY_SERVICE_UUID      "fedcba98-7654-3210-ffff-abcdefabcdef"
#define BATTERY_CHAR_UUID         "fedcba98-7654-3210-aaaa-abcdefabcdef"
#define EVENT_SERVICE_UUID        "11223344-5566-7788-9900-aabbccddeeff"
#define EVENT_CHAR_UUID           "11223344-5566-7788-9900-aabbccddff00"

// BLE Characteristics
BLECharacteristic *activityChar;
BLECharacteristic *locationChar;
BLECharacteristic *batteryChar;
BLECharacteristic *eventChar;

// SIM & MQTT
const char apn[] = "airtelgprs.com"; // Replace with your APN
const char mqttServer[] = "broker.hivemq.com";
const int mqttPort = 1883;
const char clientId[] = "uniqueTracker001";

const char* APN_1 = "www"; 
const char* APN_2 = "airtelgprs.com"; 
const char* APN_3 = "bsnlnet"; 
const char* USER = ""; 
const char* PASS = "";

// Topics
const char* topicActivity = "StepTracker/activity";
const char* topicLocation = "StepTracker/location";
const char* topicBattery  = "StepTracker/battery";
const char* topicEvent    = "StepTracker/event";

// Step detection
unsigned long lastStepTime = 0;
int stepCount = 0;
const float stepThreshold = 1.2;
const int debounceDelay = 300;

// Activity tracking
int activeMinutes = 0;
int restMinutes = 0;
unsigned long lastActivityCheck = 0;

// Battery simulation
int batteryPercent = 76;
bool charging = false;
String carrier ="";
// GPS + timestamp
String latitude = "0.0";
String longitude = "0.0";
String timestamp = "1970-01-01T00:00:00Z";
void carrierName(){
  String response = sendAT("AT+COPS?",1000);
  Serial.print("Carrier Index: ");
  Serial.println(response);
  int colonIndex = response.indexOf(':'); 
  int commaIndex = response.indexOf(',');
  carrier = response.substring(colonIndex + 7, commaIndex + 8); 
  Serial.print("Carrier : ");
  Serial.println(carrier);
  delay(50); 
}
void carrierSelect(){
  if(carrier == "IDEA"){
    modem.gprsConnect(APN_1, USER, PASS);
    Serial.println("Connected to IDEA");
    delay(1000);
    Serial.println("GPRS connected!");
  }
  else if(carrier == "IND "){
    modem.gprsConnect(APN_2, USER, PASS);
    Serial.println("Connected to Airtel");
    delay(1000);
    Serial.println("GPRS connected!");
  }
   else if(carrier == "Andh"){
    modem.gprsConnect(APN_3, USER, PASS);
    Serial.println("Connected to BSNL NET");
    delay(1000);
    Serial.println("GPRS connected!");
  }
  else{
    Serial.println("GPRS Not connected!");
  }
}
void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  ec200u.begin(115200, SERIAL_8N1, 16, 17);
  delay(3000);
  carrierName();
  carrierSelect();
 
  mqtt.setServer(mqttServer, mqttPort);
  Serial.println("MQTT Server Connected");
  delay(1000);

  if (!sendAT("AT", 1000)) {
    Serial.println("EC200U not responding");
    return;
  }
  
  sendAT("AT+QGPS=1", 2000); // Enable GPS

  BLEDevice::init("Step_Tracker");
  BLEServer *pServer = BLEDevice::createServer();

  // BLE Services
  BLEService *activityService = pServer->createService(ACTIVITY_SERVICE_UUID);
  BLEService *locationService = pServer->createService(LOCATION_SERVICE_UUID);
  BLEService *batteryService  = pServer->createService(BATTERY_SERVICE_UUID);
  BLEService *eventService    = pServer->createService(EVENT_SERVICE_UUID);

  // BLE Characteristics
  activityChar = activityService->createCharacteristic(
    ACTIVITY_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  locationChar = locationService->createCharacteristic(
    LOCATION_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  batteryChar = batteryService->createCharacteristic(
    BATTERY_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );
  eventChar = eventService->createCharacteristic(
    EVENT_CHAR_UUID, BLECharacteristic::PROPERTY_READ | BLECharacteristic::PROPERTY_NOTIFY
  );

  activityChar->addDescriptor(new BLE2902());
  locationChar->addDescriptor(new BLE2902());
  batteryChar->addDescriptor(new BLE2902());
  eventChar->addDescriptor(new BLE2902());

  activityService->start();
  locationService->start();
  batteryService->start();
  eventService->start();

  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(ACTIVITY_SERVICE_UUID);
  pAdvertising->addServiceUUID(LOCATION_SERVICE_UUID);
  pAdvertising->addServiceUUID(BATTERY_SERVICE_UUID);
  pAdvertising->addServiceUUID(EVENT_SERVICE_UUID);
  BLEDevice::startAdvertising();

  pinMode(ble_lte, INPUT_PULLUP);

  Serial.println("System Ready");
}

void loop() {
  int bt = digitalRead(ble_lte);
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;
  unsigned long currentTime = millis();

  // Step detection
  if (az_g > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    stepCount++;
    lastStepTime = currentTime;
  }

  // Activity tracking
  if (currentTime - lastActivityCheck >= 60000) {
    if (stepCount > 0) activeMinutes++;
    else restMinutes++;
    lastActivityCheck = currentTime;
  }

  // Sync every 10 seconds
  static unsigned long lastSync = 0;
  if (currentTime - lastSync >= 10000) {
    lastSync = currentTime;

    // GPS + Timestamp
    String gpsRaw = sendAT("AT+QGPSLOC=2", 5000);
    if (gpsRaw.indexOf("+QGPSLOC:") != -1) {
      latitude = parseLatitude(gpsRaw);
      longitude = parseLongitude(gpsRaw);
    }

    String timeRaw = sendAT("AT+CCLK?", 1000);
    timestamp = parseTimestamp(timeRaw);

    // JSON Payloads
    String activityJSON = "{\"steps\":" + String(stepCount) +
                          ",\"active_minutes\":" + String(activeMinutes) +
                          ",\"rest_minutes\":" + String(restMinutes) +
                          ",\"timestamp\":\"" + timestamp + "\"}";

    String locationJSON = "{\"lat\":" + latitude +
                          ",\"lng\":" + longitude +
                          ",\"timestamp\":\"" + timestamp + "\"}";

    String batteryJSON = "{\"battery_percent\":" + String(batteryPercent) +
                         ",\"charging\":" + String(charging ? "true" : "false") +
                         ",\"timestamp\":\"" + timestamp + "\"}";
    if(bt == 1){
      activityChar->setValue(activityJSON.c_str());
      locationChar->setValue(locationJSON.c_str());
      batteryChar->setValue(batteryJSON.c_str());

      activityChar->notify();
      locationChar->notify();
      batteryChar->notify();

      Serial.println("Activity: " + activityJSON);
      Serial.println("Location: " + locationJSON);
      Serial.println("Battery: " + batteryJSON);

      // Geofence Event
      if (isOutsideGeofence(latitude, longitude)) {
        String eventJSON = getEventJSON("geofence_exit", latitude, longitude, timestamp);
        eventChar->setValue(eventJSON.c_str());
        eventChar->notify();
        Serial.println("Event: " + eventJSON);
      }
    }
    else if(bt == 0){
      mqtt.connect(clientId);
      mqtt.publish(topicActivity, activityJSON.c_str());
      mqtt.publish(topicLocation, locationJSON.c_str());
      mqtt.publish(topicBattery, batteryJSON.c_str());

      if (isOutsideGeofence(latitude, longitude)) {
        String eventJSON = getEventJSON("geofence_exit", latitude, longitude, timestamp);
        mqtt.publish(topicEvent, eventJSON.c_str());
      }

      mqtt.disconnect();
      Serial.println("MQTT published");
    }
    
  }

  delay(10);
}

// AT command helper
String sendAT(String cmd, int timeout) {
  ec200u.println(cmd);
  delay(100);
  String response = "";
  long start = millis();
  while (millis() - start < timeout) {
    while (ec200u.available()) {
      response += ec200u.readString();
    }
    if (response.length() > 0) break;
  }
  return response;
}

// Parse latitude from +QGPSLOC
String parseLatitude(String data) {
  int start = data.indexOf(":") + 1;
  int firstComma = data.indexOf(",", start);
  int secondComma = data.indexOf(",", firstComma + 1);
  return data.substring(firstComma + 1, secondComma);
}

// Parse longitude from +QGPSLOC
String parseLongitude(String data) {
  int firstComma = data.indexOf(",");
  int secondComma = data.indexOf(",", firstComma + 1);
  int thirdComma = data.indexOf(",", secondComma + 1);
  return data.substring(secondComma + 1, thirdComma);
}

// Parse timestamp from AT+CCLK?
String parseTimestamp(String raw) {
  int quoteStart = raw.indexOf("\"") + 1;
  int quoteEnd = raw.indexOf("\"", quoteStart);
  String timeStr = raw.substring(quoteStart, quoteEnd);
  timeStr.replace("/", "-");
  timeStr.replace(",", "T");
  timeStr += "Z";
  return timeStr;
}

// Geofence check
bool isOutsideGeofence(String latStr, String lngStr) {
  float lat = latStr.toFloat();
  float lng = lngStr.toFloat();
  float centerLat = 17.423000;
  float centerLng = 78.456000;
  float distance = sqrt(pow(lat - centerLat, 2) + pow(lng - centerLng, 2)) * 111000;
  return distance > 100;
}

// Event JSON
String getEventJSON(String eventType, String lat, String lng, String timestamp) {
  return "{\"event\":\"" + eventType + "\",\"lat\":" + lat +
         ",\"lng\":" + lng + ",\"timestamp\":\"" + timestamp + "\"}";
}
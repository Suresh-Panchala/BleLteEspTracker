#define TINY_GSM_MODEM_BG96
#include <Wire.h>
#include <MPU6050.h>
#include <TinyGsmClient.h>
#include <PubSubClient.h>

// MPU6050
MPU6050 mpu;

// EC200U Serial
#define MODEM_RX 16
#define MODEM_TX 17
#define MODEM_BAUD 115200
HardwareSerial modemSerial(2);

TinyGsm modem(modemSerial);
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

// Topics
const char* topicActivity = "unique/tracker/activity";
const char* topicLocation = "unique/tracker/location";
const char* topicBattery  = "unique/tracker/battery";
const char* topicEvent    = "unique/tracker/event";

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

// GPS + timestamp
String latitude = "0.0";
String longitude = "0.0";
String timestamp = "1970-01-01T00:00:00Z";

String sendRawAT(String command, int timeout) {
  modemSerial.println(command);
  String response = "";
  long start = millis();
  while (millis() - start < timeout) {
    while (modemSerial.available()) {
      response += modemSerial.readStringUntil('\n');
    }
  }
  return response;
}

void setup() {
  Serial.begin(115200);
  Wire.begin();
  mpu.initialize();

  modemSerial.begin(MODEM_BAUD, SERIAL_8N1, MODEM_RX, MODEM_TX);
  delay(3000);

  Serial.println("Initializing modem...");
  modem.restart();
  modem.gprsConnect(apn, "", "");
  Serial.println("GPRS connected");

  mqtt.setServer(mqttServer, mqttPort);
  Serial.println("MQTT Server Connected");
  delay(1000);
  if (!sendRawAT("AT", 1000)) {
    Serial.println("EC200U not responding");
    return;
  }

  sendRawAT("AT+QGPS=1",2000);
  Serial.println("GPS Powered On");
}

void loop() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float az_g = az / 16384.0;
  unsigned long currentTime = millis();

  if (az_g > stepThreshold && (currentTime - lastStepTime > debounceDelay)) {
    stepCount++;
    lastStepTime = currentTime;
  }

  if (currentTime - lastActivityCheck >= 60000) {
    if (stepCount > 0) activeMinutes++;
    else restMinutes++;
    lastActivityCheck = currentTime;
  }

  static unsigned long lastMQTT = 0;
  if (currentTime - lastMQTT >= 10000) {
    lastMQTT = currentTime;

    // GPS location
    String gpsRaw = sendRawAT("AT+QGPSLOC=2",5000);
    Serial.println(gpsRaw);
    if (gpsRaw.indexOf("+QGPSLOC:") != -1) {
      latitude = parseLatitude(gpsRaw);
      longitude = parseLongitude(gpsRaw);
    }
    

    // Timestamp
    String timeRaw = sendRawAT("AT+CCLK?",1000);
    timestamp = parseTimestamp(timeRaw);

    // JSON payloads
    String activityJSON = "{\"steps\":" + String(stepCount) +
                          ",\"active_minutes\":" + String(activeMinutes) +
                          ",\"rest_minutes\":" + String(restMinutes) +
                          ",\"timestamp\":\"" + timestamp + "\"}";
    Serial.println(activityJSON);
    String locationJSON = "{\"lat\":" + latitude +
                          ",\"lng\":" + longitude +
                          ",\"timestamp\":\"" + timestamp + "\"}";
    Serial.println(locationJSON);
    String batteryJSON = "{\"battery_percent\":" + String(batteryPercent) +
                         ",\"charging\":" + String(charging ? "true" : "false") +
                         ",\"timestamp\":\"" + timestamp + "\"}";
    Serial.println(batteryJSON);
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

  delay(10);
}

// Parse helpers
String parseLatitude(String data) {
  int start = data.indexOf(":") + 1;
  int firstComma = data.indexOf(",", start);
  int secondComma = data.indexOf(",", firstComma + 1);
  return data.substring(firstComma + 1, secondComma);
}

String parseLongitude(String data) {
  int firstComma = data.indexOf(",");
  int secondComma = data.indexOf(",", firstComma + 1);
  int thirdComma = data.indexOf(",", secondComma + 1);
  return data.substring(secondComma + 1, thirdComma);
}

String parseTimestamp(String raw) {
  int quoteStart = raw.indexOf("\"") + 1;
  int quoteEnd = raw.indexOf("\"", quoteStart);
  String timeStr = raw.substring(quoteStart, quoteEnd);
  timeStr.replace("/", "-");
  timeStr.replace(",", "T");
  timeStr += "Z";
  return timeStr;
}

bool isOutsideGeofence(String latStr, String lngStr) {
  float lat = latStr.toFloat();
  float lng = lngStr.toFloat();
  float centerLat = 17.423000;
  float centerLng = 78.456000;
  float distance = sqrt(pow(lat - centerLat, 2) + pow(lng - centerLng, 2)) * 111000;
  return distance > 100;
}

String getEventJSON(String eventType, String lat, String lng, String timestamp) {
  return "{\"event\":\"" + eventType + "\",\"lat\":" + lat +
         ",\"lng\":" + lng + ",\"timestamp\":\"" + timestamp + "\"}";
}
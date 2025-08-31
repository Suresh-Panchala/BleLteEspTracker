#include <HardwareSerial.h>
HardwareSerial ec200u(2); 
void setup() {
  Serial.begin(9600);
  ec200u.begin(115200, SERIAL_8N1, 16, 17); 
  delay(3000);
  if (!sendATCommand("AT", 1000)) {
  Serial.println("Error: Communication with EC200U module failed.");
  return;
  }
  // Enable GPS
  if (!sendATCommand("AT+QGPS=1", 2000)) {
  Serial.println("Error: Failed to enable GPS.");
  return;
  }
  Serial.println("EC200U module initialized successfully.");
}
void loop() {
  String gpsData = sendATCommand("AT+QGPSLOC=2", 10000); 
  Serial.println("GPS Data: " + gpsData);
  if (gpsData.indexOf("+QGPSLOC:") != -1) {
    String latitude = parseLatitude(gpsData);
    String longitude = parseLongitude(gpsData);
    Serial.print("Latitude : ");
    Serial.print(latitude);
    Serial.print(" | ");
    Serial.print("Longitude : ");
    Serial.println(longitude);
    delay(10000); // Wait for 1 minute
  } else {
      Serial.println("Failed to retrieve GPS data.");
      delay(10000); // Wait for 10 seconds before retrying
  }
}
String sendATCommand(String command, int timeout) {
    ec200u.println(command);
    delay(100); // Small delay for command execution
    String response = "";
    long startTime = millis();
    while (millis() - startTime < timeout) {
    while (ec200u.available()) {
    response += ec200u.readString();
    }
    if (response.length() > 0) {
    break; // Exit loop if response is received
    }
  }
  Serial.println("AT Command: " + command);
  Serial.println("AT Command Response: " + response);
  return response;
}
String parseLatitude(String gpsData) {
    int startIndex = gpsData.indexOf(",") + 1;
    int endIndex = gpsData.indexOf(",", startIndex);
    String latitude = gpsData.substring(startIndex, endIndex);
    return latitude;
}
String parseLongitude(String gpsData) {
    int startIndex = gpsData.indexOf(",", gpsData.indexOf(",") + 1) + 1; 
    int endIndex = gpsData.indexOf(",", startIndex); 
    String longitude = gpsData.substring(startIndex, endIndex);
    longitude.trim();
    return longitude;
}

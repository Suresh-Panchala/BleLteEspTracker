# Step Tracker with BLE & LTE Connectivity  
**Project Documentation**

---

## Table of Contents
1. [Overview](#overview)
2. [Features](#features)
3. [System Architecture](#system-architecture)
4. [Hardware Components](#hardware-components)
5. [Software Components](#software-components)
6. [Setup Instructions](#setup-instructions)
7. [Code Explanation](#code-explanation)
8. [Data Flow](#data-flow)
9. [MQTT Topics](#mqtt-topics)
10. [BLE Services and Characteristics](#ble-services-and-characteristics)
11. [Geofencing](#geofencing)
12. [Testing and Troubleshooting](#testing-and-troubleshooting)
13. [Future Enhancements](#future-enhancements)
14. [License](#license)

---

## Overview

This project implements a **Step Tracker** device that integrates:
- **MPU6050** for step detection using accelerometer data.
- **EC200U LTE Module** for GPS location tracking and MQTT communication.
- **BLE (Bluetooth Low Energy)** for local communication when paired with a mobile app or BLE scanner.
- Real-time activity, location, battery, and event data are published via MQTT or exposed over BLE based on a hardware switch.

The system is designed to operate in two modes:
- **BLE Mode**: When the `ble_lte` pin is HIGH, data is exposed over BLE.
- **LTE Mode**: When the `ble_lte` pin is LOW, data is sent via MQTT over LTE.

---

## Features

- Step counting using MPU6050 accelerometer.
- Real-time GPS location tracking via EC200U.
- Activity monitoring (active/rest minutes).
- Battery level simulation (can be extended to real battery monitoring).
- Geofence breach detection and alerting.
- Dual communication modes:
  - **BLE** for local access.
  - **MQTT over LTE** for cloud integration.
- Timestamp synchronization from the module clock.

---

## System Architecture

```
+------------------+       +------------------+       +------------------+
|   MPU6050 Accel  |<----->|    ESP32 MCU     |<---->|  EC200U LTE Modem|
+------------------+       +------------------+       +------------------+
                                      |
                                      v
                         +-----------------------------+
                         |     MQTT Broker (HiveMQ)    |
                         +-----------------------------+

                                      |
                                      v
                         +-----------------------------+
                         | Mobile App / Cloud Platform |
                         +-----------------------------+

                                      |
                                      v
                         +-----------------------------+
                         |     BLE Central Device      |
                         +-----------------------------+
```

---

## Hardware Components

| Component         | Description                          |
|------------------|--------------------------------------|
| ESP32            | Main microcontroller with BLE/WiFi   |
| MPU6050          | 6-axis IMU for step detection        |
| EC200U LTE Module | LTE modem for internet & GPS         |
| Jumper wires     | For connections                      |
| Breadboard       | Optional for prototyping             |
| Power Supply     | 5V USB power supply                  |

---

## Software Components

### Libraries Used:
- `Wire.h` – I2C communication for MPU6050.
- `MPU6050.h` – MPU6050 sensor library.
- `HardwareSerial.h` – Serial communication with LTE module.
- `TinyGsmClient.h` – LTE modem communication.
- `PubSubClient.h` – MQTT client.
- `BLEDevice.h`, etc. – BLE stack for ESP32.

### External Services:
- **MQTT Broker**: `broker.hivemq.com`
- **APN**: Configured for Airtel (`airtelgprs.com`)
- **Topics**: See [MQTT Topics](#mqtt-topics)

---

## Setup Instructions

### 1. Wiring Diagram

| ESP32 Pin | Component         |
|-----------|-------------------|
| GPIO21    | SDA (MPU6050)     |
| GPIO22    | SCL (MPU6050)     |
| GPIO16    | RX (EC200U)       |
| GPIO17    | TX (EC200U)       |
| GPIO23    | Switch input (BLE/LTE mode) |

### 2. Flashing the Code

1. Install required libraries in Arduino IDE:
   - `TinyGSM`
   - `PubSubClient`
   - `MPU6050`

2. Select board: `ESP32 Dev Module`.

3. Upload code to ESP32.

### 3. Configuration

Update the following in the code:
- `apn[]` – Your carrier’s APN.
- `mqttServer[]` – MQTT broker address.
- Geofence center coordinates.

---

## Code Explanation

### Initialization (`setup()`)

- Initializes serial communication.
- Initializes I2C for MPU6050.
- Sets up EC200U LTE modem.
- Connects to GPRS.
- Initializes MQTT client.
- Enables GPS on the EC200U.
- Initializes BLE services and advertising.

### Main Loop (`loop()`)

1. Reads accelerometer data from MPU6050.
2. Detects steps using threshold-based logic.
3. Tracks active and rest minutes.
4. Every 10 seconds:
   - Fetches GPS location.
   - Gets timestamp.
   - Builds JSON payloads.
   - Depending on `ble_lte` pin:
     - Sends data over BLE.
     - Or publishes to MQTT broker.

---

## Data Flow

### 1. Step Detection
- Uses Z-axis acceleration from MPU6050.
- Threshold-based detection with debounce delay.

### 2. GPS & Timestamp
- Fetches location via `AT+QGPSLOC`.
- Gets time via `AT+CCLK`.

### 3. JSON Payloads

#### Activity JSON:
```json
{
  "steps": 120,
  "active_minutes": 5,
  "rest_minutes": 10,
  "timestamp": "2025-04-05T10:30:45Z"
}
```

#### Location JSON:
```json
{
  "lat": 17.423000,
  "lng": 78.456000,
  "timestamp": "2025-04-05T10:30:45Z"
}
```

#### Battery JSON:
```json
{
  "battery_percent": 76,
  "charging": false,
  "timestamp": "2025-04-05T10:30:45Z"
}
```

#### Event JSON (on geofence breach):
```json
{
  "event": "geofence_exit",
  "lat": 17.423000,
  "lng": 78.456000,
  "timestamp": "2025-04-05T10:30:45Z"
}
```

---

## MQTT Topics

| Topic                   | Description                  |
|------------------------|------------------------------|
| `StepTracker/activity` | Activity data (steps, etc.)  |
| `StepTracker/location` | GPS location data            |
| `StepTracker/battery`  | Battery status               |
| `StepTracker/event`    | Geofence breach events       |

---

## BLE Services and Characteristics

### 1. Activity Service
- UUID: `12345678-1234-1234-1234-1234567890ab`
- Characteristic: `12345678-1234-1234-1234-1234567890ac`

### 2. Location Service
- UUID: `abcdefab-cdef-abcd-efab-cdefabcdefab`
- Characteristic: `abcdefab-cdef-abcd-efab-cdefabcdefac`

### 3. Battery Service
- UUID: `fedcba98-7654-3210-ffff-abcdefabcdef`
- Characteristic: `fedcba98-7654-3210-aaaa-abcdefabcdef`

### 4. Event Service
- UUID: `11223344-5566-7788-9900-aabbccddeeff`
- Characteristic: `11223344-5566-7788-9900-aabbccddff00`

All characteristics support **Read** and **Notify** properties.

---

## Geofencing

### Geofence Center
- Latitude: `17.423000`
- Longitude: `78.456000`

### Radius
- 100 meters.

### Detection Logic
- Calculates Euclidean distance using:
  ```
  distance = sqrt((lat - centerLat)^2 + (lng - centerLng)^2) * 111000
  ```

If distance > 100 meters, triggers `geofence_exit` event.

---

## Testing and Troubleshooting

### Common Issues

1. **LTE Not Responding**
   - Check power supply to EC200U.
   - Verify serial pin connections.
   - Ensure SIM card is inserted and activated.

2. **BLE Not Discoverable**
   - Restart BLE advertising.
   - Ensure BLE UUIDs are correctly defined.

3. **GPS Not Fixing**
   - Ensure outdoor environment.
   - Check antenna connection.

4. **No MQTT Messages**
   - Confirm broker is reachable.
   - Check APN and internet connection.

---

## Future Enhancements

- Real battery monitoring using voltage divider.
- OLED display for real-time feedback.
- OTA firmware updates.
- Integration with cloud platforms (AWS IoT, Google Cloud).
- Sleep mode for power optimization.
- Historical data logging.

---

## License

This project is open-source and available under the MIT License.

---

Let me know if you'd like a PDF version or a GitHub README format!

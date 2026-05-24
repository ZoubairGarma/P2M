# Smart Gate System - Implementation Guide

## Overview
Refactored ESP32-WROVER (Brain) and ESP32-CAM (Eye) communication for robust, non-blocking operation.

---

## 🎯 Key Improvements

### 1. **Non-Blocking HTTP Communication**
- **Before**: `HTTPClient.GET()` blocked the main loop for up to 5 seconds
- **After**: Async state machine with non-blocking HTTP handling
- **Benefit**: Main loop never freezes; can handle other sensors simultaneously

### 2. **Clean Communication Protocol**
- **Single Endpoint**: `/authorize` (GET)
- **Request**: WROVER sends `GET http://CAM_IP/authorize`
- **Response**: `FACE_OK` or `FACE_NOT_FOUND` (or `CAMERA_ERROR`)
- **Predictable**: No more ambiguous responses

### 3. **Memory Optimization**
- PSRAM detection and monitoring on WROVER
- Memory warnings when free space < 50 KB
- Proper HTTP client cleanup to prevent memory leaks

### 4. **State Machine Architecture**
- Clear state transitions with debugging output
- Synchronized timer handling between devices
- Timeout protection at every stage

---

## 🔧 Configuration

### WROVER Configuration (P2Mesp32)

**File**: `src/main.cpp`

Update these constants to match your environment:
```cpp
const char* ssid = "bartage";              // Your WiFi SSID
const char* password = "zoubaxd55";        // Your WiFi password

// Timing (in milliseconds)
const unsigned long CAR_DETECTION_WINDOW = 10000;   // Time to scan card
const unsigned long CAM_RESPONSE_TIMEOUT = 8000;    // Wait for CAM response
const unsigned long GATE_OPEN_DURATION = 5000;      // Gate stays open
const unsigned long INTER_CAR_COOLDOWN = 3000;      // Wait before next car
```

**File**: `src/camera_comms.cpp`

Update the CAM IP address:
```cpp
String camIP = "10.32.245.1";  // Replace with your ESP32-CAM's actual IP
```

Find your CAM's IP:
1. Upload to CAM and open Serial Monitor
2. Look for: `✅ WiFi Connected! IP: 192.168.X.X`
3. Copy this IP to `camIP` variable

---

### ESP32-CAM Configuration (esp32_cam)

**File**: `src/main.cpp`

Update credentials:
```cpp
#define THINGER_MAX_MESSAGE_SIZE 32768
#define USERNAME "inesss"           // Your Thinger.io username
#define DEVICE_ID "cameraa"         // Your Thinger.io device ID
#define DEVICE_CREDENTIAL "cameraa" // Your Thinger.io credential
```

Update network:
```cpp
const char* ssid = "bartage";
const char* password = "zoubaxd55";
```

---

## 📊 Communication Flow

```
┌─────────────────────────────────────────────────────────────┐
│                     SMART GATE WORKFLOW                      │
└─────────────────────────────────────────────────────────────┘

1. CAR DETECTION (IR Sensor)
   └─> STATE: IDLE → CAR_DETECTED
   └─> "🚗 Car Detected! You have 10 seconds to scan..."

2. RFID SCAN (Local Check)
   └─> STATE: CAR_DETECTED → RFID_SCANNED
   └─> Check authorized database locally
   └─> If NOT authorized: STATE → ACCESS_DENIED (wait 3s)

3. CAM REQUEST (Non-blocking HTTP)
   └─> STATE: RFID_SCANNED → REQUESTING_CAM
   └─> Send: GET http://CAM_IP/authorize
   └─> STATE: REQUESTING_CAM → WAITING_CAM (timeout: 8s)

4. CAM RESPONSE (Async processing)
   └─> CAM receives /authorize request
   └─> CAM captures image
   └─> CAM sends to Thinger.io
   └─> CAM responds: "FACE_OK" or "FACE_NOT_FOUND"

5. DECISION (Non-blocking check)
   └─> WROVER checks async response
   └─> If "FACE_OK": STATE → ACCESS_GRANTED
   └─> If timeout: STATE → ACCESS_DENIED

6. GATE CONTROL
   └─> STATE: ACCESS_GRANTED
   └─> [TODO] Trigger servo motor
   └─> Gate opens for 5 seconds
   └─> STATE → IDLE (ready for next car)
```

---

## 🚀 State Machine Reference

### WROVER States
```
┌────────────────────────────────────────────────┐
│           WROVER STATE MACHINE                 │
├────────────────────────────────────────────────┤
│ STATE_IDLE                                     │
│ └─> Wait for car presence (IR sensor)          │
│                                                │
│ STATE_CAR_DETECTED                             │
│ └─> Timeout: 10 seconds                        │
│ └─> Waiting for RFID scan                      │
│                                                │
│ STATE_RFID_SCANNED                             │
│ └─> Check authorized database                  │
│ └─> If authorized: request CAM                 │
│ └─> If denied: → ACCESS_DENIED                 │
│                                                │
│ STATE_REQUESTING_CAM                           │
│ └─> Sent HTTP request to CAM                   │
│ └─> Immediately → WAITING_CAM                  │
│                                                │
│ STATE_WAITING_CAM                              │
│ └─> Timeout: 8 seconds                         │
│ └─> Wait for async HTTP response               │
│ └─> updateCameraComms() processes response     │
│                                                │
│ STATE_ACCESS_GRANTED                           │
│ └─> Open gate (servo motor)                    │
│ └─> Duration: 5 seconds                        │
│ └─> → IDLE                                     │
│                                                │
│ STATE_ACCESS_DENIED                            │
│ └─> Cooldown: 3 seconds                        │
│ └─> → IDLE                                     │
└────────────────────────────────────────────────┘
```

---

## 🔗 HTTP Endpoint Reference

### ESP32-CAM Endpoints

#### **GET /authorize** (Primary)
- **Purpose**: WROVER requests face recognition
- **Request**: `GET http://CAM_IP/authorize`
- **Response**: 
  - `200 "FACE_OK"` → Face recognized, access granted
  - `200 "FACE_NOT_FOUND"` → No face detected
  - `500 "CAMERA_ERROR"` → Camera hardware error

#### **GET /image** (Debug)
- **Purpose**: Get raw JPEG image for testing
- **Response**: Binary JPEG data

#### **GET /status** (Debug)
- **Purpose**: Get system status as JSON
- **Response**: JSON with IP, WiFi status, uptime

---

## 🐛 Debugging

### Serial Monitor Output Example

**WROVER Output:**
```
================================
🚀 SMART GATE SYSTEM STARTING
================================

📍 Initializing hardware sensors...
🔗 Connecting to Wi-Fi: bartage
✅ Connected! IP: 192.168.1.100
☁️  Initializing Blynk connection...

📊 System Ready!
✅ PSRAM Available: 3648 KB free / 4096 KB total
⏳ Waiting for car...

🚗 Car Detected! You have 10 seconds to scan your card...
📋 Card Scanned! UID:  45 43 0e 30
✅ Card is authorized! Requesting image from ESP32-CAM...
🔄 State Transition: RFID_SCANNED → REQUESTING_CAM
📸 Initiating non-blocking HTTP request to ESP32-CAM /authorize endpoint...
✏️  Waiting for ESP32-CAM response...
✅ CAM Response (200): FACE_OK
🎉 Face Authorized by CAM! ACCESS GRANTED!
🔓 OPENING GATE...
📌 [TODO] Trigger servo motor here!
🔒 Closing gate - duration expired
↩️  Returning to IDLE state
```

**ESP32-CAM Output:**
```
================================
🎥 ESP32-CAM SYSTEM STARTING
================================

🔗 Connecting to WiFi: bartage
✅ WiFi Connected! IP: 10.32.245.1
✅ HTTP Server started on port 80
📸 Waiting for /authorize request from WROVER...

📸 === /authorize ENDPOINT CALLED ===
🚀 ESP32-CAM is now capturing and analyzing...
✅ Image captured: 12345 bytes
📤 Sending image to Thinger.io...
✅ FACE AUTHORIZED - Responding FACE_OK to WROVER
```

### Common Issues

| Issue | Solution |
|-------|----------|
| "Can't reach CAM" | Check IP address in camera_comms.cpp matches CAM's actual IP |
| HTTP timeout | Increase `CAM_RESPONSE_TIMEOUT` in main.cpp |
| "Low PSRAM warning" | Check for memory leaks; reduce image quality |
| WiFi disconnects | Move router closer; check antenna connection |
| Face recognition always fails | Implement actual ML model (see TODO in reseau.cpp) |

---

## 💾 Memory Management

### WROVER Memory Monitoring
```cpp
// Call periodically to check memory
printCameraCommMemory();

// Output:
// 📊 PSRAM Status: 3200 KB free / 4096 KB total (used: 896 KB)
// 💾 Heap Status: 128 KB free / 256 KB total
```

### Memory Optimization Tips
1. Use PSRAM for large buffers if available
2. Free HTTP client after use (automatic in new code)
3. Limit image quality if memory is tight
4. Monitor with `printCameraCommMemory()` periodically

---

## 🎯 Next Steps

### Immediate TODOs

1. **ESP32-CAM Face Recognition**
   - Implement actual face detection (TensorFlow Lite)
   - Update `/authorize` response logic (see camera.cpp line ~50)
   - Test with known faces

2. **Servo Motor Control**
   - Uncomment servo code in WROVER main.cpp (line ~210)
   - Configure servo pin and pulse values
   - Add safety delays

3. **Database Integration**
   - Move RFID UIDs to external database (SD card, Cloud)
   - Sync with Thinger.io for remote management

4. **Logging**
   - Save access logs to SD card
   - Track failed attempts for security

5. **Testing**
   - Test network failure scenarios
   - Test timeout handling
   - Load testing (multiple rapid access attempts)

---

## 📋 Checklist Before Deployment

- [ ] WROVER and CAM have correct WiFi credentials
- [ ] CAM IP address in WROVER code is correct
- [ ] Blynk credentials are valid (WROVER and CAM)
- [ ] RFID authorized cards are in database
- [ ] IR sensor detects cars properly
- [ ] PIR sensor triggers motion alerts
- [ ] Serial output shows clean state transitions
- [ ] Memory warnings don't appear
- [ ] Servo motor control is implemented and tested
- [ ] All timeouts are appropriate for your environment

---

## 🔒 Security Notes

1. **Never hardcode credentials in production** - use secure storage
2. **Use HTTPS** for remote access (add SSL certificates)
3. **Rate limit** HTTP endpoints to prevent brute force
4. **Log all access attempts** for audit trail
5. **Use RFID encryption** if physically accessible
6. **Change default Thinger.io credentials**

---

## 📞 Support Reference

### ESP32-WROVER Pinout Used
- GPIO 14: IR Sensor (car detection)
- GPIO 12: PIR Sensor (motion detection)
- GPIO 5, 18, 19, 23: SPI (RFID scanner)

### ESP32-CAM Pinout Used
- GPIO 5, 18, 19, 23: SPI (camera)
- See config.h for complete pinout

---

**Last Updated**: May 2026
**Version**: 2.0 - Non-blocking Communication

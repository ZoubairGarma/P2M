# Communication Protocol - Detailed Reference

## HTTP Communication Protocol v2.0

### Request-Response Format

```
CLIENT (WROVER)                      SERVER (ESP32-CAM)
       │                                    │
       │  GET /authorize HTTP/1.1           │
       ├─────────────────────────────────> │
       │  Host: 10.32.245.1                │
       │  Connection: close                │
       │  Timeout: 5000ms                  │
       │                                   │
       │                     [Image Capture]
       │                  [Thinger.io Push]
       │                 [Face Recognition*]
       │                                   │
       │  HTTP/1.1 200 OK                  │
       │  Content-Type: text/plain         │
       │  Content-Length: 6                │
       │  Connection: close                │
       │                                   │
       │  FACE_OK                          │
       │ <───────────────────────────────  │
       │                                   │
    [Process]                          
     Response                          
       │                                   │
    [Open Gate]                            │
       │                                   │
```

### Response Codes and Meanings

| Response | HTTP Code | Meaning | Action |
|----------|-----------|---------|--------|
| `FACE_OK` | 200 | Face recognized and authorized | Open gate |
| `FACE_NOT_FOUND` | 200 | No face detected or recognized | Deny access |
| `CAMERA_ERROR` | 500 | Hardware/software camera error | Retry or deny |
| (Timeout) | N/A | CAM didn't respond in 8 seconds | Deny access |
| (Connection failed) | N/A | Can't reach CAM on network | Deny access |

### Detailed State Flow with Timing

```
T=0ms      Car appears
           ├─ STATE: IDLE → CAR_DETECTED
           └─ IR sensor triggers
              Timer starts: 0ms

T=1500ms   RFID card scanned
           ├─ STATE: CAR_DETECTED → RFID_SCANNED
           ├─ Check local database
           └─ If authorized, continue...

T=1600ms   Send HTTP request
           ├─ STATE: RFID_SCANNED → REQUESTING_CAM
           ├─ HTTPClient opens socket
           ├─ Send: GET /authorize
           └─ STATE: REQUESTING_CAM → WAITING_CAM
              Timer starts: 1600ms

T=1650ms   CAM receives request
           ├─ Begin image capture
           ├─ Encode to Base64
           └─ Push to Thinger.io

T=2100ms   CAM sends response
           ├─ HTTP 200 OK
           ├─ Body: "FACE_OK"
           └─ Socket closes

T=2150ms   WROVER processes response
           ├─ updateCameraComms() called
           ├─ Parse response
           ├─ STATE: WAITING_CAM → ACCESS_GRANTED
           └─ Timer starts: 2150ms

T=2160ms   Servo activates
           ├─ GPIO pin goes HIGH
           ├─ Gate physical movement begins
           └─ Serial: "🔓 OPENING GATE..."

T=7150ms   Gate duration expired
           ├─ Servo deactivates
           ├─ GPIO pin goes LOW
           ├─ STATE: ACCESS_GRANTED → IDLE
           └─ Serial: "🔒 Closing gate..."

T=7150ms+  Ready for next car
           ├─ STATE: IDLE
           └─ Waiting for next car detection
```

### HTTP Response Trace Example

```
> GET /authorize HTTP/1.1
> Host: 10.32.245.1:80
> Connection: close
> User-Agent: ESP32HTTPClient/1.0

< HTTP/1.1 200 OK
< Content-Type: text/plain
< Content-Length: 6
< Connection: close
< 
< FACE_OK
```

---

## Error Handling Scenarios

### Scenario 1: CAM Not Reachable
```
T=1600ms   Send HTTP GET /authorize
           └─ Socket fails to connect

WROVER:
├─ HTTPClient.begin() returns false
├─ currentState = FAILED
├─ Serial: "❌ Failed to begin HTTP connection"
└─ STATE: REQUESTING_CAM → ACCESS_DENIED
   └─ Wait 3 seconds
   └─ STATE: ACCESS_DENIED → IDLE

Result: Access denied, user retries
```

### Scenario 2: CAM Response Timeout
```
T=1600ms   Send HTTP GET /authorize
T=2000ms   CAM is busy (slow processor)
T=6000ms   Still waiting (5 seconds elapsed)
T=9600ms   Timeout exceeded (8 second limit)

WROVER:
├─ updateCameraComms() detects timeout
├─ Serial: "⏰ HTTP Request TIMEOUT after 8000ms"
├─ currentState = TIMEOUT
└─ STATE: WAITING_CAM → ACCESS_DENIED
   └─ Wait 3 seconds
   └─ STATE: ACCESS_DENIED → IDLE

Result: Access denied, user retries
```

### Scenario 3: Face Not Recognized
```
T=1600ms   Send HTTP GET /authorize
T=2100ms   CAM responds with "FACE_NOT_FOUND"

WROVER:
├─ updateCameraComms() gets response
├─ Parse "FACE_NOT_FOUND"
├─ faceMatched = false
├─ currentState = FAILED
├─ Serial: "❌ Face NOT Authorized - access denied"
└─ STATE: WAITING_CAM → ACCESS_DENIED
   └─ Wait 3 seconds
   └─ STATE: ACCESS_DENIED → IDLE

Result: Access denied, user retries
```

### Scenario 4: WiFi Disconnected
```
At any time: WiFi drops

WROVER (main loop):
├─ runBlynk() detects no connection
├─ WiFi.begin() called (reconnect)
├─ Background: waiting for WiFi

When WiFi reconnects:
├─ Connection restored
├─ Can now communicate with CAM
└─ System continues normally

Note: If car is detected while WiFi is down,
      access will be denied (can't reach CAM)
```

---

## Non-Blocking HTTP Implementation Details

### How updateCameraComms() Works

```cpp
// In main loop, called every 50ms:
void updateCameraComms() {
  // Only check if waiting for response
  if (currentState != WAITING_RESPONSE) return;
  
  // Check if timeout
  if (millis() - requestStartTime > 8000) {
    currentState = TIMEOUT;
    return;  // Non-blocking, done
  }
  
  // Check if response available (non-blocking)
  int httpResponseCode = httpClient->getResponseCode();
  
  if (httpResponseCode == 0) {
    return;  // Still waiting, exit and come back later
  }
  
  // Response ready! Process it
  if (httpResponseCode == 200) {
    String response = httpClient->getString();
    if (response == "FACE_OK") {
      faceMatched = true;
      currentState = SUCCESS;
    }
  }
  
  cleanupHTTP();  // Free resources
}
```

### Why Non-Blocking Matters

**Blocking (Old)**:
```
Main Loop:
├─ Check IR Sensor     (1ms)
├─ Check RFID          (2ms)
├─ Check PIR           (1ms)
├─ requestFaceScan()   (5000ms BLOCKED!)
│  └─ Everyone waits...
│  └─ Can't check anything
│  └─ Might miss motion alert
│  └─ Might miss RFID scan timeout
└─ Continue...
```

**Non-Blocking (New)**:
```
Main Loop (every 50ms):
├─ Check IR Sensor             (1ms)
├─ Check RFID                  (2ms)
├─ Check PIR                   (1ms)
├─ updateCameraComms()         (0.1ms - just check status)
│  └─ If response ready, process it
│  └─ If not, return immediately
├─ Check timeouts              (1ms)
└─ Continue... Total ~5ms per loop

Benefits:
✅ Main loop responsive
✅ Can detect multiple events
✅ Can handle PIR while waiting for CAM
✅ Can timeout CAM request if too slow
```

---

## Memory Usage Analysis

### WROVER Memory Breakdown

| Component | RAM | PSRAM | Purpose |
|-----------|-----|-------|---------|
| Core OS | ~100 KB | - | Arduino framework |
| WiFi Stack | ~80 KB | - | WiFi driver |
| HTTPClient | ~5 KB | - | HTTP implementation |
| Blynk | ~40 KB | - | Cloud connection |
| RFID Driver | ~10 KB | - | MFRC522 SPI |
| Program Code | ~100 KB | - | .cpp/.h files |
| Free Heap | ~50 KB | - | Available RAM |
| **Total Heap** | **~385 KB** | - | - |
| **PSRAM (if available)** | - | **4 MB** | General storage |

### Optimization Tips

1. **Use PSRAM for large buffers**:
   ```cpp
   if (psramFound()) {
     uint8_t* largeBuffer = (uint8_t*)ps_malloc(100000);
   }
   ```

2. **Monitor before crisis**:
   ```cpp
   if (ESP.getFreeHeap() < 20000) {
     Serial.println("⚠️  CRITICAL: Less than 20KB free!");
   }
   ```

3. **String pooling** - Use F() macro:
   ```cpp
   Serial.println(F("This string stays in FLASH, not RAM"));
   ```

---

## Troubleshooting Guide

### Problem: "HTTPClient failed with code -1"
```
Cause: Socket connection timeout
Fix:   - Check CAM IP address is correct
       - Check CAM is powered on and WiFi connected
       - Check firewall allows port 80
       - Check distance between devices (WiFi range)
Action: Increase HTTP_TIMEOUT_MS to 10000 if network is slow
```

### Problem: "PSRAM NOT detected"
```
Cause: ESP32-WROVER not using PSRAM build variant
Fix:   - PlatformIO: Change board to "esp32" with PSRAM option
       - OR add to platformio.ini:
         board_build.f_flash = 40000000L
         board_build.flash_mode = dio
         board_build.psram_mode = opi
```

### Problem: Face always returns "FACE_NOT_FOUND"
```
Cause: Face recognition not implemented in CAM
Fix:   - Camera.cpp line 50: faceRecognized = true is placeholder
       - Implement TensorFlow Lite or OpenCV face detection
       - Test with different lighting conditions
       - Adjust camera exposure and white balance
```

### Problem: Servo doesn't activate
```
Cause: TODO code not implemented in WROVER main.cpp
Fix:   - Uncomment servo initialization in setup()
       - Uncomment servoOpen()/servoClose() in STATE_ACCESS_GRANTED
       - Configure servo pin and angle values
       - Test servo directly with PWM signal first
```

---

## Performance Metrics

### Expected Timing

| Operation | Duration |
|-----------|----------|
| IR Detection → Serial Output | 50ms |
| RFID Scan | 200-500ms |
| Local Authorization Check | <1ms |
| HTTP Request Send | ~50ms |
| CAM Image Capture | ~200-300ms |
| CAM Image Encoding | ~100-200ms |
| CAM Response Send | ~50ms |
| WROVER Response Process | <1ms |
| **Total Gate-to-Open** | ~500-800ms |

### Network Impact

| Condition | Impact |
|-----------|--------|
| WiFi 2.4GHz (good) | +50-100ms |
| WiFi 2.4GHz (fair) | +200-400ms |
| WiFi 5GHz | +20-50ms |
| WiFi weak signal | +500ms+ (may timeout) |
| Same subnet | Fastest |
| Different subnet | Add ~50ms |

---

## Development vs Production

### Development Setup
```cpp
// Enable all debugging
#define DEBUG 1
const unsigned long HTTP_TIMEOUT_MS = 5000;
const unsigned long REQUEST_TIMEOUT_MS = 8000;
```

### Production Setup
```cpp
// Disable verbose output
#define DEBUG 0
const unsigned long HTTP_TIMEOUT_MS = 3000;  // Faster
const unsigned long REQUEST_TIMEOUT_MS = 6000;  // More responsive
```

---

**Last Updated**: May 2026
**Protocol Version**: 2.0

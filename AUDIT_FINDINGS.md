# **Dual-ESP32 Smart Gate Architecture - Deep Dive Audit**

**Date:** May 24, 2026  
**Systems Audited:** ESP32-WROVER-B (Master) + ESP32-CAM (Slave)  
**Purpose:** Face Authentication for Smart Gate Control

---

## **EXECUTIVE SUMMARY**

The architecture has **5 critical issues** that could cause crashes, memory leaks, and timing failures:
1. Timeout mismatches between HTTP client and face recognition
2. Infinite blocking loops and unresponsive web server
3. Memory leaks in HTTP client initialization and frame buffer handling
4. State synchronization failures (cannot distinguish success from failure)
5. Graceful WiFi failure recovery missing

**Risk Level:** HIGH - Production-blocking without fixes.

---

## **DETAILED FINDINGS**

### **1. TIMEOUT MISMATCHES**

#### Issue 1.1: Timeout Configuration Inconsistency
- **Location:** `P2Mesp32/src/camera_comms.cpp` (WROVER)
- **Problem:** 
  - `HTTP_TIMEOUT_MS = 5000` (connection timeout)
  - `REQUEST_TIMEOUT_MS = 8000` (total timeout)
  - HTTP socket can timeout at 5s, but state machine waits until 8s
  - This creates a 3-second window where the state machine thinks it's still waiting for a response that already timed out
- **Impact:** Unpredictable behavior; potential for duplicate requests or zombie connections

#### Issue 1.2: No Timeout on CAM Face Recognition
- **Location:** `esp32_cam/src/reseau.cpp` (/authorize endpoint)
- **Problem:**
  ```cpp
  uint8_t* hash = generateFaceHash(fb);           // NO TIMEOUT
  int faceIndex = recognizeFace(hash);            // NO TIMEOUT
  ```
  - Face recognition operations are synchronous with no timeout
  - If face generation takes > 5 seconds, WROVER times out but CAM continues blocking
  - Web server thread is blocked, cannot serve other requests
- **Impact:** CAM web server becomes unresponsive; WROVER gets timeout; asymmetric behavior

---

### **2. BLOCKING CODE**

#### Issue 2.1: INFINITE BLOCKING LOOP in CAM WiFi Init
- **Location:** `esp32_cam/src/reseau.cpp` - `initialiserWiFiEtCloud()`
- **Problem:**
  ```cpp
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  ```
  - **NO TIMEOUT** - if WiFi credential is wrong or network unreachable, this loops forever
  - Setup() hangs indefinitely; device never reaches `initialiserServeurWeb()`
  - No recovery mechanism
- **Impact:** Device becomes brick if WiFi credentials are wrong; manual reset required

#### Issue 2.2: HTTP Server Blocked During Face Recognition
- **Location:** `esp32_cam/src/reseau.cpp` - `/authorize` endpoint
- **Problem:**
  - All operations inside the HTTP handler are synchronous/blocking
  - `esp_camera_fb_get()` (blocks until frame is ready)
  - `generateFaceHash()` and `recognizeFace()` (CPU-intensive)
  - During this time, `server.handleClient()` in main loop cannot process other requests
  - If recognition takes 3+ seconds, any other HTTP request hangs
- **Impact:** Multi-client requests will timeout or queue indefinitely

#### Issue 2.3: WROVER State Machine Delay
- **Location:** `P2Mesp32/src/main.cpp` - main loop
- **Problem:**
  ```cpp
  delay(50);  // At end of loop
  ```
  - Not critical, but adds unnecessary latency to state machine
  - With 50ms delay per loop, it takes 100ms minimum to detect state changes
- **Impact:** Minor - 50ms latency to each state transition

---

### **3. MEMORY LEAKS**

#### Issue 3.1: HTTPClient Not Freed on Connection Failure
- **Location:** `P2Mesp32/src/camera_comms.cpp` - `requestFaceScan()`
- **Problem:**
  ```cpp
  httpClient = new HTTPClient();
  String url = "http://" + camIP + "/authorize";
  
  if (!httpClient->begin(url)) {
    Serial.println("❌ Failed to begin HTTP connection");
    currentState = FAILED;
    delete httpClient;           // ← Freed here
    httpClient = nullptr;
    return;                       // ← But never called updateCameraComms()!
  }
  ```
  - If connection fails, the object is deleted immediately
  - BUT: state is set to FAILED, so `updateCameraComms()` is called in main loop
  - If called again, nullptr GET() is attempted
  - **However**, in normal operation, the early cleanup is fine, but the code doesn't re-acquire the object properly on retry
- **Impact:** Potential null pointer dereference on rapid retries

#### Issue 3.2: Frame Buffer Leak on Camera Error
- **Location:** `esp32_cam/src/reseau.cpp` - `/authorize` endpoint
- **Problem:**
  ```cpp
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Erreur capture");
    server.send(500, "text/plain", "CAMERA_ERROR");
    return;  // ← fb never returned!
  }
  
  // ... later if recognizeFace() fails ...
  if (faceIndex >= 0) {
    // ... success path returns fb
  } else {
    server.send(200, "text/plain", "FACE_UNKNOWN");
    // ... code continues to free(hash) and esp_camera_fb_return(fb)
  }
  ```
  - If initial capture fails, `server.send()` doesn't return; function returns without `esp_camera_fb_return()`
  - This leaks one frame buffer (max 320x240 JPEG ≈ 10KB)
  - After 10-20 errors, PSRAM exhausted, CAM crashes
- **Impact:** Crash after repeated camera failures

#### Issue 3.3: HTTPClient GET() Can Leak on Rapid State Changes
- **Location:** `P2Mesp32/src/camera_comms.cpp` - `updateCameraComms()`
- **Problem:**
  ```cpp
  int httpResponseCode = httpClient->GET();
  
  if (httpResponseCode <= 0) {
    Serial.println("⏳ Waiting for response...");
    return;  // ← If GET() keeps returning ≤0, cleanupHTTP() never called
  }
  ```
  - If network is unstable, GET() may return 0-100 error codes repeatedly
  - Until a "real" response arrives or timeout, the HTTPClient stays allocated
  - Multiple rapid retries could accumulate objects
- **Impact:** Slow memory leak over 100s of failed requests

---

### **4. STATE SYNC ISSUES**

#### Issue 4.1: Cannot Distinguish Success from Failure
- **Location:** Both systems
- **Problem:**
  - CAM sends `HTTP 200` for BOTH success AND failure cases:
    - Success: `server.send(200, "text/plain", "FACE_OK")`
    - Failure: `server.send(200, "text/plain", "FACE_UNKNOWN")`
  - WROVER only has HTTP response code to distinguish success
  - But response code is ALWAYS 200, so WROVER must parse the body text
  - **Issue:** If network corrupts the response body, WROVER cannot tell
  - **Best Practice:** Use HTTP status codes correctly:
    - `200 OK` for authorized (FACE_OK)
    - `401 Unauthorized` for rejected (FACE_UNKNOWN)
    - `202 Accepted` for still processing (FACE_PROCESSING)
- **Impact:** Ambiguous state; hard to debug; recovers poorly from network errors

#### Issue 4.2: No Intermediate Status During Face Recognition
- **Location:** ESP32-CAM `/authorize` endpoint
- **Problem:**
  - CAM blocks for 3-5+ seconds doing face recognition
  - WROVER has NO visibility into progress (no "PROCESSING" response)
  - WROVER state machine shows "WAITING_CAM" for the entire duration
  - If CAM crashes during recognition, WROVER waits the full 8-second timeout before failing
- **Impact:** Poor UX; slow failure detection

#### Issue 4.3: State Reset Race Condition
- **Location:** `P2Mesp32/src/camera_comms.cpp` - `isFaceAuthorized()`
- **Problem:**
  ```cpp
  bool isFaceAuthorized() {
    if (faceMatched && currentState == SUCCESS) {
      faceMatched = false;
      currentState = IDLE;  // ← Resets immediately
      return true;
    }
    return false;
  }
  ```
  - Called from main loop in STATE_WAITING_CAM
  - If called twice in one iteration (shouldn't happen, but race is possible), second call sees state already reset
  - This is a minor issue in current architecture, but fragile
- **Impact:** Race condition on very fast state transitions

---

### **5. ADDITIONAL ISSUES**

#### Issue 5.1: No Graceful WiFi Failure Recovery
- **Location:** `P2Mesp32/src/main.cpp` - setup()
- **Problem:**
  - WiFi.begin() is called in setup() with timeout of 10 seconds (20 * 500ms)
  - If it fails, code continues with `WiFi.status() != WL_CONNECTED` = true
  - Only periodic check in main loop attempts reconnection (5-second intervals)
  - HTTPClient will fail silently if WiFi drops mid-request
- **Impact:** Slow recovery from WiFi dropout; potential for zombie connections

#### Issue 5.2: NVS Commit Status Not Checked
- **Location:** `esp32_cam/src/camera.cpp` - `enrollFace()`
- **Problem:**
  ```cpp
  nvs_commit(nvs_handle);  // ← Return code not checked!
  nvs_close(nvs_handle);
  esp_camera_fb_return(fb);
  free(hash);
  Serial.printf("✅ Visage enregistré...");  // ← Prints success even if commit failed!
  ```
- **Impact:** Face enrollment appears successful but data may not be persisted; face is lost on power reset

#### Issue 5.3: Oversimplified Face Recognition Algorithm
- **Location:** `esp32_cam/src/camera.cpp` - `generateFaceHash()`
- **Problem:**
  ```cpp
  for (size_t i = 0; i < len && i < 256; i++) {
    hash[i % 32] ^= buf[i];  // XOR of first 256 bytes
    hash[(i + 1) % 32] += buf[i];
  }
  ```
  - This is essentially a checksum, not real face recognition
  - Different faces will have similar hashes; same face at different angles will have different hashes
  - **Workaround:** Use ESP-WHO library for ML-based face detection
- **Impact:** High false positive/negative rate; unreliable authentication

---

## **SEVERITY SUMMARY**

| **Issue** | **Severity** | **Component** | **Fix Required** |
|-----------|------------|---------------|-----------------|
| WiFi infinite loop | 🔴 CRITICAL | CAM WiFi init | Yes - Timeout + Retry |
| CAM face recognition timeout | 🔴 CRITICAL | CAM /authorize | Yes - Timeout wrapper |
| Frame buffer leak on error | 🔴 CRITICAL | CAM /authorize | Yes - Proper cleanup |
| Timeout mismatch | 🟠 HIGH | WROVER HTTP | Yes - Align timeouts |
| HTTP response parsing | 🟠 HIGH | Both | Yes - Use HTTP status codes |
| HTTPClient not freed on retry | 🟠 HIGH | WROVER | Yes - Proper state mgmt |
| No intermediate status | 🟡 MEDIUM | CAM | Yes - Send 202 Accepted |
| 50ms loop delay | 🟡 MEDIUM | WROVER | Optional - Remove delay |
| NVS commit not checked | 🟡 MEDIUM | CAM NVS | Yes - Check return |
| Oversimplified face hash | 🟡 MEDIUM | CAM face | Yes - Use ML recognition |

---

## **TESTING RECOMMENDATIONS**

1. **WiFi Failure Test**: Disconnect WiFi during operation; verify CAM recovers and WROVER times out gracefully
2. **Timeout Test**: Inject 10-second delay in `generateFaceHash()`; verify WROVER timeout at 8 seconds
3. **Memory Leak Test**: Trigger 100 camera errors; measure heap usage (should not grow)
4. **Rapid Request Test**: Send 10 HTTP requests per second to CAM; verify no dropped requests
5. **Long-Duration Test**: Run for 24 hours; monitor for crashes, timeouts, or memory leaks


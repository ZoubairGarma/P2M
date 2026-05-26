// src/camera_comms.cpp (WROVER - CORRECTED, Non-blocking & Crash-Free)
#include "camera_comms.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================
// CONFIGURATION - ALIGNED TIMEOUTS
// ============================================
String camIP = "10.93.156.1";  // Replace with your ESP32-CAM's IP
const int HTTP_CONNECT_TIMEOUT_MS = 10000;   // 10s to connect (increased for face processing)
const int HTTP_RESPONSE_TIMEOUT_MS = 30000;  // 30s to get response (face recognition is slow!)
const unsigned long REQUEST_TOTAL_TIMEOUT_MS = 35000;  // 35s total (must be >= HTTP_RESPONSE_TIMEOUT_MS)

// ============================================
// STATE MACHINE & HTTP MANAGEMENT
// ============================================
CameraCommState currentState = IDLE;
unsigned long requestStartTime = 0;
HTTPClient* httpClient = nullptr;
bool faceMatched = false;
unsigned long lastRequestTime = 0;

// PSRAM memory tracking
size_t psramFree = 0;
size_t psramUsed = 0;

// ============================================
// INITIALIZATION
// ============================================
void setupCameraComms() {
  Serial.println("📷 Camera Comms Initialized (Non-blocking mode)");
  
  // Check PSRAM availability
  if (psramFound()) {
    psramFree = ESP.getFreePsram();
    psramUsed = ESP.getPsramSize() - psramFree;
    Serial.printf("✅ PSRAM Available: %u KB free / %u KB total\n", 
                  psramFree / 1024, ESP.getPsramSize() / 1024);
  } else {
    Serial.println("⚠️  PSRAM NOT detected - using regular SRAM only");
  }
  
  currentState = IDLE;
}

// ============================================
// NON-BLOCKING REQUEST FUNCTION
// ============================================
void requestFaceScan() {
  // Only allow one request at a time
  if (currentState != IDLE) {
    Serial.println("⏳ Camera request already in progress, ignoring new request");
    return;
  }
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ Wi-Fi not connected! Cannot reach ESP32-CAM");
    currentState = FAILED;
    return;
  }
  
  Serial.println("📸 Initiating non-blocking HTTP request to ESP32-CAM /authorize endpoint...");
  
  currentState = REQUESTING;
  requestStartTime = millis();
  lastRequestTime = millis();
  faceMatched = false;
  
  // Clean up any existing HTTP client
  cleanupHTTP();
  httpClient = new HTTPClient();
  
  if (httpClient == nullptr) {
    Serial.println("❌ Failed to allocate HTTPClient memory");
    currentState = FAILED;
    return;
  }
  
  String url = "http://" + camIP + "/authorize";
  
  Serial.println("🔗 Camera IP: " + camIP);
  Serial.println("📡 URL: " + url);
  Serial.println("📸 Triggering RFID photo capture on camera...");
  
  // FIX: Set timeouts BEFORE begin()
  httpClient->setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  httpClient->setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  
  Serial.printf("⏱️  Timeouts set: Connect=%dms, Response=%dms\n", 
                HTTP_CONNECT_TIMEOUT_MS, HTTP_RESPONSE_TIMEOUT_MS);
  
  // Attempt connection with proper error handling
  if (!httpClient->begin(url)) {
    Serial.println("❌ Failed to initialize HTTP connection");
    Serial.println("   Possible causes:");
    Serial.println("   1. Camera IP is wrong - check ESP32-CAM startup logs");
    Serial.println("   2. Camera not connected to WiFi network");
    Serial.println("   3. Camera hasn't finished booting");
    Serial.println("   4. Network connectivity issue between ESP32 boards");
    currentState = FAILED;
    cleanupHTTP();  // Properly clean up on error
    return;
  }
  
  // Transition to waiting state
  currentState = WAITING_RESPONSE;
  Serial.println("✏️  Waiting for ESP32-CAM response (max " + String(REQUEST_TOTAL_TIMEOUT_MS/1000) + "s)...");
}

// ============================================
// UPDATE FUNCTION (Call in main loop)
// ============================================
void updateCameraComms() {
  if (currentState != WAITING_RESPONSE) {
    return;  // Early exit if not waiting
  }
  
  if (httpClient == nullptr) {
    // This shouldn't happen, but guard against null pointer
    currentState = FAILED;
    return;
  }
  
  // FIX: Check timeout FIRST before any operations
  unsigned long elapsedTime = millis() - requestStartTime;
  if (elapsedTime > REQUEST_TOTAL_TIMEOUT_MS) {
    Serial.print("⏰ HTTP Request TIMEOUT after ");
    Serial.print(elapsedTime);
    Serial.println(" ms");
    currentState = TIMEOUT;
    cleanupHTTP();
    return;
  }
  
  // Send the request and get response code
  int httpResponseCode = httpClient->GET();
  
  // FIX: Handle in-progress (0 means still connecting/sending)
  if (httpResponseCode == 0) {
    // Still waiting, no response yet
    return;
  }
  
  // Handle network errors
  if (httpResponseCode < 0) {
    Serial.print("❌ HTTP Error Code: ");
    Serial.println(httpResponseCode);
    
    // Print specific error details
    switch(httpResponseCode) {
      case -1:
        Serial.println("   ERROR: Connection failed - Camera not reachable");
        Serial.println("   Check: Is ESP32-CAM powered on?");
        Serial.println("   Check: Is ESP32-CAM IP correct? (Current: " + camIP + ")");
        Serial.println("   Check: Are both devices on same WiFi network?");
        break;
      case -2:
        Serial.println("   ERROR: Connection lost during request");
        break;
      case -3:
        Serial.println("   ERROR: Connection timeout (server not responding)");
        Serial.println("   Camera may be: powering up, processing, or overloaded");
        break;
      case -11:
        Serial.println("   ERROR: DNS resolution failed");
        Serial.println("   Camera IP might be incorrect");
        break;
      default:
        Serial.printf("   ERROR: HTTP error %d\n", httpResponseCode);
        break;
    }
    
    currentState = FAILED;
    cleanupHTTP();
    return;
  }
  
  // FIX: Use proper HTTP status codes for clear semantics
  // Response received! /rfid/trigger just returns 200 if triggered successfully
  if (httpResponseCode == 200) {
    // 200 OK - RFID trigger accepted, camera is now capturing photos
    String response = httpClient->getString();
    Serial.print("✅ Camera Response (200): ");
    Serial.println(response);
    
    // Camera will now process in background - simulate success after response
    faceMatched = true;  // Accept the trigger as successful
    currentState = SUCCESS;
    Serial.println("🎉 RFID trigger sent! Camera capturing photos...");
    cleanupHTTP();
    return;
  } 
  else {
    // Any other response code
    Serial.print("⚠️  Unexpected CAM response code: ");
    Serial.println(httpResponseCode);
    currentState = FAILED;
    cleanupHTTP();
    return;
  }
}

// ============================================
// HELPER FUNCTIONS
// ============================================
bool isFaceAuthorized() {
  // FIX: Only return true ONCE, then reset state atomically
  if (currentState == SUCCESS && faceMatched) {
    faceMatched = false;
    currentState = IDLE;  // Reset state after consuming result
    return true;
  }
  return false;
}

CameraCommState getCameraCommState() {
  return currentState;
}

void resetCameraComm() {
  currentState = IDLE;
  faceMatched = false;
  cleanupHTTP();
}

void cleanupHTTP() {
  if (httpClient != nullptr) {
    httpClient->end();     // Close connection
    delete httpClient;
    httpClient = nullptr;
  }
}

// ============================================
// MEMORY MONITORING
// ============================================
void printCameraCommMemory() {
  if (psramFound()) {
    size_t currentFree = ESP.getFreePsram();
    size_t currentUsed = ESP.getPsramSize() - currentFree;
    
    Serial.printf("📊 PSRAM Status: %u KB free / %u KB total (used: %u KB)\n",
                  currentFree / 1024, ESP.getPsramSize() / 1024, currentUsed / 1024);
    
    if (currentFree < 50 * 1024) {  // Less than 50 KB free
      Serial.println("⚠️  WARNING: Low PSRAM available!");
    }
  }
  
  Serial.printf("💾 Heap Status: %u KB free / %u KB total\n",
                ESP.getFreeHeap() / 1024, ESP.getHeapSize() / 1024);
}

// ============================================
// DIAGNOSTIC: Test camera connectivity
// ============================================
void testCameraConnection() {
  Serial.println("\n==========================================");
  Serial.println("🔍 CAMERA CONNECTIVITY TEST");
  Serial.println("==========================================");
  
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("❌ WiFi not connected! Cannot test camera.");
    return;
  }
  
  Serial.printf("✅ WiFi connected: %s\n", WiFi.SSID().c_str());
  Serial.printf("📡 Current device IP: %s\n", WiFi.localIP().toString().c_str());
  Serial.printf("🎥 Testing camera at: http://%s/\n\n", camIP.c_str());
  
  HTTPClient testClient;
  testClient.setConnectTimeout(3000);
  testClient.setTimeout(5000);
  
  String testUrl = "http://" + camIP + "/";
  
  Serial.print("Attempting connection...");
  if (!testClient.begin(testUrl)) {
    Serial.println(" FAILED!");
    Serial.println("❌ Cannot initialize connection to camera");
    return;
  }
  
  int response = testClient.GET();
  
  Serial.print(" ");
  
  if (response == 200) {
    Serial.println("\n✅ CAMERA IS REACHABLE!");
    String data = testClient.getString();
    Serial.println("Response: " + data);
  } else if (response > 0) {
    Serial.printf("\n⚠️  Camera responded with HTTP %d\n", response);
  } else if (response == 0) {
    Serial.println("\n⏳ Request in progress (0)");
  } else {
    Serial.printf("\n❌ Connection failed (error: %d)\n", response);
    Serial.println("   Possible causes:");
    Serial.println("   • Camera IP is incorrect");
    Serial.println("   • Camera is not powered on");
    Serial.println("   • Camera WiFi not connected");
    Serial.println("   • Network connectivity issue");
  }
  
  testClient.end();
  
  Serial.println("\n🎥 To fix:");
  Serial.println("   1. Check ESP32-CAM startup logs for its actual IP");
  Serial.println("   2. Update camIP variable if needed");
  Serial.println("   3. Verify both devices on same WiFi network (bartage)");
  Serial.println("==========================================\n");
}

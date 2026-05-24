// src/camera_comms.cpp (WROVER - CORRECTED, Non-blocking & Crash-Free)
#include "camera_comms.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================
// CONFIGURATION - ALIGNED TIMEOUTS
// ============================================
String camIP = "10.93.156.1";  // Replace with your ESP32-CAM's IP
const int HTTP_CONNECT_TIMEOUT_MS = 3000;   // 3s to connect
const int HTTP_RESPONSE_TIMEOUT_MS = 6000;  // 6s to get response
const unsigned long REQUEST_TOTAL_TIMEOUT_MS = 8000;  // 8s total (must be >= HTTP_RESPONSE_TIMEOUT_MS)

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
  
  // FIX: Set timeouts BEFORE begin()
  httpClient->setConnectTimeout(HTTP_CONNECT_TIMEOUT_MS);
  httpClient->setTimeout(HTTP_RESPONSE_TIMEOUT_MS);
  
  // Attempt connection with proper error handling
  if (!httpClient->begin(url)) {
    Serial.println("❌ Failed to initialize HTTP connection");
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
    currentState = FAILED;
    cleanupHTTP();
    return;
  }
  
  // FIX: Use proper HTTP status codes for clear semantics
  // Response received! Parse based on status code
  if (httpResponseCode == 200) {
    // 200 OK - Face recognized
    String response = httpClient->getString();
    Serial.print("✅ CAM Response (200): ");
    Serial.println(response);
    
    faceMatched = true;
    currentState = SUCCESS;
    Serial.println("🎉 Face Authorized by ESP32-CAM!");
    cleanupHTTP();
    return;
  } 
  else if (httpResponseCode == 202) {
    // 202 Accepted - CAM still processing, retry next loop
    Serial.println("⏳ CAM processing face, retrying...");
    return;  // Keep waiting
  }
  else if (httpResponseCode == 401) {
    // 401 Unauthorized - Face not recognized
    String response = httpClient->getString();
    Serial.print("❌ CAM Response (401): ");
    Serial.println(response);
    
    currentState = FAILED;
    Serial.println("❌ Face NOT Authorized - access denied");
    cleanupHTTP();
    return;
  }
  else if (httpResponseCode == 500) {
    // 500 Internal Server Error - Camera or processing error
    String response = httpClient->getString();
    Serial.print("❌ CAM Error (500): ");
    Serial.println(response);
    
    currentState = FAILED;
    Serial.println("❌ Camera error on ESP32-CAM side");
    cleanupHTTP();
    return;
  }
  else {
    // Unexpected status code
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

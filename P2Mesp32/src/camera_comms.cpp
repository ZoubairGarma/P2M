// src/camera_comms.cpp (WROVER - Non-blocking Communication)
#include "camera_comms.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// ============================================
// CONFIGURATION
// ============================================
String camIP = "10.93.156.1";  // Replace with your ESP32-CAM's IP
const int HTTP_TIMEOUT_MS = 5000;  // 5 second timeout for HTTP request
const unsigned long REQUEST_TIMEOUT_MS = 8000;  // 8 second total timeout

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
  
  // Initialize HTTP client (will be used in updateCameraComms)
  if (httpClient != nullptr) {
    delete httpClient;
  }
  httpClient = new HTTPClient();
  
  String url = "http://" + camIP + "/authorize";
  
  // Set timeout and begin connection
  httpClient->setConnectTimeout(HTTP_TIMEOUT_MS);
  httpClient->setTimeout(HTTP_TIMEOUT_MS);
  
  if (!httpClient->begin(url)) {
    Serial.println("❌ Failed to begin HTTP connection");
    currentState = FAILED;
    delete httpClient;
    httpClient = nullptr;
    return;
  }
  
  // Transition to waiting state
  currentState = WAITING_RESPONSE;
  Serial.println("✏️  Waiting for ESP32-CAM response...");
}

// ============================================
// UPDATE FUNCTION (Call in main loop)
// ============================================
void updateCameraComms() {
  if (currentState != WAITING_RESPONSE || httpClient == nullptr) {
    return;
  }
  
  // Check timeout
  unsigned long elapsedTime = millis() - requestStartTime;
  if (elapsedTime > REQUEST_TIMEOUT_MS) {
    Serial.print("⏰ HTTP Request TIMEOUT after ");
    Serial.print(elapsedTime);
    Serial.println(" ms");
    currentState = TIMEOUT;
    cleanupHTTP();
    return;
  }
  
  // Send the request and get response code
  int httpResponseCode = httpClient->GET();
  
  // If response code is 0 or negative, request failed or in progress
  if (httpResponseCode <= 0) {
    Serial.println("⏳ Waiting for response...");
    return;  // Still waiting, return to main loop
  }
  
  // Response received!
  if (httpResponseCode == 200) {
    String response = httpClient->getString();
    Serial.print("✅ CAM Response (");
    Serial.print(httpResponseCode);
    Serial.print("): ");
    Serial.println(response);
    
    // Check for authorization response
    if (response.indexOf("FACE_OK") != -1) {
      faceMatched = true;
      currentState = SUCCESS;
      Serial.println("🎉 Face Authorized by ESP32-CAM!");
      cleanupHTTP();
      return;
    }

    // Handle intermediate processing state without failing immediately
    if (response.indexOf("FACE_PROCESSING") != -1 || response.indexOf("PROCESSING") != -1) {
      Serial.println("⏳ CAM still processing face, retrying until timeout...");
      return;
    }

    if (response.indexOf("FACE_UNKNOWN") != -1 || response.indexOf("UNKNOWN") != -1) {
      currentState = FAILED;
      Serial.println("❌ Face NOT Authorized - access denied");
      cleanupHTTP();
      return;
    }

    if (response.indexOf("CAMERA_ERROR") != -1) {
      currentState = FAILED;
      Serial.println("❌ Camera error returned by CAM");
      cleanupHTTP();
      return;
    }

    // Unknown response, keep waiting until timeout to allow the camera to finish.
    Serial.println("⚠️ Unknown CAM response, retrying until timeout...");
    return;
  } else {
    Serial.print("❌ HTTP Error ");
    Serial.print(httpResponseCode);
    Serial.println(" from CAM");
    currentState = FAILED;
  }
  
  cleanupHTTP();
}

// ============================================
// HELPER FUNCTIONS
// ============================================
bool isFaceAuthorized() {
  if (faceMatched && currentState == SUCCESS) {
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
    httpClient->end();
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
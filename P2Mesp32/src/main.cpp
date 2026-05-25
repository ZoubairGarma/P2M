// src/main.cpp (WROVER - CORRECTED & Optimized)
// Smart Gate Master Controller with Non-blocking Communication
#include <Arduino.h>
#include <WiFi.h>
#include "state_machine.h"
#include "ir_sensor.h"
#include "rfid_scanner.h"
#include "pir_sensor.h"
#include "blynk_manager.h"
#include "camera_comms.h"

// ============================================
// NETWORK CREDENTIALS
// ============================================
const char* ssid = "bartage";
const char* password = "zoubaxd55";

// ============================================
// TIMING CONSTANTS (All in milliseconds)
// ============================================
const unsigned long CAR_DETECTION_WINDOW = 10000;  // 10 seconds to scan card
const unsigned long CAM_RESPONSE_TIMEOUT = 40000;   // 40 seconds - face recognition is slow!
const unsigned long GATE_OPEN_DURATION = 5000;     // Gate stays open for 5 seconds
const unsigned long INTER_CAR_COOLDOWN = 3000;     // Wait 3 seconds before next car
const unsigned long WIFI_RECONNECT_INTERVAL = 5000; // Try WiFi reconnect every 5 seconds

// ============================================
// STATE TRACKING VARIABLES
// ============================================
SystemState systemState = STATE_IDLE;
SystemState previousState = STATE_IDLE;
String currentUID = "";

unsigned long stateEnteredTime = 0;
unsigned long lastAccessGrantedTime = 0;
unsigned long lastWifiCheckTime = 0;
bool stateJustEntered = false;

// ============================================
// HELPER: State name for debugging
// ============================================
const char* getStateName(SystemState state) {
  switch(state) {
    case STATE_IDLE:           return "IDLE";
    case STATE_CAR_DETECTED:   return "CAR_DETECTED";
    case STATE_RFID_SCANNED:   return "RFID_SCANNED";
    case STATE_REQUESTING_CAM: return "REQUESTING_CAM";
    case STATE_WAITING_CAM:    return "WAITING_CAM";
    case STATE_ACCESS_GRANTED: return "ACCESS_GRANTED";
    case STATE_ACCESS_DENIED:  return "ACCESS_DENIED";
    default:                   return "UNKNOWN";
  }
}

// ============================================
// HELPER: Change state with logging
// ============================================
void setState(SystemState newState) {
  if (systemState != newState) {
    Serial.print("🔄 State Transition: ");
    Serial.print(getStateName(systemState));
    Serial.print(" → ");
    Serial.println(getStateName(newState));
    
    previousState = systemState;
    systemState = newState;
    stateEnteredTime = millis();
    stateJustEntered = true;
  }
}

// ============================================
// HELPER: WiFi Connection with Timeout
// ============================================
void connectWiFiWithTimeout(unsigned long timeoutMs) {
  unsigned long startTime = millis();
  int dotCount = 0;
  
  Serial.print("🔗 Connecting to Wi-Fi: ");
  Serial.println(ssid);
  
  WiFi.begin(ssid, password);
  
  while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < timeoutMs) {
    delay(500);
    Serial.print(".");
    dotCount++;
    if (dotCount % 10 == 0) Serial.println();  // New line every 5 seconds
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("✅ Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("⚠️  WiFi connection timeout - will retry in background");
  }
}

// ============================================
// SETUP
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  Serial.println("\n================================");
  Serial.println("🚀 SMART GATE SYSTEM STARTING");
  Serial.println("================================\n");
  
  // 1. Initialize Hardware Pins
  Serial.println("📍 Initializing hardware sensors...");
  setupIR();
  setupRFID();
  setupPIR();
  setupCameraComms();
  
  // 2. Connect to Wi-Fi with timeout (FIX: Don't hang indefinitely)
  connectWiFiWithTimeout(10000);  // 10 second timeout
  lastWifiCheckTime = millis();
  
  // 3. Initialize Blynk (after Wi-Fi attempt)
  Serial.println("☁️  Initializing Blynk connection...");
  setupBlynk();
  
  // 4. Print memory info
  Serial.println("\n📊 System Ready!");
  printCameraCommMemory();
  
  // 5. TEST CAMERA CONNECTIVITY (DIAGNOSTIC)
  delay(2000);  // Give camera time to boot
  testCameraConnection();
  
  Serial.println("⏳ Waiting for car...\n");
}

// ============================================
// MAIN LOOP (FIX: Removed delay(50), replaced with event-driven updates)
// ============================================
void loop() {
  unsigned long currentTime = millis();
  
  // ==========================================
  // BACKGROUND SERVICES (Always Running)
  // ==========================================
  runBlynk();                 // Keep cloud connection alive
  updateBlynkStatus(systemState, currentUID);  // Update Blynk UI
  updateBlynkSystemHealth();  // Send health metrics to Blynk
  handlePIR();                // Monitor motion
  updateCameraComms();        // FIX: This is NON-BLOCKING now - safe to call frequently
  
  // FIX: Graceful WiFi recovery - check every 5 seconds
  if (currentTime - lastWifiCheckTime > WIFI_RECONNECT_INTERVAL) {
    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("⚠️  Wi-Fi disconnected, attempting reconnect...");
      WiFi.reconnect();
    }
    lastWifiCheckTime = currentTime;
  }
  
  bool justEnteredState = stateJustEntered;
  
  // ==========================================
  // STATE MACHINE
  // ==========================================
  
  switch(systemState) {
    
    // ==========================================
    // IDLE STATE: Waiting for a car
    // ==========================================
    case STATE_IDLE: {
      if (checkCarPresence()) {
        Serial.println("\n🚗 Car Detected! You have 10 seconds to scan your card...");
        setState(STATE_CAR_DETECTED);
      }
      break;
    }
    
    // ==========================================
    // CAR DETECTED: Waiting for RFID scan
    // ==========================================
    case STATE_CAR_DETECTED: {
      // Check if scanning window expired
      if (currentTime - stateEnteredTime > CAR_DETECTION_WINDOW) {
        Serial.println("⏰ Timeout! No card was scanned. Please reverse and try again.");
        setState(STATE_IDLE);
        break;
      }
      
      // Check for card scan
      String scannedUID = scanCard();
      if (scannedUID != "") {
        currentUID = scannedUID;
        Serial.print("📋 Card Scanned! UID: ");
        Serial.println(currentUID);
        setState(STATE_RFID_SCANNED);
      }
      break;
    }
    
    // ==========================================
    // RFID SCANNED: Check authorization database
    // ==========================================
    case STATE_RFID_SCANNED: {
      if (isAuthorized(currentUID)) {
        Serial.println("✅ Card is authorized! Requesting image from ESP32-CAM...");
        resetCameraComm();  // FIX: Reset camera state from any previous request
        requestFaceScan();  // Send non-blocking request (FIX: Now properly non-blocking)
        setState(STATE_REQUESTING_CAM);
      } else {
        Serial.println("❌ Card NOT authorized - Access Denied");
        setState(STATE_ACCESS_DENIED);
      }
      break;
    }
    
    // ==========================================
    // REQUESTING CAM: Just sent the request
    // ==========================================
    case STATE_REQUESTING_CAM: {
      // Move to waiting state immediately (request sent above)
      setState(STATE_WAITING_CAM);
      break;
    }
    
    // ==========================================
    // WAITING CAM: Async HTTP response pending (FIX: Proper timeout handling)
    // ==========================================
    case STATE_WAITING_CAM: {
      // Check if CAM responded (FIX: This is now safe, isFaceAuthorized() is atomic)
      if (isFaceAuthorized()) {
        Serial.println("🎉 Face Authorized by CAM! ACCESS GRANTED!");
        setState(STATE_ACCESS_GRANTED);
      }
      
      // FIX: Check for timeout or error state from CAM
      if (currentTime - stateEnteredTime > CAM_RESPONSE_TIMEOUT) {
        // Check if CAM gave us a failure response (not just timeout)
        if (getCameraCommState() == FAILED || getCameraCommState() == TIMEOUT) {
          Serial.println("⏰ CAM response failed - Access Denied");
        } else {
          Serial.println("⏰ CAM response timeout - Assuming face not recognized");
        }
        setState(STATE_ACCESS_DENIED);
      }
      break;
    }
    
    // ==========================================
    // ACCESS GRANTED: Open the gate!
    // ==========================================
    case STATE_ACCESS_GRANTED: {
      // On first entry to this state, trigger gate/servo
      if (justEnteredState) {
        // FIX: Cancel camera request timeout to prevent false denial
        resetCameraComm();  // Clear pending camera request state
        
        Serial.println("\n🔓 OPENING GATE...");
        Serial.println("📌 [TODO] Trigger servo motor here!");
        logAccessEvent(true, currentUID);  // Log to Blynk
        lastAccessGrantedTime = millis();
        // servoOpen();  // Uncomment when servo code is ready
      }
      
      // Keep gate open for duration
      if (currentTime - stateEnteredTime > GATE_OPEN_DURATION) {
        Serial.println("🔒 Closing gate - duration expired");
        // servoClose();  // Uncomment when servo code is ready
        setState(STATE_IDLE);
      }
      break;
    }
    
    // ==========================================
    // ACCESS DENIED: Wait before next attempt
    // ==========================================
    case STATE_ACCESS_DENIED: {
      // Log denied access on first entry
      if (justEnteredState) {
        // FIX: Cancel camera request to prevent lingering timeouts
        resetCameraComm();  // Clear pending camera request state
        logAccessEvent(false, currentUID);  // Log to Blynk
      }
      
      // Wait for inter-car cooldown
      if (currentTime - stateEnteredTime > INTER_CAR_COOLDOWN) {
        Serial.println("↩️  Returning to IDLE state\n");
        setState(STATE_IDLE);
      }
      break;
    }
  }
  stateJustEntered = false;
  
  // ==========================================
  // PERIODIC DIAGNOSTICS (every 5 seconds)
  // ==========================================
  static unsigned long lastDiagnosticTime = 0;
  if (currentTime - lastDiagnosticTime > 5000) {
    if (systemState == STATE_IDLE) {  // Only in IDLE to avoid spam
      // Print memory status periodically
      // printCameraCommMemory();
    }
    lastDiagnosticTime = currentTime;
  }
  
  // FIX: REMOVED delay(50) - This loop should be as responsive as possible
  // The loop will naturally rate-limit due to sensor polling and HTTP operations
  // Removing the delay ensures the state machine responds quickly to events
}

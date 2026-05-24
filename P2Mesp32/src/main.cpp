// src/main.cpp (WROVER - The Brain)
// Smart Gate Master Controller with Non-blocking Communication
#include <Arduino.h>
#include <WiFi.h>
#include "state_machine.h"
#include "ir_sensor.h"
#include "rfid_scanner.h"
#include "pir_sensor.h"
#include "blynk_manager.h"
#include "camera_comms.h"
#include "state_machine.h" // Add this for SystemState definition
// ============================================
// NETWORK CREDENTIALS
// ============================================
const char* ssid = "bartage";
const char* password = "zoubaxd55";

// TIMING CONSTANTS (All in milliseconds)
// ============================================
const unsigned long CAR_DETECTION_WINDOW = 10000;  // 10 seconds to scan card
const unsigned long CAM_RESPONSE_TIMEOUT = 8000;   // 8 seconds to get response from CAM
const unsigned long GATE_OPEN_DURATION = 5000;     // Gate stays open for 5 seconds
const unsigned long INTER_CAR_COOLDOWN = 3000;     // Wait 3 seconds before next car

// ============================================
// STATE TRACKING VARIABLES
// ============================================
SystemState systemState = STATE_IDLE;
SystemState previousState = STATE_IDLE;
String currentUID = "";

unsigned long stateEnteredTime = 0;
unsigned long lastAccessGrantedTime = 0;
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
  
  // 2. Connect to Wi-Fi
  Serial.print("🔗 Connecting to Wi-Fi: ");
  Serial.println(ssid);
  WiFi.begin(ssid, password);
  
  int wifiAttempts = 0;
  while (WiFi.status() != WL_CONNECTED && wifiAttempts < 20) {
    delay(500);
    Serial.print(".");
    wifiAttempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("\n✅ Connected! IP: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\n❌ Wi-Fi connection failed!");
    // Continue anyway, may reconnect automatically
  }

  // 3. Initialize Blynk (after Wi-Fi)
  Serial.println("☁️  Initializing Blynk connection...");
  setupBlynk();
  
  // 4. Print memory info
  Serial.println("\n📊 System Ready!");
  printCameraCommMemory();
  Serial.println("⏳ Waiting for car...\n");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  // ==========================================
  // BACKGROUND SERVICES (Always Running)
  // ==========================================
  runBlynk();                 // Keep cloud connection alive
  updateBlynkStatus(systemState, currentUID);  // Update Blynk UI
  updateBlynkSystemHealth();  // Send health metrics to Blynk
  handlePIR();                // Monitor motion
  updateCameraComms();        // Process async HTTP responses
  
  unsigned long currentTime = millis();
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
        requestFaceScan();  // Send non-blocking request
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
      // Move to waiting state immediately
      setState(STATE_WAITING_CAM);
      break;
    }
    
    // ==========================================
    // WAITING CAM: Async HTTP response pending
    // ==========================================
    case STATE_WAITING_CAM: {
      // Check if CAM responded
      if (isFaceAuthorized()) {
        Serial.println("🎉 Face Authorized by CAM! ACCESS GRANTED!");
        setState(STATE_ACCESS_GRANTED);
      }
      
      // Check timeout
      if (currentTime - stateEnteredTime > CAM_RESPONSE_TIMEOUT) {
        Serial.println("⏰ CAM response timeout - Assuming face not recognized");
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
      // Check Wi-Fi status
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⚠️  Wi-Fi disconnected, reconnecting...");
        WiFi.begin(ssid, password);
      }
    }
    lastDiagnosticTime = currentTime;
  }
  
  delay(50);  // Small debounce delay
}
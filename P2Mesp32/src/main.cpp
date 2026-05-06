// src/main.cpp
#include <Arduino.h>
#include <WiFi.h>
#include "ir_sensor.h"
#include "rfid_scanner.h"
#include "pir_sensor.h"      // Added PIR
#include "blynk_manager.h"   // Added Blynk
#include "camera_comms.h"    // Added Camera

const char* password = "zoubaxd55";
const char* ssid = "bartage"; 

// System state tracking
bool waitingForScan = false;
unsigned long waitingStartTime = 0;
const unsigned long SCAN_TIMEOUT = 10000; // 10 seconds to scan card

void setup() {
  Serial.begin(115200);
  
  // 1. Initialize Hardware Pins
  setupIR();     
  setupRFID();   
  setupPIR();    // Initialize PIR hardware
  setupCameraComms(); // Initialize Camera Comms
  
  // 2. Connect to Wi-Fi
  Serial.print("Connecting to Wi-Fi");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWROVER Connected to Wi-Fi!");

  // 3. Initialize Cloud Services (MUST happen after Wi-Fi connects)
  setupBlynk();  // Piggyback Blynk onto the Wi-Fi
  
  delay(1000);
  Serial.println("Smart Gate Ready! Waiting for car...");
  Serial.println("------------------------------------");
}

void loop() {
  // ==========================================
  // 0. BACKGROUND SECURITY TASKS
  // ==========================================
  runBlynk();  // Keeps the cloud connection alive
  handlePIR(); // Checks for motion and alerts guard in the background

  unsigned long currentTime = millis();

  // ==========================================
  // 1. IR SENSOR LOGIC
  // ==========================================
  if (checkCarPresence()) {
    Serial.println("🚗 Car Detected! You have 10 seconds to scan your card...");
    waitingForScan = true;
    waitingStartTime = currentTime;
  }

  // ==========================================
  // 2. TIMEOUT LOGIC
  // ==========================================
  if (waitingForScan && (currentTime - waitingStartTime >= SCAN_TIMEOUT)) {
    Serial.println("⏰ Timeout! No card was scanned. Please reverse and try again.");
    waitingForScan = false; // Close the window
  }

  // ==========================================
  // 3. RFID SCANNER LOGIC
  // ==========================================
  if (waitingForScan) {
    String scannedUID = scanCard();
    
    if (scannedUID != "") { 
      Serial.print("Card Scanned! UID: ");
      Serial.println(scannedUID);
      
      // Check our database
      if (isAuthorized(scannedUID)) {
        Serial.println("✅ ACCESS GRANTED! Welcome.");
        // Signal ESP32-CAM to take a picture
        Serial.println("📸 Requesting ESP32-CAM to capture image...");
        requestFaceScan();
        // (Servo motor code will go here!)
      } else {
        Serial.println("❌ ACCESS DENIED! Unknown card.");
      }
      
      waitingForScan = false; // Close the scanning window
    }
  }

  delay(50); // Small debounce delay
}
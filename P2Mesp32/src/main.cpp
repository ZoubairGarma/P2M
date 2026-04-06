// src/main.cpp
#include <Arduino.h>
#include "ir_sensor.h"
#include "rfid_scanner.h"

// System state tracking
bool waitingForScan = false;
unsigned long waitingStartTime = 0;
const unsigned long SCAN_TIMEOUT = 10000; // 10 seconds to scan card

void setup() {
  Serial.begin(115200);
  
  setupIR();     // Initialize IR hardware
  setupRFID();   // Initialize RFID hardware
  
  delay(1000);
  Serial.println("Smart Gate Ready! Waiting for car...");
  Serial.println("------------------------------------");
}

void loop() {
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
      Serial.print("Card Scanned! UID:");
      Serial.println(scannedUID);
      
      // Check our new database!
      if (isAuthorized(scannedUID)) {
        Serial.println("✅ ACCESS GRANTED! Welcome.");
        // (We will add the code to physically open the gate here next)
      } else {
        Serial.println("❌ ACCESS DENIED! Unknown card.");
      }
      
      waitingForScan = false; // Close the scanning window
    }
  }

  delay(50); // Small debounce delay
}
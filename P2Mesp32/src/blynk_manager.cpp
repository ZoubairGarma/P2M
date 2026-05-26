#include "blynk_manager.h"
#include <Arduino.h>
// BLYNK CREDENTIALS - MUST BE AT THE VERY TOP
#define BLYNK_TEMPLATE_ID "TMPL28Hj90jCY"
#define BLYNK_TEMPLATE_NAME "smart gate"
#define BLYNK_AUTH_TOKEN "2Dbb0NIvOxFvIUDXVbiXHvSpmkWSqr5p"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

// ============================================
// GLOBAL COUNTERS & TRACKING
// ============================================
static int totalAccessCount = 0;
static unsigned long lastBlynkUpdate = 0;
static unsigned long lastHealthUpdate = 0;
static int lastHealthScore = -1;  // Track last value to detect changes

void setupBlynk() {
  // We don't use Blynk.begin() because you already manage Wi-Fi in main.cpp.
  // We use Blynk.config() to piggyback on the existing connection.
  Blynk.config(BLYNK_AUTH_TOKEN);
  Serial.println("Blynk Manager configured.");
}

void runBlynk() {
  Blynk.run(); // Keeps the connection to the cloud alive
}

void sendMotionAlert() {
  Serial.println("🚨 Motion Detected - Updating Dashboard LED...");
  Blynk.virtualWrite(V1, 1); // Turn ON Virtual LED on Pin V1
  
  // ❌ REMOVED: Blynk.logEvent("motion_alert", "Someone is at the gate!"); 
  // We remove the event so it doesn't eat your 100 daily limit!
}

void clearMotionAlert() {
  Blynk.virtualWrite(V1, 0); // Turn OFF Virtual LED
}

// ============================================
// MANUAL GATE CONTROL (V2 - Button)
// ============================================
BLYNK_WRITE(V2) {
  int pin_value = param.asInt();
  if (pin_value == 1) {
    Serial.println("🔓 MANUAL GATE OPEN (via Blynk)");
    Blynk.logEvent("manual_control", "Gate opened manually via app");
    // TODO: Uncomment when servo code is ready
    // servoOpen();
  } else {
    Serial.println("🔒 MANUAL GATE CLOSE (via Blynk)");
    Blynk.logEvent("manual_control", "Gate closed manually via app");
    // TODO: Uncomment when servo code is ready
    // servoClose();
  }
}

// ============================================
// UPDATE BLYNK STATUS DISPLAY (V0, V3, V4, V5)
// ============================================
void updateBlynkStatus(SystemState state, String cardUID) {
  unsigned long currentTime = millis();
  
  // Update every 2 seconds to avoid flooding
  if (currentTime - lastBlynkUpdate < 2000) {
    return;
  }
  lastBlynkUpdate = currentTime;
  
  // V0: System State Display
  const char* stateNames[] = {"IDLE", "CAR_DETECTED", "RFID_SCANNED", 
                             "REQUESTING_CAM", "WAITING_CAM", "ACCESS_GRANTED", "ACCESS_DENIED"};
  Blynk.virtualWrite(V0, stateNames[state]);
  
  // V4: Access Counter
  Blynk.virtualWrite(V4, totalAccessCount);
  
  // V5: Last Scanned Card
  if (cardUID != "") {
    Blynk.virtualWrite(V5, "Card: " + cardUID);
  } else {
    Blynk.virtualWrite(V5, "No card");
  }
}

// ============================================
// UPDATE SYSTEM HEALTH (V3, V6)
// ============================================
void updateBlynkSystemHealth() {
  unsigned long currentTime = millis();
  
  // Update health every 1 second (not every loop iteration)
  if (currentTime - lastHealthUpdate < 1000) {
    return;
  }
  lastHealthUpdate = currentTime;
  
  // V3: WiFi & Memory Status
  int wifiSignal = WiFi.RSSI();  // Signal strength in dBm
  int freeRam = ESP.getFreeHeap() / 1024;  // KB
  String statusMsg = "WiFi: " + String(wifiSignal) + "dBm | RAM: " + String(freeRam) + "KB";
  Blynk.virtualWrite(V3, statusMsg);
  
  // V6: System Health Gauge (0-100%)
  // Health based on: WiFi signal, Free RAM, and PSRAM if available
  int healthScore = 100;
  
  // WiFi signal impact (worst: -90, best: -30)
  if (wifiSignal < -85) healthScore -= 40;
  else if (wifiSignal < -75) healthScore -= 25;
  else if (wifiSignal < -60) healthScore -= 10;
  
  // RAM impact (warning below 150KB)
  if (freeRam < 80) healthScore -= 50;
  else if (freeRam < 150) healthScore -= 25;
  else if (freeRam < 250) healthScore -= 10;
  
  // PSRAM impact if available
  if (psramFound()) {
    int psramFree = ESP.getFreePsram() / 1024;
    if (psramFree < 100) healthScore -= 20;
    else if (psramFree < 300) healthScore -= 10;
  }
  
  // Add time-based variation (makes it "live") - small fluctuation
  // This ensures the gauge updates even if nothing major changes
  static unsigned long lastHeartbeat = 0;
  unsigned long timeSinceLastHeartbeat = (currentTime - lastHeartbeat) / 1000;
  if (timeSinceLastHeartbeat > 0 && timeSinceLastHeartbeat < 60) {
    // Add tiny fluctuation based on heartbeat (±2%)
    healthScore += (timeSinceLastHeartbeat % 5) - 2;
  }
  lastHeartbeat = currentTime;
  
  healthScore = constrain(healthScore, 0, 100);
  
  // ALWAYS send, don't cache (forces update on Blynk)
  Blynk.virtualWrite(V6, healthScore);
}

// ============================================
// LOG ACCESS EVENTS
// ============================================
void logAccessEvent(bool granted, String cardUID) {
  if (granted) {
    Serial.println("✅ Access Granted - Logging to Blynk");
    Blynk.logEvent("access_granted", "Card: " + cardUID);
    totalAccessCount++;
    incrementAccessCounter();
  } else {
    Serial.println("❌ Access Denied - Logging to Blynk");
    Blynk.logEvent("access_denied", "Unauthorized: " + cardUID);
  }
}

// ============================================
// INCREMENT ACCESS COUNTER
// ============================================
void incrementAccessCounter() {
  Blynk.virtualWrite(V4, totalAccessCount);
  Serial.print("📊 Total Access Count: ");
  Serial.println(totalAccessCount);
}
#include "blynk_manager.h"
#include <Arduino.h>

// BLYNK CREDENTIALS - MUST BE AT THE VERY TOP
#define BLYNK_TEMPLATE_ID "TMPLxxxxxx"
#define BLYNK_TEMPLATE_NAME "Smart Gate"
#define BLYNK_AUTH_TOKEN "YourAuthTokenHere"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>

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
  Serial.println("🚨 Alerting Security Guard via Blynk...");
  Blynk.virtualWrite(V1, 1); // Turn ON Virtual LED on Pin V1
  Blynk.logEvent("motion_alert", "Someone is at the gate!"); // Push Notification
}

void clearMotionAlert() {
  Blynk.virtualWrite(V1, 0); // Turn OFF Virtual LED
}
#include "pir_sensor.h"
#include "blynk_manager.h" // Include Blynk so the PIR can trigger alerts
#include <Arduino.h>

const int PIR_PIN = 12 ; // Replace with your actual pin
unsigned long lastMotionTime = 0;
const unsigned long cooldownTime = 10000; // 10 seconds between alerts

void setupPIR() {
  pinMode(PIR_PIN, INPUT);
  Serial.println("PIR Motion Sensor ready.");
}

void handlePIR() {
  if (digitalRead(PIR_PIN) == HIGH) {
    // Check if 10 seconds have passed since the last alert
    if (millis() - lastMotionTime > cooldownTime) {
      sendMotionAlert(); // Fire the Blynk alert
      lastMotionTime = millis(); // Reset the timer
    }
  } else {
    clearMotionAlert(); // Turn off the Blynk LED if no motion
  }
}
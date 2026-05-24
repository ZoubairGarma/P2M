#include "pir_sensor.h"
#include "blynk_manager.h" // Include Blynk so the PIR can trigger alerts
#include <Arduino.h>

const int PIR_PIN = 12 ; // Replace with your actual pin
unsigned long lastMotionTime = 0;
const unsigned long cooldownTime = 10000; // 10 seconds between alerts
static bool motionAlertActive = false;

void setupPIR() {
  pinMode(PIR_PIN, INPUT);
  Serial.println("PIR Motion Sensor ready.");
}

void handlePIR() {
  if (digitalRead(PIR_PIN) == HIGH) {
    // Send one alert per motion event, then only re-alert after cooldown
    if (!motionAlertActive || millis() - lastMotionTime > cooldownTime) {
      sendMotionAlert(); // Fire the Blynk alert
      motionAlertActive = true;
      lastMotionTime = millis(); // Reset the timer
    }
  } else {
    if (motionAlertActive) {
      clearMotionAlert(); // Turn off the Blynk LED if no motion
      motionAlertActive = false;
    }
  }
}
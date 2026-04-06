// src/ir_sensor.cpp
#include <Arduino.h>
#include "ir_sensor.h"

const int IR_PIN = 14; 
int lastSensorState = HIGH; 
unsigned long lastDetectionTime = 0;
const unsigned long DETECTION_COOLDOWN = 5000;

void setupIR() {
  pinMode(IR_PIN, INPUT_PULLUP);
}

bool checkCarPresence() {
  int sensorValue = digitalRead(IR_PIN);
  unsigned long currentTime = millis();
  bool carDetected = false;

  if (sensorValue == LOW && lastSensorState == HIGH) {
    if (currentTime - lastDetectionTime >= DETECTION_COOLDOWN) {
      carDetected = true; // Tell the main code a car arrived!
      lastDetectionTime = currentTime;
    }
  }
  
  if (sensorValue == HIGH && lastSensorState == LOW) {
    Serial.println("... Gate empty, waiting ...");
  }
  
  lastSensorState = sensorValue;
  return carDetected;
}
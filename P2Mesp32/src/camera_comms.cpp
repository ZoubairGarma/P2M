// src/camera_comms.cpp (WROVER)
#include "camera_comms.h"
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

// REPLACE THIS WITH YOUR ESP32-CAM'S ACTUAL IP ADDRESS
String camIP = "10.169.58.1";

bool faceMatched = false;

void setupCameraComms() {
  Serial.println("Wi-Fi HTTP Comms Ready. Waiting for main Wi-Fi connection...");
}

void requestFaceScan() {
  Serial.println("📸 Sending HTTP GET request to ESP32-CAM...");
  faceMatched = false; 
  
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;
    
    // The WROVER "visits" the /authorize webpage on the CAM
    String url = "http://" + camIP + "/authorize";
    http.begin(url);
    
    int httpResponseCode = http.GET();
    
    if (httpResponseCode > 0) {
      String response = http.getString();
      if (response == "ACCESS_GRANTED") {
        faceMatched = true;
      }
    } else {
      Serial.print("❌ Error reaching CAM. HTTP Code: ");
      Serial.println(httpResponseCode);
    }
    
    http.end(); // Free up resources
  } else {
    Serial.println("❌ WROVER is not connected to Wi-Fi!");
  }
}

bool isFaceAuthorized() {
  if (faceMatched) {
    faceMatched = false; // Reset for the next car
    return true;
  }
  return false; 
}
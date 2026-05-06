#include <Arduino.h>
#include <WiFi.h>
#define THINGER_MAX_MESSAGE_SIZE 32768
#include <ThingerESP32.h>
#include "config.h"
#include "camera.h"
#include "esp_camera.h"
#include "reseau.h"
String derniereImageBase64 = "";

// Variable pour tracer l'état précédent du RFID
bool previousRfidDetected = false;

// Variables pour la capture en série
bool captureSeriesActive = false;
unsigned long captureSeriesStartTime = 0;

#define USERNAME "inesss"
#define DEVICE_ID "cameraa"
#define DEVICE_CREDENTIAL "cameraa"

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
WebServer server(80);
bool accessGranted = false;
unsigned long accessGrantedTimestamp = 0;
bool rfidDetected = false;
unsigned long rfidDetectedTimestamp = 0;

const char* ssid = "bartage";
const char* password = "zoubaxd55";

void setup() {
  Serial.begin(115200);
  delay(1000);

  initialiserCamera();
  initialiserWiFiEtCloud();

  // Existing RFID status resource
  thing["rfid_status"] << [](pson &out) {
    if (rfidDetected && millis() - rfidDetectedTimestamp < RFID_STATUS_DURATION_MS) {
      out = "OK";
    } else {
      out = "WAIT";
    }
  };

  // -----------------------------------------------------------
  // 📸 NOUVEAU: RESSOURCE IMAGE THINGER (Push en temps réel)
  // -----------------------------------------------------------
thing["image"] >> [](pson& out) {
    out = derniereImageBase64.c_str();
  };

  initialiserServeurWeb();
}

void loop() {
  thing.handle();
  server.handleClient();
  
  // ============================================
  // 🔄 MONITORING AUTOMATIQUE RFID
  // ============================================
  // Détecte un changement d'état RFID (false -> true)
  if (rfidDetected && !previousRfidDetected) {
    Serial.println("\n✅ RFID DETECTED! Triggering photo capture...");
    captureSeriesActive = true;
    captureSeriesStartTime = millis();
    previousRfidDetected = true;
  }
  // Réinitialiser quand le RFID retourne à WAIT
  else if (!rfidDetected && previousRfidDetected) {
    Serial.println("⏸️  RFID état retourné à WAIT");
    previousRfidDetected = false;
  }
  
  // ============================================
  // 📸 GESTION DES CAPTURES EN SÉRIE
  // ============================================
  if (captureSeriesActive) {
    unsigned long elapsedTime = millis() - captureSeriesStartTime;
    
    // Vérifier si on est encore dans la fenêtre de capture
    if (elapsedTime < PHOTO_CAPTURE_DURATION_MS) {
      // Vérifier si on doit capturer (intervalle entre photos)
      static unsigned long lastCaptureTime = 0;
      if (millis() - lastCaptureTime >= PHOTO_INTERVAL_MS) {
        capturePhotoAutomatic();
        lastCaptureTime = millis();
      }
    } else {
      // Fin de la série de captures
      captureSeriesActive = false;
      Serial.println("✔️  Série de captures terminée!");
    }
  }
}
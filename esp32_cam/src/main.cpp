#include <Arduino.h>
#include <WiFi.h>
#define THINGER_MAX_MESSAGE_SIZE 32768
#include <ThingerESP32.h>
#include "config.h"
#include "camera.h"
#include "esp_camera.h"
#include "reseau.h"
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

String derniereImageBase64 = "";

// Variable pour tracer l'état précédent du RFID
bool previousRfidDetected = false;

// Variables pour la capture en série
bool captureSeriesActive = false;
unsigned long captureSeriesStartTime = 0;
unsigned long lastCaptureTime = 0;  // FIX: GLOBAL variable instead of static!

// Variables pour enrôlement (reconnaissance faciale)
bool enrollMode = false;
String enrollingEmployeeName = "";

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
  Serial.println("\n\n=== ESP32-CAM Démarrage ===\n");

  // Initialiser NVS pour les visages
  initializeNVS();

  initialiserCamera();
  initialiserWiFiEtCloud();

  // ===== RESSOURCES THINGER =====
  
  // RFID Status
  thing["rfid_status"] << [](pson &out) {
    if (rfidDetected && millis() - rfidDetectedTimestamp < RFID_STATUS_DURATION_MS) {
      out = "OK";
    } else {
      out = "WAIT";
    }
  };

  // Image en temps réel
  thing["image"] >> [](pson& out) {
    out = derniereImageBase64.c_str();
  };

  // ===== NOUVEAU: Ressource ENROLL (Bouton d'enrôlement) =====
  thing["enroll"] << [](pson& in) {
    if (in.is_empty()) return;
    
    String employeeName = (const char*)in;
    if (employeeName.length() > 0) {
      Serial.printf("\n🎯 ENRÔLEMENT DEMANDÉ: %s\n", employeeName.c_str());
      
      enrollMode = true;
      enrollingEmployeeName = employeeName;
      captureSeriesActive = true;
      captureSeriesStartTime = millis();
      
      sendTelegramMessage("📝 Enrôlement en cours pour: " + employeeName);
    }
  };

  // Statut liste des visages
  thing["faces_list"] >> [](pson& out) {
    int count = getNVSFaceCount();
    out = count;
  };

  initialiserServeurWeb();
  
  // Afficher la liste des visages enregistrés au démarrage
  listAllEnrolledFaces();
  Serial.println("\n✅ ESP32-CAM prêt!\n");
}

void loop() {
  // FIX: Don't block on Telegram every loop iteration
  // Only handle Thinger connection every 500ms
  static unsigned long lastThingerHandle = 0;
  if (millis() - lastThingerHandle > 500) {
    thing.handle();
    lastThingerHandle = millis();
  }
  
  // ✅ CRITICAL: Handle web server FREQUENTLY so /authorize responds immediately
  server.handleClient();
  delay(10);  // Yield to other tasks briefly
  
  // ============================================
  // 🔄 MONITORING AUTOMATIQUE RFID
  // ============================================
  if (rfidDetected && !previousRfidDetected) {
    Serial.println("\n✅ RFID DETECTED! Triggering photo capture...");
    captureSeriesActive = true;
    captureSeriesStartTime = millis();
    previousRfidDetected = true;
  }
  else if (!rfidDetected && previousRfidDetected) {
    Serial.println("⏸️  RFID état retourné à WAIT");
    previousRfidDetected = false;
  }
  
  // ============================================
  // 📸 GESTION DES CAPTURES EN SÉRIE (FIX: No static, proper state reset)
  // ============================================
  if (captureSeriesActive) {
    unsigned long elapsedTime = millis() - captureSeriesStartTime;
    
    // FIX: Initialize lastCaptureTime on FIRST entry
    if (elapsedTime < 100) {
      lastCaptureTime = millis();
    }
    
    if (elapsedTime < PHOTO_CAPTURE_DURATION_MS) {
      // FIX: Use GLOBAL lastCaptureTime instead of static
      if (millis() - lastCaptureTime >= PHOTO_INTERVAL_MS) {
        capturePhotoAutomatic();
        lastCaptureTime = millis();  // Reset immediately after capture
      }
    } else {
      // SERIES COMPLETE - CLEANUP ALL STATE
      captureSeriesActive = false;
      enrollMode = false;
      rfidDetected = false;  // FIX: CLEAR RFID FLAG!
      rfidDetectedTimestamp = 0;  // FIX: CLEAR TIMESTAMP!
      previousRfidDetected = false;  // Reset tracking
      lastCaptureTime = 0;  // FIX: Reset capture timer
      Serial.println("✔️  Série de captures terminée! État réinitialisé.");
    }
  }
}
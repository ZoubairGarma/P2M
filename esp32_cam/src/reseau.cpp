// src/reseau.cpp (ESP32-CAM - CORRECTED, with WiFi Timeout)
#include "reseau.h"
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include <ThingerESP32.h>
#include <esp_camera.h>

extern ThingerESP32 thing;
extern bool enrollMode;
extern String enrollingEmployeeName;
extern String derniereImageBase64;
extern bool rfidDetected;
extern unsigned long rfidDetectedTimestamp;
extern bool captureSeriesActive;
extern unsigned long captureSeriesStartTime;

// FIX: WiFi timeout constant
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // 15 seconds to connect to WiFi
const unsigned long MAX_FACE_RECOGNITION_TIME_MS = 5000;  // Max 5 seconds for face recognition

void initialiserWiFiEtCloud() {
  Serial.print("Connexion WiFi...");
  WiFi.begin(ssid, password);
  
  // FIX: TIMEOUT-PROTECTED WiFi connection (was infinite loop!)
  unsigned long wifiStartTime = millis();
  while (WiFi.status() != WL_CONNECTED && 
         (millis() - wifiStartTime) < WIFI_CONNECT_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  
  Serial.println();
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("✅ WiFi connecté!");
    Serial.print(">>> Adresse IP de la caméra : ");
    Serial.println(WiFi.localIP());
    
    thing.add_wifi(ssid, password);
    
    // Initialiser Telegram après WiFi connecté
    Serial.println("🤖 Initialisation Telegram Bot...");
    sendTelegramMessage("✅ ESP32-CAM démarré et prêt!");
  } else {
    // FIX: Don't hang - just log and continue
    // WiFi might still reconnect automatically or be configured via web portal
    Serial.println("⚠️  WiFi connection timeout - Device may have limited functionality");
    Serial.println("❌ Check SSID/Password in config.h");
  }
}

void initialiserServeurWeb() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Camera prete.");
  });

  // ============================================
  // 🔄 ENDPOINT RFID DU WROVER
  // ============================================
  server.on("/rfid/trigger", HTTP_GET, []() {
    Serial.println("\n🚀 Endpoint /rfid/trigger appelé par WROVER!");
    rfidDetected = true;
    rfidDetectedTimestamp = millis();
    Serial.println("✅ rfidDetected = true - Capture va démarrer automatiquement!");
    server.send(200, "text/plain", "RFID_TRIGGERED");
  });

  // ============================================
  // 📝 ENDPOINT ENRÔLEMENT MANUEL (HTTP)
  // ============================================
  server.on("/enroll", HTTP_GET, []() {
    String name = server.arg("name");
    if (name.length() == 0) {
      server.send(400, "text/plain", "Paramètre 'name' requis");
      return;
    }
    
    Serial.printf("\n📝 Enrôlement HTTP demandé: %s\n", name.c_str());
    enrollMode = true;
    enrollingEmployeeName = name;
    captureSeriesActive = true;
    captureSeriesStartTime = millis();
    
    server.send(200, "text/plain", "Enrôlement lancé pour: " + name);
  });

  // Routes existantes
  server.on("/photo", HTTP_GET, []() {
    server.send(200, "text/plain", "Photos capturées automatiquement au RFID");
  });
  
  // ============================================
  // 🔐 MAIN AUTHORIZATION ENDPOINT (COMPLETELY CORRECTED)
  // ============================================
  server.on("/authorize", HTTP_GET, []() {
    Serial.println("\n🚀 Endpoint /authorize appelé par WROVER!");
    
    unsigned long recognitionStartTime = millis();
    
    // ============================================
    // STEP 1: Capture photo
    // ============================================
    camera_fb_t* fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Erreur capture");
      server.send(500, "text/plain", "Camera capture failed");
      return;  // fb is NULL, safe to return
    }
    
    // ============================================
    // STEP 2: Generate hash
    // ============================================
    uint8_t* hash = generateFaceHash(fb);
    if (hash == nullptr) {
      Serial.println("❌ Erreur génération hash");
      esp_camera_fb_return(fb);  // FIX: Return frame buffer on error
      server.send(500, "text/plain", "Face hash generation failed");
      return;
    }
    
    unsigned long hashTime = millis() - recognitionStartTime;
    if (hashTime > MAX_FACE_RECOGNITION_TIME_MS) {
      Serial.println("⏰ Face hash generation timeout!");
      // FIX: CRITICAL - Use free() for both PSRAM and heap
      if (hash) free(hash);
      esp_camera_fb_return(fb);  // Always return frame buffer
      server.send(500, "text/plain", "Face processing timeout");
      return;
    }
    
    // ============================================
    // STEP 3: Recognize face
    // ============================================
    int faceIndex = recognizeFace(hash);
    unsigned long totalTime = millis() - recognitionStartTime;
    
    Serial.printf("Face recognition completed in %ld ms\n", totalTime);
    
    // ============================================
    // STEP 4: Handle result
    // ============================================
    if (faceIndex >= 0) {
      // Visage reconnu ✅
      FaceSignature face;
      if (!loadFaceFromNVS(faceIndex, &face)) {
        Serial.println("❌ Erreur chargement face NVS");
        // FIX: CRITICAL - Cleanup on all error paths!
        if (hash) free(hash);
        esp_camera_fb_return(fb);
        server.send(500, "text/plain", "Face load from NVS failed");
        return;
      }
      
      Serial.printf("✅ VISAGE RECONNU: %s (ID: %d)\n", face.name, faceIndex);
      
      // Telegram notification
      String msg = "✅ Accès accordé à " + String(face.name);
      sendTelegramMessage(msg);
      
      // FIX: CRITICAL - Cleanup BEFORE sending response
      if (hash) free(hash);
      esp_camera_fb_return(fb);
      
      delay(50);  // Let PSRAM settle before next request
      
      // FIX: Return 200 OK for success
      server.send(200, "text/plain", "FACE_OK");
    } else {
      // Visage inconnu - ALERTE ❌
      Serial.println("⚠️  VISAGE INCONNU - ALERTE");
      
      // Envoyer photo à Thinger
      String b64 = base64_encode(fb->buf, fb->len);
      derniereImageBase64 = "data:image/jpeg;base64," + b64;
      protoson::pson data = derniereImageBase64.c_str();
      thing.set_property("image", data);
      
      // Telegram alerte
      sendTelegramMessage("⚠️ ALERTE: Inconnu détecté! Photo enregistrée.");
      
      // FIX: CRITICAL - Cleanup BEFORE sending response
      if (hash) free(hash);
      esp_camera_fb_return(fb);
      
      delay(50);  // Let PSRAM settle before next request
      
      // FIX: Return 401 Unauthorized for face not recognized
      server.send(401, "text/plain", "FACE_UNKNOWN");
    }
  });

  // Route pour servir l'image
  server.on("/image", HTTP_GET, []() {
    Serial.println(">>> /image requested!");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Erreur capture pour /image");
      server.send(500, "text/plain", "Image capture failed");
      return;
    }
    Serial.printf("Image capturée: %d bytes\n", fb->len);
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Connection: close");
    client.printf("Content-Length: %d\r\n", fb->len);
    client.println();
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);  // FIX: Explicit frame buffer return
    delay(50);  // FIX: Let PSRAM settle
    Serial.println("Image envoyée!");
  });

  // ============================================
  // 🗑️ ENDPOINT POUR EFFACER LES VISAGES (DEBUG)
  // ============================================
  server.on("/faces/clear", HTTP_GET, []() {
    Serial.println("🗑️  Suppression de tous les visages enregistrés");
    nvs_handle_t nvs_handle;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_erase_all(nvs_handle);
    
    // FIX: Check commit result
    esp_err_t err = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    delay(100);  // FIX: Let system settle after erase
    
    if (err == ESP_OK) {
      server.send(200, "text/plain", "Tous les visages ont été supprimés");
      sendTelegramMessage("🗑️ Tous les visages ont été supprimés");
    } else {
      server.send(500, "text/plain", "Erreur suppression visages");
    }
  });

  // ============================================
  // 📋 ENDPOINT LISTE DES VISAGES
  // ============================================
  server.on("/faces/list", HTTP_GET, []() {
    int count = getNVSFaceCount();
    String response = "Visages enregistrés (" + String(count) + "):\n";
    
    for (int i = 0; i < count; i++) {
      FaceSignature face;
      if (loadFaceFromNVS(i, &face)) {
        response += String(i) + ": " + String(face.name) + "\n";
      }
    }
    
    server.send(200, "text/plain", response);
  });

  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);
  });

  server.onNotFound([]() {
    Serial.print("Route introuvable: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Route not found");
  });
  
  server.begin();
  Serial.println("✅ Serveur HTTP lancé!");
}

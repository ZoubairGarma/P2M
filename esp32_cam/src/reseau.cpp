// src/reseau.cpp (ESP32-CAM - CORRECTED, with WiFi Timeout)
#include "reseau.h"
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>

#include <esp_camera.h>

extern bool alerteIntrus;
extern bool enrollMode;
extern String enrollingEmployeeName;
extern String derniereImageBase64;
extern bool rfidDetected;
extern unsigned long rfidDetectedTimestamp;
extern bool captureSeriesActive;
extern unsigned long captureSeriesStartTime;
extern bool recognitionDone;

// WiFiClientSecure pour Telegram
WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOT_TOKEN, clientSecure);

// FIX: WiFi timeout constant
const unsigned long WIFI_CONNECT_TIMEOUT_MS = 15000;  // 15 seconds to connect to WiFi
// MAX_FACE_RECOGNITION_TIME_MS is defined in config.h

void initialiserWiFi() {
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
server.on("/authorize", HTTP_GET, []() {
    Serial.println("\n🚀 Endpoint /authorize - Starting Face AI Series...");
    
    unsigned long recognitionStartTime = millis();
    bool faceRecognized = false;
    String recognizedName = "";
    bool intruderPhotoSaved = false;

    // 📸 We take a SERIES of up to 5 pictures (giving the user 3-5 seconds to look at the camera)
    for (int tries = 1; tries <= 5; tries++) {
      Serial.printf("\n📸 Attempt %d/5...\n", tries);
      
      camera_fb_t* fb = esp_camera_fb_get();
      
      // WORKAROUND: If camera is sleepy or busy, clear it and retry
      if (!fb) {
        Serial.println("⚠️ Camera busy or asleep, retrying in 200ms...");
        delay(200);
        continue; // Skip to next attempt
      }

      // STEP 1: Detect face in this specific photo
      FaceDetectionResult detection = detectFaceAI(fb);
      if (!detection.detected) {
        Serial.println("⚠️ No face detected in this frame.");
        esp_camera_fb_return(fb); // ALWAYS return buffer
        delay(200);
        continue;
      }

      // STEP 2: Extract face embedding
      float embedding[FACE_EMBEDDING_SIZE];
      if (!extractFaceEmbedding(fb, embedding)) {
        Serial.println("⚠️ AI Extraction failed.");
        esp_camera_fb_return(fb);
        delay(200);
        continue;
      }

      // STEP 3: Recognize face
      int faceIndex = recognizeFaceAI(embedding);
      if (faceIndex >= 0) {
        FaceEmbedding face;
        if (loadFaceEmbedding(faceIndex, &face)) {
          faceRecognized = true;
          recognizedName = String(face.name);
          esp_camera_fb_return(fb);
          break; // SUCCESS! Break out of the 5-photo loop immediately!
        }
      }
      
      // STEP 4: If we are at try 5 and STILL unknown, save the photo as an intruder
      if (tries == 5) {
         Serial.println("❌ All attempts failed. Saving intruder photo...");
         String b64 = base64_encode(fb->buf, fb->len);
         derniereImageBase64 = "data:image/jpeg;base64," + b64;
         intruderPhotoSaved = true;
      }
      
      esp_camera_fb_return(fb); // Clear the pipe for the next photo
      delay(200); // Small pause before next shot
    }

    // ============================================
    // FINAL VERDICT AFTER THE SERIES OF PHOTOS
    // ============================================
    if (faceRecognized) {
      Serial.printf("✅ FACE RECOGNIZED: %s\n", recognizedName.c_str());
      
      // Telegram notification
      String msg = "✅ Access granted to " + recognizedName;
      sendTelegramMessage(msg);
      
      // Tell WROVER to open the gate
      server.send(200, "text/plain", "FACE_OK");
    } else {
      Serial.println("⚠️ UNKNOWN FACE OR NO FACE - ACCESS DENIED");
      
      alerteIntrus = true; // Tell main.cpp to send to Thinger
      
      if (intruderPhotoSaved) {
        sendTelegramMessage("⚠️ ALERT: Access Denied! Unrecognized person. Photo recorded.");
      } else {
        sendTelegramMessage("⚠️ ALERT: Access Denied! (Person moved away or camera failed)");
      }
      
      // Tell WROVER to keep gate closed
      server.send(401, "text/plain", "FACE_UNKNOWN");
    }
    
    unsigned long totalTime = millis() - recognitionStartTime;
    Serial.printf("⏱️ Authorization process finished in %ld ms\n", totalTime);
  });

  // ============================================
  // � ENDPOINT POUR AFFICHER LA DERNIÈRE PHOTO D'ENRÔLEMENT
  // ============================================
  server.on("/last_enroll", HTTP_GET, []() {
    Serial.println(">>> /last_enroll requested!");
    
    // Vérifier si une photo d'enrôlement a été stockée
    if (lastEnrolledPhoto == NULL || lastEnrolledSize == 0) {
      Serial.println("❌ Aucune photo d'enrôlement stockée");
      server.send(404, "text/plain", "No enrolled photo available");
      return;
    }
    
    Serial.printf("Envoi photo enrôlement: %d bytes\n", lastEnrolledSize);
    
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Connection: close");
    client.printf("Content-Length: %d\r\n", lastEnrolledSize);
    client.println();
    client.write(lastEnrolledPhoto, lastEnrolledSize);
    
    delay(50);  // FIX: Let PSRAM settle
    Serial.println("✅ Photo enrôlement envoyée!");
  });

  // ============================================
  // �🗑️ ENDPOINT POUR EFFACER LES VISAGES (DEBUG)
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
  // 🔧 RESET & ENROLL (One-shot: clear all + enroll immediately)
  // ============================================
  server.on("/reset-and-enroll", HTTP_GET, []() {
    String name = server.arg("name");
    if (name.length() == 0) {
      server.send(400, "text/plain", "Paramètre 'name' requis");
      return;
    }
    
    Serial.printf("\n🔧 RESET-AND-ENROLL: %s\n", name.c_str());
    
    // STEP 1: Clear all faces
    nvs_handle_t nvs_handle;
    nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    nvs_erase_all(nvs_handle);
    esp_err_t clearErr = nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    delay(100);
    
    if (clearErr != ESP_OK) {
      server.send(500, "text/plain", "Clear failed");
      return;
    }
    
    Serial.println("✅ All faces cleared!");
    
    // STEP 2: Start enrollment immediately
    enrollMode = true;
    enrollingEmployeeName = name;
    captureSeriesActive = true;
    captureSeriesStartTime = millis();
    
    Serial.printf("✅ Enrollment started for: %s\n", name.c_str());
    
    server.send(200, "text/plain", "Reset complete! Enrolling: " + name);
  });

  // ============================================
  server.on("/faces/list", HTTP_GET, []() {
    int count = getNVSFaceCount();
    String response = "Enrolled faces (" + String(count) + "):\n";
    
    // TODO: Load and display face names from NVS
    // For now just show count
    for (int i = 0; i < count; i++) {
      response += String(i) + ": <face>\n";
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

// ==================== TELEGRAM MESSAGE HANDLER ====================
void handleTelegramMessages() {
  // Check for Telegram messages (if WiFi connected and bot initialized)
  if (WiFi.status() == WL_CONNECTED) {
    if (bot.getUpdates(bot.last_message_received + 1)) {
      // UniversalTelegramBot stores a single message at a time
      Serial.println("📨 Telegram message received");
      
      // Optional: handle simple commands
      String text = bot.messages[0].text;
      if (text == "/status") {
        String reply = "🤖 ESP32-CAM is running!\n";
        reply += "📷 Face recognition active\n";
        reply += "✅ Ready to process faces";
        bot.sendMessage(bot.messages[0].chat_id, reply);
      }
    }
  }
}

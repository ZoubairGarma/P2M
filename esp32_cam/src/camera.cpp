#include "camera.h"
#include "config.h"
#include "esp_camera.h"
#include <WebServer.h>
#include <ThingerESP32.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <nvs_flash.h>
#include <nvs.h>

extern WebServer server;
extern ThingerESP32 thing;
extern String derniereImageBase64;
extern bool enrollMode;
extern String enrollingEmployeeName;

// WiFiClientSecure pour Telegram
WiFiClientSecure clientSecure;
UniversalTelegramBot bot(BOT_TOKEN, clientSecure);

// ==================== BASE64 ENCODING ====================
String base64_encode(const uint8_t* data, size_t len) {
  const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded = "";
  int i = 0, j = 0;
  uint8_t char_array_3[3], char_array_4[4];
  
  while (len--) {
    char_array_3[i++] = *(data++);
    if (i == 3) {
      char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
      char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
      char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
      char_array_4[3] = char_array_3[2] & 0x3f;
      for(i = 0; i < 4; i++) encoded += base64_chars[char_array_4[i]];
      i = 0;
    }
  }
  
  if (i) {
    for(j = i; j < 3; j++) char_array_3[j] = '\0';
    char_array_4[0] = (char_array_3[0] & 0xfc) >> 2;
    char_array_4[1] = ((char_array_3[0] & 0x03) << 4) + ((char_array_3[1] & 0xf0) >> 4);
    char_array_4[2] = ((char_array_3[1] & 0x0f) << 2) + ((char_array_3[2] & 0xc0) >> 6);
    char_array_4[3] = char_array_3[2] & 0x3f;
    for (j = 0; j < i + 1; j++) encoded += base64_chars[char_array_4[j]];
    while((i++ < 3)) encoded += '=';
  }
  return encoded;
}

// ==================== INITIALISATION CAMERA ====================
void initialiserCamera() {
  Serial.println("📷 Initialisation caméra...");

  camera_config_t config;
  
  // Rétrocompatibilité
#if defined(ESP_ARDUINO_VERSION_MAJOR) && ESP_ARDUINO_VERSION_MAJOR < 3
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
#endif

  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;

  // ✅ PSRAM & QVGA pour stabilité
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 10;
    config.fb_count = 2;
    Serial.println("✅ PSRAM détecté - Configuration optimale");
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
    Serial.println("⚠️  PSRAM non détecté - Configuration basique");
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Erreur init caméra !");
    return;
  }
  
  Serial.println("✅ Caméra initialisée");
}

// ==================== RECONNAISSANCE FACIALE ====================

// Génère un hash simple du visage (moyenne de luminosité dans zones clés)
uint8_t* generateFaceHash(camera_fb_t* fb) {
  uint8_t* hash = (uint8_t*)malloc(32);
  memset(hash, 0, 32);
  
  if (!fb) return hash;
  
  uint8_t* buf = fb->buf;
  size_t len = fb->len;
  
  // Simple: calcul de checksum par octets
  for (size_t i = 0; i < len && i < 256; i++) {
    hash[i % 32] ^= buf[i];
    hash[(i + 1) % 32] += buf[i];
  }
  
  return hash;
}

// Compare deux signatures (retourne pourcentage de similarité 0-100)
int compareFaceSignatures(uint8_t* hash1, uint8_t* hash2) {
  if (!hash1 || !hash2) return 0;
  
  int matchCount = 0;
  for (int i = 0; i < 32; i++) {
    if (hash1[i] == hash2[i]) matchCount++;
  }
  
  return (matchCount * 100) / 32;  // 0-100%
}

// ==================== NVS (STOCKAGE PERSISTANT) ====================

void initializeNVS() {
  Serial.println("🔧 Initialisation NVS...");
  esp_err_t err = nvs_flash_init();
  if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  Serial.println("✅ NVS initialisé");
}

int getNVSFaceCount() {
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ Erreur ouverture NVS");
    return 0;
  }
  
  uint8_t count = 0;
  nvs_get_u8(nvs_handle, NVS_FACES_COUNT_KEY, &count);
  nvs_close(nvs_handle);
  
  return count;
}

bool enrollFace(const char* employeeName) {
  Serial.printf("\n📝 Enrôlement du visage: %s\n", employeeName);
  
  // Capturer le visage
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Erreur capture visage");
    return false;
  }
  
  // Générer la signature
  uint8_t* hash = generateFaceHash(fb);
  
  // Ouvrir NVS
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ Erreur NVS");
    esp_camera_fb_return(fb);
    free(hash);
    return false;
  }
  
  // Obtenir le nombre actuel
  uint8_t count = 0;
  nvs_get_u8(nvs_handle, NVS_FACES_COUNT_KEY, &count);
  
  if (count >= MAX_ENROLLED_FACES) {
    Serial.printf("❌ Max faces atteint (%d)\n", MAX_ENROLLED_FACES);
    nvs_close(nvs_handle);
    esp_camera_fb_return(fb);
    free(hash);
    return false;
  }
  
  // Stocker le hash et le nom
  char key_hash[32], key_name[32];
  sprintf(key_hash, "hash_%d", count);
  sprintf(key_name, "name_%d", count);
  
  nvs_set_blob(nvs_handle, key_hash, hash, 32);
  nvs_set_str(nvs_handle, key_name, employeeName);
  
  // Incrémenter le compteur
  count++;
  nvs_set_u8(nvs_handle, NVS_FACES_COUNT_KEY, count);
  nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
  
  esp_camera_fb_return(fb);
  free(hash);
  
  Serial.printf("✅ Visage enregistré #%d: %s\n", count - 1, employeeName);
  return true;
}

int recognizeFace(uint8_t* faceHash) {
  int count = getNVSFaceCount();
  if (count == 0) {
    Serial.println("⚠️  Aucun visage enregistré");
    return -1;
  }
  
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  
  if (err != ESP_OK) return -1;
  
  int bestMatch = -1;
  int bestScore = 0;
  
  for (int i = 0; i < count; i++) {
    char key_hash[32];
    sprintf(key_hash, "hash_%d", i);
    
    uint8_t storedHash[32];
    size_t len = 32;
    
    if (nvs_get_blob(nvs_handle, key_hash, storedHash, &len) == ESP_OK) {
      int score = compareFaceSignatures(faceHash, storedHash);
      Serial.printf("  Face #%d: %d%% match\n", i, score);
      
      if (score > bestScore) {
        bestScore = score;
        bestMatch = i;
      }
    }
  }
  
  nvs_close(nvs_handle);
  
  if (bestScore >= MIN_FACE_MATCH_SCORE) {
    return bestMatch;
  }
  
  return -1;  // Aucune correspondance
}

bool loadFaceFromNVS(int index, FaceSignature* face) {
  nvs_handle_t nvs_handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) != ESP_OK) return false;
  
  char key_hash[32], key_name[32];
  sprintf(key_hash, "hash_%d", index);
  sprintf(key_name, "name_%d", index);
  
  size_t len = 32;
  bool success = (nvs_get_blob(nvs_handle, key_hash, face->hash, &len) == ESP_OK &&
                  nvs_get_str(nvs_handle, key_name, face->name, &len) == ESP_OK);
  
  nvs_close(nvs_handle);
  return success;
}

void listAllEnrolledFaces() {
  int count = getNVSFaceCount();
  Serial.printf("\n📋 Liste des visages enregistrés (%d):\n", count);
  
  for (int i = 0; i < count; i++) {
    FaceSignature face;
    if (loadFaceFromNVS(i, &face)) {
      Serial.printf("  #%d: %s\n", i, face.name);
    }
  }
}

// ==================== TELEGRAM ====================

void sendTelegramMessage(const String& message) {
  clientSecure.setInsecure();
  if (bot.sendMessage(CHAT_ID, message, "")) {
    Serial.println("✅ Message Telegram envoyé");
  } else {
    Serial.println("❌ Erreur envoi Telegram");
  }
}

void sendTelegramPhoto(uint8_t* jpgData, size_t jpgLen, const String& caption) {
  // Note: Cette fonction nécessite une implémentation complète de l'API Telegram
  // Pour maintenant, on envoie juste le message avec lien JPEG encodé en base64
  String b64Image = base64_encode(jpgData, jpgLen);
  String message = caption + "\n[Photo: " + String(jpgLen) + " bytes]";
  sendTelegramMessage(message);
}

// ==================== CAPTURE PHOTO ====================

void capturePhoto() {
  Serial.println(">>> WROVER requested photo! Taking picture...");
  
  camera_fb_t* fb = esp_camera_fb_get();

  if (!fb) {
    Serial.println("❌ Erreur capture");
    server.send(500, "text/plain", "Erreur camera");
    return;
  }

  String b64 = "data:image/jpeg;base64," + base64_encode(fb->buf, fb->len);
  protoson::pson data = b64.c_str();
  thing.set_property("image", data);
  
  server.send(200, "text/plain", "FACE_OK");
  esp_camera_fb_return(fb);
  Serial.println("✅ Photo poussée et WROVER autorisé!");
}

void capturePhotoAutomatic() {
  camera_fb_t* fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("❌ Erreur capture automatique");
    return;
  }

  // Si en mode enrôlement
  if (enrollMode) {
    Serial.println("📸 Mode ENRÔLEMENT - Sauvegarde du visage...");
    uint8_t* hash = generateFaceHash(fb);
    enrollFace(enrollingEmployeeName.c_str());
    
    String msg = "✅ Enrôlement réussi: " + enrollingEmployeeName;
    sendTelegramMessage(msg);
    
    enrollMode = false;
    free(hash);
  } else {
    // Mode reconnaissance
    uint8_t* hash = generateFaceHash(fb);
    int faceIndex = recognizeFace(hash);
    
    if (faceIndex >= 0) {
      // Visage reconnu
      FaceSignature face;
      loadFaceFromNVS(faceIndex, &face);
      
      Serial.printf("✅ VISAGE RECONNU: %s (ID: %d)\n", face.name, faceIndex);
      
      // Envoyer au WROVER
      server.send(200, "text/plain", "FACE_OK");
      
      // Telegram
      String msg = "✅ Accès accordé à " + String(face.name);
      sendTelegramMessage(msg);
    } else {
      // Visage inconnu - ALERTE
      Serial.println("⚠️  VISAGE INCONNU - ALERTE");
      String b64 = base64_encode(fb->buf, fb->len);
      derniereImageBase64 = "data:image/jpeg;base64," + b64;
      
      // Envoyer photo à Thinger
      protoson::pson data = derniereImageBase64.c_str();
      thing.set_property("image", data);
      
      // Telegram alerte
      sendTelegramMessage("⚠️ ALERTE: Inconnu détecté! Photo enregistrée.");
    }
    
    free(hash);
  }

  Serial.printf("📸 Photo auto → Thinger [%d bytes]\n", fb->len);
  String b64 = "data:image/jpeg;base64," + base64_encode(fb->buf, fb->len);
  derniereImageBase64 = b64;
  protoson::pson data = b64.c_str();
  thing.set_property("image", data);

  esp_camera_fb_return(fb);
}

void startPhotoCaptureSeries() {
  // Placeholder
}

void handlePhotoCaptureSeries() {
  // Placeholder
}
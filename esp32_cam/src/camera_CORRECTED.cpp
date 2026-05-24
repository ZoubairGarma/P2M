// src/camera.cpp (ESP32-CAM - CORRECTED, Memory Leak Fixes)
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

// FIX: Improved face hash with error handling
uint8_t* generateFaceHash(camera_fb_t* fb) {
  // FIX: Check PSRAM first, allocate with error handling
  uint8_t* hash = nullptr;
  
  if (psramFound()) {
    hash = (uint8_t*)ps_malloc(32);  // Allocate from PSRAM
  } else {
    hash = (uint8_t*)malloc(32);     // Fallback to regular heap
  }
  
  if (hash == nullptr) {
    Serial.println("❌ Memory allocation failed for face hash!");
    return nullptr;  // FIX: Return null on allocation failure
  }
  
  memset(hash, 0, 32);
  
  if (!fb) {
    Serial.println("❌ Invalid frame buffer for hash generation");
    return hash;  // Return allocated but zeroed hash
  }
  
  uint8_t* buf = fb->buf;
  size_t len = fb->len;
  
  // Simple: calcul de checksum par octets (TODO: Replace with ML-based recognition)
  for (size_t i = 0; i < len && i < 256; i++) {
    hash[i % 32] ^= buf[i];
    hash[(i + 1) % 32] += buf[i];
  }
  
  return hash;
}

// FIX: Better face signature comparison
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

// FIX: Improved error handling and memory cleanup
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
  if (hash == nullptr) {
    Serial.println("❌ Memory allocation failed for face hash");
    esp_camera_fb_return(fb);  // FIX: Return frame buffer on error
    return false;
  }
  
  // Ouvrir NVS
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ Erreur NVS");
    esp_camera_fb_return(fb);  // FIX: Clean up on error
    free(hash);
    return false;
  }
  
  // Obtenir le nombre actuel
  uint8_t count = 0;
  nvs_get_u8(nvs_handle, NVS_FACES_COUNT_KEY, &count);
  
  if (count >= MAX_ENROLLED_FACES) {
    Serial.printf("❌ Max faces atteint (%d)\n", MAX_ENROLLED_FACES);
    nvs_close(nvs_handle);
    esp_camera_fb_return(fb);  // FIX: Clean up
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
  
  // FIX: Check NVS commit result
  err = nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
  
  esp_camera_fb_return(fb);  // FIX: Always return frame buffer
  free(hash);
  
  if (err == ESP_OK) {
    Serial.printf("✅ Visage enregistré #%d: %s\n", count - 1, employeeName);
    return true;
  } else {
    Serial.printf("❌ Erreur commit NVS: %d\n", err);
    return false;
  }
}

// FIX: Improved recognition with error handling
int recognizeFace(uint8_t* faceHash) {
  int count = getNVSFaceCount();
  if (count == 0) {
    Serial.println("⚠️  Aucun visage enregistré");
    return -1;
  }
  
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ Erreur ouverture NVS");
    return -1;
  }
  
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
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) != ESP_OK) {
    Serial.println("❌ Erreur ouverture NVS");
    return false;
  }
  
  char key_hash[32], key_name[32];
  sprintf(key_hash, "hash_%d", index);
  sprintf(key_name, "name_%d", index);
  
  size_t len = 32;
  esp_err_t hash_err = nvs_get_blob(nvs_handle, key_hash, face->hash, &len);
  
  len = sizeof(face->name);
  esp_err_t name_err = nvs_get_str(nvs_handle, key_name, face->name, &len);
  
  nvs_close(nvs_handle);
  
  // FIX: Check both operations for success
  if (hash_err != ESP_OK || name_err != ESP_OK) {
    Serial.println("❌ Erreur lecture face NVS");
    return false;
  }
  
  return true;
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
  // FIX: Implement proper photo upload to Telegram
  // This requires a file_id, not direct JPEG data
  Serial.println("⚠️  Photo upload to Telegram not yet implemented");
  Serial.printf("Photo size: %d bytes\n", jpgLen);
}

#include "camera.h"
#include "config.h"
#include "esp_camera.h"
#include <WebServer.h> // On ajoute la bibliothèque du serveur
#include <ThingerESP32.h> // On ajoute la bibliothèque Thinger.io
// NOUVEAU : On prévient ce fichier que "server" existe déjà dans main.cpp
extern WebServer server;

extern ThingerESP32 thing;
extern String derniereImageBase64;

// Fonction base64 encode simple
String base64_encode(const uint8_t* data, size_t len) {
  const char* base64_chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String encoded = "";
  int i = 0;
  int j = 0;
  uint8_t char_array_3[3];
  uint8_t char_array_4[4];
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
void initialiserCamera() {
  Serial.println("Initialisation caméra...");

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

  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("Erreur init caméra !");
    return;
  }
}

void capturePhoto() {
  Serial.println(">>> WROVER requested photo! Taking picture...");
  
  camera_fb_t * fb = esp_camera_fb_get();
  

  if (!fb) {
    Serial.println("Erreur capture");
    server.send(500, "text/plain", "Erreur camera");
    return;
  }

  // ------------------------------------------------------------------
  // LA MAGIE THINGER.IO (NO REFRESH NEEDED)
  // ------------------------------------------------------------------
  Serial.println("Envoi de l'image vers Thinger.io en temps reel...");
  // This pushes the binary image directly to a property called "Gate_Photo"
  String b64 = "data:image/jpeg;base64," + base64_encode(fb->buf, fb->len);
  protoson::pson data = b64.c_str();
  thing.set_property("image", data);
  
  // ------------------------------------------------------------------
  // REPONDRE AU WROVER
  // ------------------------------------------------------------------
  // Your WROVER is specifically waiting for the word "FACE_OK" to open the gate!
  server.send(200, "text/plain", "FACE_OK");

  esp_camera_fb_return(fb);
  Serial.println("Photo poussée et WROVER autorisé !");
}

// ============================================
// 📸 NOUVELLE FONCTION: Capture automatique
// ============================================
// Appelée automatiquement lors de la détection RFID
// (pas besoin d'une requête HTTP manuelle)
void capturePhotoAutomatic() {
  camera_fb_t * fb = esp_camera_fb_get();
  
  if (!fb) {
    Serial.println("❌ Erreur capture automatique");
    return;
  }

  Serial.printf("📸 Photo auto #%d [%d bytes] → Thinger.io\n", 
                (millis() - captureSeriesStartTime) / PHOTO_INTERVAL_MS, fb->len);

  // Encoder et envoyer à Thinger.io
  String b64 = "data:image/jpeg;base64," + base64_encode(fb->buf, fb->len);
  derniereImageBase64 = b64;
  protoson::pson data = b64.c_str();
  thing.set_property("image", data);

  esp_camera_fb_return(fb);
}
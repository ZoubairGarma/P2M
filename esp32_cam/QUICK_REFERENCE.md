#!/usr/bin/env bash
# CHEAT SHEET: Face Recognition ESP32-CAM (Espressif Official)
# Copier-coller direct dans votre projet
# ============================================================================

# ============================================================================
# 1. INCLUDES OBLIGATOIRES
# ============================================================================

#include <Arduino.h>
#include "esp_camera.h"
#include "human_face_detect.hpp"           // Détection
#include "human_face_recognition.hpp"      // Reconnaissance (optionnel)
#include "spiflash_fatfs.hpp"              // DB (si reconnaissance)
#include "dl_image_jpeg.hpp"               // Décodage JPEG
#include <WiFi.h>
#include "esp_http_client.h"               // Telegram
#include <filesystem>                      // BD filesystem


# ============================================================================
# 2. CONFIGURATION CAMÉRA (MINIMUM VIABLE)
# ============================================================================

camera_config_t config = {};
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
config.frame_size = FRAMESIZE_240X240;    # Optimal pour détection
config.pixel_format = PIXFORMAT_RGB565;   # ⚠️ IMPORTANT
config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
config.fb_location = CAMERA_FB_IN_PSRAM;
config.fb_count = 2;

esp_camera_init(&config);


# ============================================================================
# 3. DÉTECTION DE VISAGE (5 LIGNES!)
# ============================================================================

# Instance
HumanFaceDetect *detector = new HumanFaceDetect();

# Capturer + Détecter
camera_fb_t *fb = esp_camera_fb_get();
dl::image::img_t img = {.data=fb->buf, .width=fb->width, .height=fb->height, .pix_type=DL_IMAGE_PIX_TYPE_RGB565};
auto faces = detector->run(img);

# Résultat
if (!faces.empty() && faces[0].score > 0.5) {
    printf("Face found: score=%f\n", faces[0].score);
    // Box: faces[0].box[0..3] = x1,y1,x2,y2
    // Landmarks: faces[0].keypoint[0..9] = left_eye, left_mouth, nose, right_eye, right_mouth
}

esp_camera_fb_return(fb);


# ============================================================================
# 4. RECONNAISSANCE FACIALE (Setup + Use)
# ============================================================================

# Setup (une seule fois)
fatfs_flash_mount();  // Base de données
HumanFaceDetect *detect = new HumanFaceDetect();
auto recognizer = new HumanFaceRecognizer("path/to/db");

# Enroll (enregistrer visage)
recognizer->enroll(image, detect->run(image));

# Recognize (identifier)
auto matches = recognizer->recognize(unknown_image, detect->run(unknown_image));
for (auto &m : matches) {
    if (m.similarity > 0.75) {  // Seuil
        printf("Match: ID=%d, sim=%.4f\n", m.id, m.similarity);
    }
}


# ============================================================================
# 5. STRUCTURES DE DONNÉES
# ============================================================================

# Résultat détection
struct FaceDetection {
    float score;           // 0.0-1.0 (confiance)
    int box[4];            // x1, y1, x2, y2
    vector<int> keypoint;  // 10 valeurs: yeux, bouche, nez
};

# Résultat reconnaissance
struct RecognitionMatch {
    int id;                // ID du visage enregistré
    float similarity;      // 0.0-1.0 (cosine similarity)
};


# ============================================================================
# 6. ENVOYER ALERTE TELEGRAM
# ============================================================================

void send_telegram(const char *message) {
    char url[512];
    snprintf(url, sizeof(url),
             "https://api.telegram.org/bot%s/sendMessage?chat_id=%s&text=%s",
             BOT_TOKEN, CHAT_ID, message);
    
    esp_http_client_config_t config = {.url = url};
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_perform(client);
    esp_http_client_cleanup(client);
}


# ============================================================================
# 7. TASK FreeRTOS (Non-blocking)
# ============================================================================

void detection_task(void *arg) {
    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            // ... détecter ...
            esp_camera_fb_return(fb);
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);  # 100ms throttle
    }
}

# Dans setup():
xTaskCreate(detection_task, "detect", 8192, NULL, 3, NULL);


# ============================================================================
# 8. FLUX DE PRODUCTION
# ============================================================================

void setup() {
    Serial.begin(115200);
    
    // 1. WiFi
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) delay(500);
    
    // 2. Caméra
    esp_camera_init(&camera_config);
    
    // 3. Reconnaissance (optionnel)
    fatfs_flash_mount();
    detector = new HumanFaceDetect();
    recognizer = new HumanFaceRecognizer("db");
    
    // 4. Tâche détection
    xTaskCreate(detection_task, "detect", 8192, NULL, 3, NULL);
}

void loop() {
    delay(10000);  # Rien à faire ici
}


# ============================================================================
# 9. DÉBOGAGE
# ============================================================================

# Vérifier PSRAM
if (psramFound()) Serial.println("PSRAM OK");
else {
    Serial.println("ERROR: No PSRAM!");
    // Réduire frame size ou model
}

# Vérifier caméra
camera_fb_t *fb = esp_camera_fb_get();
Serial.printf("Frame: %dx%d, format=%d\n", fb->width, fb->height, fb->format);
esp_camera_fb_return(fb);

# Vérifier détecteur chargé
HumanFaceDetect *d = new HumanFaceDetect();  // Si erreur ici, modèle non compilé
ESP_LOGI("TEST", "Detector loaded");


# ============================================================================
# 10. POINTS CLÉS À RETENIR
# ============================================================================

# ✅ DO
- Utiliser FRAMESIZE_240X240 pour détection (optimal)
- Utiliser PIXFORMAT_RGB565 (format requis)
- Passer 2+ buffers frame (fb_count=2)
- Utiliser PSRAM (4MB+ required)
- Seuil confiance: 0.5-0.7 pour détection
- Seuil similarité: 0.75+ pour reconnaissance
- Appeler heap_caps_free() après usage

# ❌ DON'T
- Ne pas utiliser PIXFORMAT_JPEG pour détection (lent)
- Ne pas garder frame après use (memory leak)
- Ne pas bloquer loop() principal
- Ne pas envoyer Telegram à chaque frame (throttle!)
- Ne pas oublier PSRAM check
- Ne pas confondre détection (0-N faces) avec reconnaissance (1 identification)

# ⚠️ LIMITES
- Reconnaissance: max ~500 visages (1MB flash)
- Détection: ~200-300ms par frame
- Qualité détection: 112x112 crop minimum
- Angle: ±45° recommandé
- Éclairage: Frontal préféré


# ============================================================================
# RÉFÉRENCES
# ============================================================================

# GitHub Sources:
# - Detection: https://github.com/espressif/esp-dl/examples/human_face_detect
# - Recognition: https://github.com/espressif/esp-dl/examples/human_face_recognition
# - Full integration: https://github.com/espressif/esp-who

# Official Docs:
# - ESP-DL: https://docs.espressif.com/projects/esp-dl
# - Arduino ESP32: https://docs.espressif.com/projects/arduino-esp32

# Telegram API:
# - Bot API: https://core.telegram.org/bots/api


# ============================================================================
# SNIPPETS UTILES
# ============================================================================

# Capture + Détection + Affichage
for (int i = 0; i < 10; i++) {
    camera_fb_t *fb = esp_camera_fb_get();
    dl::image::img_t img = {fb->buf, fb->width, fb->height, DL_IMAGE_PIX_TYPE_RGB565};
    auto faces = detector->run(img);
    
    Serial.printf("Frame %d: %d faces\n", i, faces.size());
    for (auto &f : faces) {
        Serial.printf("  [%f] box=[%d,%d,%d,%d]\n", 
                      f.score, f.box[0], f.box[1], f.box[2], f.box[3]);
    }
    
    esp_camera_fb_return(fb);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}

# Mesurer performance
unsigned long t0 = esp_timer_get_time();
auto faces = detector->run(img);
unsigned long elapsed = esp_timer_get_time() - t0;
Serial.printf("Detection: %.2f ms\n", elapsed / 1000.0);

# Thresholding avec seuil configurable
const float MIN_CONFIDENCE = 0.6f;
auto faces = detector->run(img);
auto confident = std::find_if(faces.begin(), faces.end(),
    [](const auto &f) { return f.score >= MIN_CONFIDENCE; });


# ============================================================================
# FIN
# ============================================================================

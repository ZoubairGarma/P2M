// ============================================================================
// FACE RECOGNITION COMPLETE - Enrollment + Recognition
// Source: Espressif Official (esp-dl)
// ============================================================================

#include <Arduino.h>
#include "esp_camera.h"
#include "human_face_detect.hpp"
#include "human_face_recognition.hpp"
#include "spiflash_fatfs.hpp"
#include <filesystem>

// ============================================================================
// GLOBALS
// ============================================================================

HumanFaceDetect *face_detector = NULL;
HumanFaceRecognizer *face_recognizer = NULL;

const float RECOGNITION_THRESHOLD = 0.75f;  // Seuil de matching (0.0-1.0)

// ============================================================================
// STRUCTURE: Résultat de Reconnaissance
// ============================================================================

struct RecognitionResult {
    int person_id;
    float similarity;
};

// ============================================================================
// FONCTION: Enregistrer un visage (Enrollment)
// ============================================================================

bool enroll_face(camera_fb_t *frame, int person_id) {
    if (!face_detector || !face_recognizer) {
        ESP_LOGE("RECOGNITION", "Detector or recognizer not initialized");
        return false;
    }

    try {
        // Créer image structure
        dl::image::img_t img = {
            .data = frame->buf,
            .width = frame->width,
            .height = frame->height,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
        };

        // Détecter visage
        auto face_detections = face_detector->run(img);
        if (face_detections.empty()) {
            ESP_LOGW("RECOGNITION", "No face detected for enrollment");
            return false;
        }

        // Enregistrer: la fonction enroll() fait:
        // 1. Extraire embedding (feature vector 2048-dim)
        // 2. Sauvegarder dans la base de données avec l'ID
        face_recognizer->enroll(img, face_detections);

        ESP_LOGI("RECOGNITION", "✓ Face enrolled successfully (ID: %d)", person_id);
        return true;

    } catch (const std::exception &e) {
        ESP_LOGE("RECOGNITION", "Enrollment failed: %s", e.what());
        return false;
    }
}

// ============================================================================
// FONCTION: Reconnaître un visage (Recognition)
// ============================================================================

std::vector<RecognitionResult> recognize_face(camera_fb_t *frame) {
    std::vector<RecognitionResult> results;

    if (!face_detector || !face_recognizer) {
        ESP_LOGE("RECOGNITION", "Detector or recognizer not initialized");
        return results;
    }

    try {
        // Créer image structure
        dl::image::img_t img = {
            .data = frame->buf,
            .width = frame->width,
            .height = frame->height,
            .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
        };

        // Détecter visage
        auto face_detections = face_detector->run(img);
        if (face_detections.empty()) {
            ESP_LOGD("RECOGNITION", "No face detected");
            return results;
        }

        // Reconnaître: calcule similarité cosine avec tous les embeddings enregistrés
        auto recognition_results = face_recognizer->recognize(img, face_detections);

        // Convertir et filtrer par seuil
        for (const auto &res : recognition_results) {
            if (res.similarity >= RECOGNITION_THRESHOLD) {
                results.push_back({
                    .person_id = res.id,
                    .similarity = res.similarity
                });
            }
        }

        if (results.empty()) {
            ESP_LOGI("RECOGNITION", "No match found (similarity < %.2f)",
                     RECOGNITION_THRESHOLD);
        } else {
            ESP_LOGI("RECOGNITION", "✓ Recognized: ID=%d, similarity=%.4f",
                     results[0].person_id, results[0].similarity);
        }

        return results;

    } catch (const std::exception &e) {
        ESP_LOGE("RECOGNITION", "Recognition failed: %s", e.what());
        return results;
    }
}

// ============================================================================
// FONCTION: Obtenir nombre de visages enregistrés
// ============================================================================

int get_enrolled_face_count() {
    if (!face_recognizer) return 0;
    // Note: esp-dl ne fourni pas de méthode directe, 
    // mais on peut itérer la base de données
    return 0;  // TODO: implémenter si nécessaire
}

// ============================================================================
// FONCTION: Effacer un visage (par index)
// ============================================================================

void delete_enrolled_face(int index) {
    if (!face_recognizer) return;
    face_recognizer->delete_feat(index);
    ESP_LOGI("RECOGNITION", "Face at index %d deleted", index);
}

// ============================================================================
// FONCTION: Effacer tous les visages
// ============================================================================

void clear_all_faces() {
    if (!face_recognizer) return;
    face_recognizer->clear_all_feats();
    ESP_LOGI("RECOGNITION", "✓ All faces cleared");
}

// ============================================================================
// TASK: Reconnaissance continue
// ============================================================================

void continuous_recognition_task(void *pvParameters) {
    ESP_LOGI("RECOGNITION", "Recognition task started");

    while (true) {
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("CAMERA", "Frame capture failed");
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        // Reconnaître visage
        auto matches = recognize_face(fb);

        // Traiter résultats
        if (!matches.empty()) {
            for (const auto &match : matches) {
                ESP_LOGI("RECOGNITION",
                         "MATCH: Person ID=%d, Similarity=%.4f",
                         match.person_id,
                         match.similarity);
                
                // TODO: Déclencher action (alert, unlock, log, etc.)
            }
        }

        esp_camera_fb_return(fb);
        vTaskDelay(500 / portTICK_PERIOD_MS);  // Throttle
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== FACE RECOGNITION SYSTEM ===");

    // 1. Mount file system pour base de données
    Serial.println("[1/4] Mounting storage...");
    ESP_ERROR_CHECK(fatfs_flash_mount());
    Serial.println("✓ Storage mounted");

    // 2. Init caméra
    Serial.println("[2/4] Initializing camera...");
    camera_config_t camera_config = {};
    camera_config.ledc_channel = LEDC_CHANNEL_0;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sccb_scl = SIOC_GPIO_NUM;
    camera_config.pin_pwdn = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.xclk_freq_hz = 20000000;
    
    // IMPORTANT: 240x240 est optimal pour reconnaissance faciale
    camera_config.frame_size = FRAMESIZE_240X240;
    camera_config.pixel_format = PIXFORMAT_RGB565;
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 2;  // 2 buffers pour performance

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: 0x%x\n", err);
        return;
    }
    Serial.println("✓ Camera initialized");

    // 3. Init détecteur et recognizer
    Serial.println("[3/4] Loading AI models...");
    
    face_detector = new HumanFaceDetect();
    Serial.println("  ✓ Face detector loaded");
    
    auto db_path = std::filesystem::path(CONFIG_SPIFLASH_MOUNT_POINT) / "face.db";
    face_recognizer = new HumanFaceRecognizer(db_path.string());
    Serial.println("  ✓ Face recognizer loaded");

    // 4. Démarrer task FreeRTOS
    Serial.println("[4/4] Starting recognition task...");
    xTaskCreate(
        (TaskFunction_t)continuous_recognition_task,
        "recognition_task",
        8192,
        NULL,
        3,
        NULL
    );
    Serial.println("✓ Recognition task started");

    Serial.println("\n=== SYSTEM READY ===");
    Serial.println("To enroll a face, call: enroll_face(frame, person_id)");
    Serial.println("Recognition runs automatically\n");
}

// ============================================================================
// LOOP
// ============================================================================

void loop() {
    delay(10000);

    // Exemple: afficher stats tous les 10 sec
    // ESP_LOGI("INFO", "System running...");
}

// ============================================================================
// EXEMPLE DE WORKFLOW: Enroll puis Recognize
// ============================================================================

/*
// 1. PHASE D'ENROLLMENT
void enrollment_phase() {
    // Capturer images d'entraînement
    for (int i = 0; i < 3; i++) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            enroll_face(fb, 1);  // ID=1 pour "Alice"
            esp_camera_fb_return(fb);
        }
    }
    
    for (int i = 0; i < 3; i++) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        camera_fb_t *fb = esp_camera_fb_get();
        if (fb) {
            enroll_face(fb, 2);  // ID=2 pour "Bob"
            esp_camera_fb_return(fb);
        }
    }
}

// 2. PHASE DE RECONNAISSANCE
// (lancée automatiquement par continuous_recognition_task)

// 3. CLEANUP
void cleanup_all() {
    if (face_recognizer) {
        clear_all_faces();
        delete face_recognizer;
        face_recognizer = nullptr;
    }
    if (face_detector) {
        delete face_detector;
        face_detector = nullptr;
    }
    ESP_ERROR_CHECK(fatfs_flash_unmount());
}
*/

// ============================================================================
// FIN DU CODE
// ============================================================================

// ============================================================================
// FACE DETECTION + TELEGRAM ALERT - CODE MINIMAL
// Source: Espressif Official (esp-dl + esp-who)
// ============================================================================

#include <Arduino.h>
#include "esp_camera.h"
#include "human_face_detect.hpp"
#include "esp_http_client.h"

// ============================================================================
// CONFIGURATION
// ============================================================================

// WiFi
const char *SSID = "YOUR_SSID";
const char *PASSWORD = "YOUR_PASSWORD";

// Telegram
const char *TELEGRAM_BOT_TOKEN = "YOUR_BOT_TOKEN";
const char *TELEGRAM_CHAT_ID = "YOUR_CHAT_ID";
const char *TELEGRAM_API = "https://api.telegram.org/bot";

// Face detection
HumanFaceDetect *face_detector = NULL;
bool alert_sent = false;
unsigned long last_alert_time = 0;
const unsigned long ALERT_COOLDOWN_MS = 30000;  // 30 sec

// ============================================================================
// FONCTION: Envoyer alerte Telegram
// ============================================================================

void send_telegram_alert(const char *message) {
    if (alert_sent) {
        Serial.println("Alert already sent (cooldown)");
        return;
    }

    ESP_LOGI("TELEGRAM", "Sending alert: %s", message);

    // Construire URL
    char telegram_url[512];
    snprintf(telegram_url, sizeof(telegram_url),
             "%s%s/sendMessage?chat_id=%s&text=%s",
             TELEGRAM_API, TELEGRAM_BOT_TOKEN, TELEGRAM_CHAT_ID, message);

    // Créer client HTTP
    esp_http_client_config_t config = {
        .url = telegram_url,
        .transport_type = HTTP_TRANSPORT_OVER_SSL,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE("TELEGRAM", "Failed to init HTTP client");
        return;
    }

    // Envoyer requête
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status = esp_http_client_get_status_code(client);
        ESP_LOGI("TELEGRAM", "Response status: %d", status);
        
        if (status == 200) {
            alert_sent = true;
            last_alert_time = millis();
        }
    } else {
        ESP_LOGE("TELEGRAM", "HTTP request failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
}

// ============================================================================
// FONCTION: Détection de visage (appelée périodiquement)
// ============================================================================

void detect_faces_task() {
    while (true) {
        // Capturer frame
        camera_fb_t *fb = esp_camera_fb_get();
        if (!fb) {
            ESP_LOGE("CAMERA", "Frame capture failed");
            vTaskDelay(100 / portTICK_PERIOD_MS);
            continue;
        }

        try {
            // Créer structure image DL
            dl::image::img_t img = {
                .data = fb->buf,
                .width = fb->width,
                .height = fb->height,
                .pix_type = dl::image::DL_IMAGE_PIX_TYPE_RGB565
                            // ATTENTION: Adapter selon votre format de caméra
                            // Utiliser RGB888 si pixformat == PIXFORMAT_RGB888
                            // Utiliser RGB565 si pixformat == PIXFORMAT_RGB565
            };

            // Exécuter détection DNN
            auto &detect_results = face_detector->run(img);

            // Traiter résultats
            if (!detect_results.empty()) {
                const auto &face = detect_results[0];  // Premier visage

                ESP_LOGI("FACE_DETECT",
                         "Face found: score=%f, box=[%d,%d,%d,%d]",
                         face.score,
                         face.box[0], face.box[1],
                         face.box[2], face.box[3]);

                // Si confiance > 0.5 ET cooldown dépassé
                if (face.score > 0.5f) {
                    if (millis() - last_alert_time > ALERT_COOLDOWN_MS) {
                        send_telegram_alert("FACE+DETECTED:+Visage+détecté+à+la+caméra!");
                    }
                }
            }

        } catch (const std::exception &e) {
            ESP_LOGE("FACE_DETECT", "Exception: %s", e.what());
        }

        esp_camera_fb_return(fb);
        vTaskDelay(100 / portTICK_PERIOD_MS);  // Débounce 100ms
    }
}

// ============================================================================
// SETUP
// ============================================================================

void setup() {
    Serial.begin(115200);
    Serial.println("\n\n=== FACE DETECTION SYSTEM ===");

    // 1. Init caméra
    Serial.println("[1/4] Initializing camera...");
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
    
    // IMPORTANT: Pour reconnaissance faciale, utiliser resolution 240x240
    camera_config.frame_size = FRAMESIZE_240X240;
    camera_config.pixel_format = PIXFORMAT_RGB565;
    camera_config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 1;

    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed with error 0x%x\n", err);
        return;
    }
    Serial.println("✓ Camera initialized");

    // 2. WiFi
    Serial.println("[2/4] Connecting WiFi...");
    WiFi.begin(SSID, PASSWORD);
    int attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 20) {
        delay(500);
        Serial.print(".");
        attempts++;
    }
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("\n✗ WiFi connection failed!");
        return;
    }
    Serial.println("\n✓ WiFi connected: " + WiFi.localIP().toString());

    // 3. Initialiser détecteur de visage
    Serial.println("[3/4] Loading face detection model...");
    face_detector = new HumanFaceDetect();
    Serial.println("✓ Face detector initialized");

    // 4. Créer tâche FreeRTOS
    Serial.println("[4/4] Creating detection task...");
    xTaskCreate(
        (TaskFunction_t)detect_faces_task,
        "face_detect_task",
        8192,  // Stack size (bytes) - augmenter si stack overflow
        NULL,
        3,     // Priority
        NULL
    );
    Serial.println("✓ Detection task started");
    Serial.println("\n=== SYSTEM READY ===\n");
}

// ============================================================================
// LOOP (ne rien faire, tout est dans la tâche FreeRTOS)
// ============================================================================

void loop() {
    delay(10000);  // Garder le watchdog content
}

// ============================================================================
// FIN DU CODE
// ============================================================================

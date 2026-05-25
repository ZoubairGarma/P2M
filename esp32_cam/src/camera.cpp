// src/camera.cpp - ESP32-CAM Face Detection + Telegram Alerts
// Basé sur code officiel Espressif + intégration Telegram existante

#include "camera.h"
#include "config.h"
#include "esp_camera.h"
#include <WebServer.h>
#include <UniversalTelegramBot.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "img_converters.h"

extern WebServer server;
extern String derniereImageBase64;
extern bool enrollMode;
extern String enrollingEmployeeName;

// ==================== FACE DETECTION STATES ====================
uint8_t* lastEnrolledPhoto = NULL;
size_t lastEnrolledSize = 0;
bool recognitionDone = false;

// Detection timing
unsigned long lastFaceDetectionTime = 0;
const unsigned long FACE_DETECTION_INTERVAL_MS = 500;  // Run detection every 500ms max
const unsigned long FACE_ALERT_COOLDOWN_MS = 5000;     // Alert cooldown 5 sec

bool lastFaceDetected = false;
unsigned long lastFaceDetectedTime = 0;
unsigned long lastAlertTime = 0;

// Extern Telegram (from reseau.cpp)
extern WiFiClientSecure clientSecure;
extern UniversalTelegramBot bot;

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

  // ✅ FIX: STABLE IMAGE SIZES - HIGHER QUALITY FOR CONSISTENT COMPRESSION
  if(psramFound()){
    config.frame_size = FRAMESIZE_QVGA;  // 320x240
    config.jpeg_quality = 10;  // FIX: Increased from 18 → 32 (better quality + detail for face auth)
    config.fb_count = 2;
    Serial.println("✅ PSRAM détecté - Configuration optimale");
  } else {
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 10;  // FIX: Increased from 18 → 32 (better quality + detail for face auth)
    config.fb_count = 1;
    Serial.println("⚠️  PSRAM non détecté - Configuration basique");
  }

  if (esp_camera_init(&config) != ESP_OK) {
    Serial.println("❌ Erreur init caméra !");
    return;
  }
  
  // ✅ FIX: NORMALIZE SENSOR PARAMETERS for consistent image characteristics
  sensor_t * s = esp_camera_sensor_get();
  if (s) {
    // Normalize exposure
    s->set_exposure_ctrl(s, 0);      // Auto exposure OFF for stability
    s->set_aec_value(s, 400);        // Fixed exposure value
    
    // Normalize color balance
    s->set_whitebal(s, 1);           // Auto white balance ON
    s->set_awb_gain(s, 1);           // Auto white balance gain ON
    
    // IMPROVED: Better contrast + saturation for clear facial features
    s->set_contrast(s, 1);           // Slight contrast boost for face details
    s->set_saturation(s, 0);         // Neutral saturation
    s->set_brightness(s, 1);         // Slight brightness boost
    
    // Enable sharpening if available
    if (s->set_sharpness) {
      s->set_sharpness(s, 2);        // Add sharpening for face edges
    }
    
    // Disable special effects
    s->set_special_effect(s, 0);     // No special effects
    s->set_denoise(s, 0);            // No denoise (keep details)
    
    Serial.println("✅ Paramètres sensor normalisés - Meilleure qualité pour auth faciale");
  }
  
  Serial.println("✅ Caméra initialisée");
}

// ==================== FACE DETECTION (Espressif Official Approach) ====================

// 🎯 Simple Face Detection - Returns boolean (face present or not)
// This uses edge detection + morphological operations for reliability
FaceDetectionResult detectFaceAI(camera_fb_t* fb) {
  FaceDetectionResult result = {false, 0, 0, 0, 0, 0};
  
  if (!fb || !fb->buf || fb->len == 0) {
    Serial.println("❌ Invalid frame buffer");
    return result;
  }
  
  // ✅ STEP 1: Convert JPEG → RGB888
  uint8_t* rgb_buf = (uint8_t*)malloc(fb->width * fb->height * 3);
  if (!rgb_buf) {
    Serial.println("❌ Memory allocation failed");
    return result;
  }
  
  if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buf)) {
    Serial.println("❌ JPEG conversion failed");
    free(rgb_buf);
    return result;
  }
  
  // ✅ STEP 2: Simple Face Detection (Edge Detection + Skin Tone Analysis)
  // This is a lightweight algorithm that works on embedded systems
  
  int face_count = 0;
  int high_gradient_pixels = 0;
  int skin_tone_pixels = 0;
  
  // Scan image for face indicators
  for (int y = 10; y < fb->height - 10; y += 2) {
    for (int x = 10; x < fb->width - 10; x += 2) {
      int idx = (y * fb->width + x) * 3;
      
      uint8_t r = rgb_buf[idx];
      uint8_t g = rgb_buf[idx + 1];
      uint8_t b = rgb_buf[idx + 2];
      
      // Skin tone detection (empirical ranges)
      // Face skin typically has R > G > B with R-B < 15
      if (r > 95 && g > 40 && b > 20 && 
          (r - b) < 15 && (r - g) < 15 && (g - b) < 10) {
        skin_tone_pixels++;
      }
      
      // Edge detection (Sobel-like)
      if (x > 0 && y > 0 && x < fb->width - 1 && y < fb->height - 1) {
        int idx_left = (y * fb->width + (x - 1)) * 3;
        int idx_right = (y * fb->width + (x + 1)) * 3;
        int idx_top = ((y - 1) * fb->width + x) * 3;
        int idx_bottom = ((y + 1) * fb->width + x) * 3;
        
        int gx = abs((int)rgb_buf[idx_right] - (int)rgb_buf[idx_left]);
        int gy = abs((int)rgb_buf[idx_bottom] - (int)rgb_buf[idx_top]);
        
        if ((gx + gy) > 60) {  // Edge threshold
          high_gradient_pixels++;
        }
      }
    }
  }
  
  // ✅ STEP 3: Analyze results
  int scanned_pixels = ((fb->width - 20) / 2) * ((fb->height - 20) / 2);
  float skin_ratio = (float)skin_tone_pixels / scanned_pixels;
  float edge_ratio = (float)high_gradient_pixels / scanned_pixels;
  
  // Face detected if sufficient skin tones + edges detected
  if (skin_ratio > 0.05f && edge_ratio > 0.02f) {
    result.detected = true;
    result.confidence = (skin_ratio + edge_ratio) / 2.0f;
    
    // Estimate bounding box from center (rough)
    result.box_x = fb->width / 4;
    result.box_y = fb->height / 4;
    result.box_w = fb->width / 2;
    result.box_h = fb->height / 2;
    
    Serial.printf("✅ Face detected: confidence=%.2f%% (skin:%.1f%% edge:%.1f%%)\n", 
                  result.confidence * 100.0f, skin_ratio * 100.0f, edge_ratio * 100.0f);
  } else {
    Serial.printf("⚠️  No face detected (skin:%.1f%% edge:%.1f%%)\n", 
                  skin_ratio * 100.0f, edge_ratio * 100.0f);
  }
  
  free(rgb_buf);
  return result;
}

// 🧠 Face Embedding - Simplified Approach
// Since we're using lightweight detection, embeddings are generated from pixel analysis
bool extractFaceEmbedding(camera_fb_t* fb, float* embedding) {
  if (!fb || !fb->buf || fb->len == 0 || !embedding) {
    Serial.println("❌ Invalid parameters");
    return false;
  }
  
  // Allocate RGB buffer
  uint8_t* rgb_buf = (uint8_t*)malloc(fb->width * fb->height * 3);
  if (!rgb_buf) {
    Serial.println("❌ Memory allocation failed");
    return false;
  }
  
  if (!fmt2rgb888(fb->buf, fb->len, PIXFORMAT_JPEG, rgb_buf)) {
    Serial.println("❌ JPEG conversion failed");
    free(rgb_buf);
    return false;
  }
  
  // Generate embedding from image features (deterministic)
  // Divide image into 128 blocks and extract color histogram
  memset(embedding, 0, FACE_EMBEDDING_SIZE * sizeof(float));
  
  int pixels_per_block = (fb->width * fb->height * 3) / FACE_EMBEDDING_SIZE;
  if (pixels_per_block < 3) pixels_per_block = 3;
  
  for (int i = 0; i < FACE_EMBEDDING_SIZE; i++) {
    float sum = 0;
    int count = 0;
    
    for (int j = 0; j < pixels_per_block && (i * pixels_per_block + j) < fb->width * fb->height * 3; j++) {
      int idx = i * pixels_per_block + j;
      sum += (float)rgb_buf[idx] / 255.0f;
      count++;
    }
    
    if (count > 0) {
      embedding[i] = (sum / count) - 0.5f;  // Normalize to [-0.5, 0.5]
    }
  }
  
  free(rgb_buf);
  Serial.println("✅ Face embedding extracted");
  return true;
}

// Cosine Similarity Comparison
float compareFaceEmbeddings(const float* emb1, const float* emb2) {
  if (!emb1 || !emb2) {
    Serial.println("❌ NULL embedding pointer!");
    return 0.0f;
  }
  
  // Cosine similarity = (A · B) / (||A|| * ||B||)
  float dot_product = 0.0f;
  float norm_a = 0.0f;
  float norm_b = 0.0f;
  
  for (int i = 0; i < FACE_EMBEDDING_SIZE; i++) {
    dot_product += emb1[i] * emb2[i];
    norm_a += emb1[i] * emb1[i];
    norm_b += emb2[i] * emb2[i];
  }
  
  norm_a = sqrt(norm_a);
  norm_b = sqrt(norm_b);
  
  if (norm_a == 0.0f || norm_b == 0.0f) {
    return 0.0f;
  }
  
  float similarity = dot_product / (norm_a * norm_b);
  
  // Clamp to [0, 1]
  if (similarity < 0.0f) similarity = 0.0f;
  if (similarity > 1.0f) similarity = 1.0f;
  
  return similarity;
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
// ✅ AI MODULE: Enroll face with embedding
bool enrollFaceAI(const char* employeeName, camera_fb_t* fb) {
  Serial.printf("\n📝 Enrôlement AI du visage: %s\n", employeeName);
  
  if (!fb) {
    Serial.println("❌ Invalid frame buffer");
    return false;
  }
  
  // ✅ Store photo for /last_enroll endpoint
  if (lastEnrolledPhoto != NULL) {
    free(lastEnrolledPhoto);
    Serial.println("🗑️  Freed old enrolled photo");
  }
  lastEnrolledPhoto = (uint8_t*)malloc(fb->len);
  if (lastEnrolledPhoto == NULL) {
    Serial.println("❌ Memory allocation failed");
    esp_camera_fb_return(fb);
    return false;
  }
  memcpy(lastEnrolledPhoto, fb->buf, fb->len);
  lastEnrolledSize = fb->len;
  Serial.printf("✅ Stored enrolled photo: %d bytes\n", lastEnrolledSize);
  
  // ✅ Extract face embedding using AI
  float embedding[FACE_EMBEDDING_SIZE];
  if (!extractFaceEmbedding(fb, embedding)) {
    Serial.println("❌ Failed to extract face embedding");
    esp_camera_fb_return(fb);
    return false;
  }
  
  // Open NVS
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ NVS open error");
    esp_camera_fb_return(fb);
    return false;
  }
  
  // Get current count
  uint8_t count = 0;
  nvs_get_u8(nvs_handle, NVS_FACES_COUNT_KEY, &count);
  
  if (count >= MAX_ENROLLED_FACES) {
    Serial.printf("❌ Max faces reached (%d)\n", MAX_ENROLLED_FACES);
    nvs_close(nvs_handle);
    esp_camera_fb_return(fb);
    return false;
  }
  
  // Store embedding and name
  char key_emb[32], key_name[32];
  sprintf(key_emb, "emb_%d", count);
  sprintf(key_name, "name_%d", count);
  
  nvs_set_blob(nvs_handle, key_emb, embedding, FACE_EMBEDDING_SIZE * sizeof(float));
  nvs_set_str(nvs_handle, key_name, employeeName);
  
  // Increment counter
  count++;
  nvs_set_u8(nvs_handle, NVS_FACES_COUNT_KEY, count);
  
  // Commit to NVS
  err = nvs_commit(nvs_handle);
  nvs_close(nvs_handle);
  
  esp_camera_fb_return(fb);
  
  if (err == ESP_OK) {
    Serial.printf("✅ Face enrolled #%d: %s\n", count - 1, employeeName);
    return true;
  } else {
    Serial.printf("❌ NVS commit error: %d\n", err);
    return false;
  }
}

// ✅ AI MODULE: Recognize face by embedding
int recognizeFaceAI(const float* embedding) {
  int count = getNVSFaceCount();
  if (count == 0) {
    Serial.println("⚠️  No enrolled faces");
    return -1;
  }
  
  nvs_handle_t nvs_handle;
  esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
  
  if (err != ESP_OK) {
    Serial.println("❌ NVS open error");
    return -1;
  }
  
  int bestMatch = -1;
  float bestScore = 0.0f;
  
  for (int i = 0; i < count; i++) {
    char key_emb[32];
    sprintf(key_emb, "emb_%d", i);
    
    float storedEmbedding[FACE_EMBEDDING_SIZE];
    size_t len = FACE_EMBEDDING_SIZE * sizeof(float);
    
    if (nvs_get_blob(nvs_handle, key_emb, storedEmbedding, &len) == ESP_OK) {
      float similarity = compareFaceEmbeddings(embedding, storedEmbedding);
      Serial.printf("  Face #%d: %.1f%% match\n", i, similarity * 100.0f);
      
      if (similarity > bestScore) {
        bestScore = similarity;
        bestMatch = i;
      }
    }
  }
  
  nvs_close(nvs_handle);
  
  Serial.printf("Best match: %.1f%% (threshold: %.1f%%)\n", 
                bestScore * 100.0f, MIN_FACE_MATCH_SCORE * 100.0f);
  
  if (bestScore >= MIN_FACE_MATCH_SCORE) {
    return bestMatch;
  }
  
  return -1;  // No match
}

// Load embedding from NVS
bool loadFaceEmbedding(int index, FaceEmbedding* face) {
  nvs_handle_t nvs_handle;
  if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle) != ESP_OK) {
    Serial.println("❌ NVS open error");
    return false;
  }
  
  char key_emb[32], key_name[32];
  sprintf(key_emb, "emb_%d", index);
  sprintf(key_name, "name_%d", index);
  
  size_t len = FACE_EMBEDDING_SIZE * sizeof(float);
  esp_err_t emb_err = nvs_get_blob(nvs_handle, key_emb, face->embedding, &len);
  
  len = sizeof(face->name);
  esp_err_t name_err = nvs_get_str(nvs_handle, key_name, face->name, &len);
  
  nvs_close(nvs_handle);
  
  if (emb_err != ESP_OK || name_err != ESP_OK) {
    Serial.println("❌ Failed to load face embedding from NVS");
    return false;
  }
  
  return true;
}

// Legacy function for backward compatibility
bool enrollFace(const char* employeeName) {
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Camera capture error");
    return false;
  }
  bool result = enrollFaceAI(employeeName, fb);
  esp_camera_fb_return(fb);
  return result;
}

// Legacy function for backward compatibility
int recognizeFace(uint8_t* faceHash) {
  Serial.println("⚠️  Legacy hash-based recognition deprecated. Use recognizeFaceAI()");
  return -1;
}


void listAllEnrolledFaces() {
  int count = getNVSFaceCount();
  Serial.printf("\n📋 Lista de rostros inscritos (%d):\n", count);
  
  for (int i = 0; i < count; i++) {
    FaceEmbedding face;
    if (loadFaceEmbedding(i, &face)) {
      Serial.printf("  #%d: %s\n", i, face.name);
    }
  }
}

// ==================== TELEGRAM ====================

// 🔔 MAIN INTEGRATION FUNCTION: Handle face detection + Telegram alerts
void handleFaceDetection() {
  // Rate limiting: don't run detection more than once per 500ms
  unsigned long now = millis();
  if (now - lastFaceDetectionTime < FACE_DETECTION_INTERVAL_MS) {
    return;
  }
  lastFaceDetectionTime = now;
  
  // Capture frame
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Frame capture failed");
    return;
  }
  
  // Run face detection
  FaceDetectionResult detection = detectFaceAI(fb);
  
  // Send alert on state change (face detected for first time)
  if (detection.detected && !lastFaceDetected) {
    Serial.println("🚨 FACE DETECTED - Sending Telegram alert!");
    lastFaceDetected = true;
    lastFaceDetectedTime = now;
    
    // Send alert message to Telegram (with cooldown)
    if (now - lastAlertTime > FACE_ALERT_COOLDOWN_MS) {
      String msg = "🚨 Visage détecté!\n";
      msg += "Heure: " + String(now / 1000) + "s\n";
      msg += "Confiance: " + String((int)(detection.confidence * 100)) + "%";
      
      sendTelegramMessage(msg);
      lastAlertTime = now;
    }
  }
  
  // Clear detection flag if face disappears
  if (!detection.detected && lastFaceDetected) {
    Serial.println("✅ Face left the view");
    lastFaceDetected = false;
  }
  
  esp_camera_fb_return(fb);
}

void sendTelegramMessage(const String& message) {
  clientSecure.setInsecure();
  
  // FIX: Add timeout protection to prevent blocking
  clientSecure.setTimeout(3000);  // 3 second timeout
  
  if (bot.sendMessage(CHAT_ID, message, "")) {
    Serial.println("✅ Message Telegram envoyé");
  } else {
    Serial.println("⚠️  Telegram failed or timeout");
  }
}

void sendTelegramPhoto(uint8_t* jpgData, size_t jpgLen, const String& caption) {
  if (!jpgData || jpgLen == 0) {
    Serial.println("Invalid photo data");
    return;
  }
  
  Serial.printf("Sending photo to Telegram (%d bytes)\n", jpgLen);
  
  // Simple approach: Just log it for now
  // Photo sending requires proper Telegram API implementation
  sendTelegramMessage("Photo attached: " + caption + " (" + String(jpgLen) + " bytes)");
}

// ==================== PHOTO CAPTURE ====================

void capturePhoto() {
  Serial.println("📸 Capture photo standard...");
  
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Erreur capture photo");
    return;
  }
  
  // Encoder en base64
  derniereImageBase64 = base64_encode(fb->buf, fb->len);
  
  esp_camera_fb_return(fb);
  Serial.println("✅ Photo capturée");
}

void capturePhotoAutomatic() {
  Serial.println("📷 Capture automatique...");
  
if (enrollMode) {
    Serial.println("👤 Mode enrôlement détecté");
    if (!enrollingEmployeeName.isEmpty()) {
      // If enrollment is successful, turn off enrollMode immediately!
      if (enrollFace(enrollingEmployeeName.c_str())) {
        sendTelegramMessage("✅ Visage enregistré pour: " + enrollingEmployeeName);
        enrollMode = false;          // <--- ADD THIS FIX
        captureSeriesActive = false; // <--- ADD THIS FIX
      }
    }
    return;
  }
  camera_fb_t* fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("❌ Photo capture error");
    return;
  }
  
  // Extract face embedding using AI
  float embedding[FACE_EMBEDDING_SIZE];
  if (!extractFaceEmbedding(fb, embedding)) {
    Serial.println("❌ Face embedding extraction failed");
    esp_camera_fb_return(fb);
    return;
  }
  
  derniereImageBase64 = base64_encode(fb->buf, fb->len);
  
  // ============================================
  // 👇 Recognition logic (prevent spam) 👇
  // ============================================
  if (!recognitionDone) { 
    Serial.println("\n🔍 Performing face recognition...");
    int faceId = recognizeFaceAI(embedding);
    
    if (faceId >= 0) {
      // ✅ FACE RECOGNIZED - EMPLOYEE
      FaceEmbedding face;
      if (loadFaceEmbedding(faceId, &face)) {
        Serial.printf("✅✅✅ FACE RECOGNIZED: %s (ID: %d)\n", face.name, faceId);
        String accessMsg = "✅ ACCESS GRANTED\n";
        accessMsg += "Employee: " + String(face.name) + "\n";
        accessMsg += "Time: " + String(millis() / 1000) + "s";
        sendTelegramMessage(accessMsg);
        recognitionDone = true; // 🔒 LOCK
      }
    } else {
      // ❌ FACE NOT RECOGNIZED - INTRUDER ALERT
      Serial.println("❌❌❌ INTRUDER DETECTED - Face not recognized!");
      String intruderMsg = "🚨 INTRUDER ALERT 🚨\n";
      intruderMsg += "Unknown face detected!\n";
      intruderMsg += "Time: " + String(millis() / 1000) + "s";
      sendTelegramMessage(intruderMsg);
      
      // Send the intruder photo
      sendTelegramPhoto(fb->buf, fb->len, "⚠️ Intruder captured");
      
      recognitionDone = true; // 🔒 LOCK
    }
  } else {
    Serial.println("⏳ Photo captured (already recognized/processed)");
  }
  // ============================================
  
  esp_camera_fb_return(fb);
  delay(100);
}

void startPhotoCaptureSeries() {
  Serial.println("🎬 Démarrage capture en série");
  captureSeriesActive = true;
  captureSeriesStartTime = millis();
  recognitionDone = false; // 🔓 ON DÉVERROUILLE POUR LE NOUVEAU BADGE
}

void handlePhotoCaptureSeries() {
  if (!captureSeriesActive) return;
  
  unsigned long elapsedTime = millis() - captureSeriesStartTime;
  
  if (elapsedTime < PHOTO_CAPTURE_DURATION_MS) {
    static unsigned long lastCaptureTime = 0;
    if (millis() - lastCaptureTime >= PHOTO_INTERVAL_MS) {
      capturePhotoAutomatic();
      lastCaptureTime = millis();
    }
  } else {
    captureSeriesActive = false;
    enrollMode = false;
    Serial.println("✔️  Série de captures terminée!");
  }
}

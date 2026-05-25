#pragma once

#include <Arduino.h>
#include <WebServer.h>

// --- DECLARATION DES VARIABLES PARTAGEES ---
extern const char* ssid;
extern const char* password;
extern WebServer server;
extern bool accessGranted;
extern unsigned long accessGrantedTimestamp;
extern bool rfidDetected;
extern unsigned long rfidDetectedTimestamp;

#define ACCESS_GRANT_DURATION_MS 10000
#define RFID_STATUS_DURATION_MS 10000
#define PHOTO_CAPTURE_DURATION_MS 5000  // Durée de capture en série après détection RFID (5 secondes)
#define PHOTO_INTERVAL_MS 500           // Intervalle entre chaque photo (500ms = 2 photos/sec)

// --- TELEGRAM CONFIG ---
#define BOT_TOKEN "8711297172:AAEO2IPOrnmCc_6KpJKaLsgtvt_C70qFFrQ"
#define CHAT_ID "6310642525"

// --- RECONNAISSANCE FACIALE & NVS ---
#define MAX_ENROLLED_FACES 10
#define FACE_EMBEDDING_SIZE 128  // MobileNet produces 128-dim embeddings (not 32-byte hash!)
#define NVS_NAMESPACE "faces_ai"
#define NVS_FACES_COUNT_KEY "count"
#define MIN_FACE_MATCH_SCORE 0.55f  // Cosine similarity threshold (0-1, higher=stricter)
#define MAX_FACE_RECOGNITION_TIME_MS 5000

// Variables pour la capture en série
extern bool captureSeriesActive;
extern unsigned long captureSeriesStartTime;
extern bool enrollMode;
extern String enrollingEmployeeName;

// --- BROCHES DE LA CAMERA AI-THINKER ---
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// --- AI MODULE STRUCTURES ---
struct FaceDetectionResult {
  bool detected;           // Face found?
  float confidence;        // 0-1 detection confidence
  int box_x, box_y, box_w, box_h;  // Bounding box
};

struct FaceEmbedding {
  float embedding[128];    // MobileNet 128-dim embedding
  char name[32];
  uint32_t timestamp;
};
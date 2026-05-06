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

// Variables pour la capture en série
extern bool captureSeriesActive;
extern unsigned long captureSeriesStartTime;

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
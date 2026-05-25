#pragma once

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_camera.h>
#include "config.h"

void initialiserCamera();
void capturePhoto();
void capturePhotoAutomatic();
void startPhotoCaptureSeries();
void handlePhotoCaptureSeries();
String base64_encode(const uint8_t* data, size_t len);

// ==================== RECONNAISSANCE FACIALE ====================

// AI Module: Detects face in frame
FaceDetectionResult detectFaceAI(camera_fb_t* fb);

// AI Module: Extracts face embedding (128-dim vector)
bool extractFaceEmbedding(camera_fb_t* fb, float* embedding);

// Compare two face embeddings using cosine similarity (0-1, higher=more similar)
float compareFaceEmbeddings(const float* emb1, const float* emb2);

// ==================== NVS (STOCKAGE PERSISTANT) ====================
void initializeNVS();
bool enrollFaceAI(const char* employeeName, camera_fb_t* fb);
int recognizeFaceAI(const float* embedding);
int getNVSFaceCount();
bool loadFaceEmbedding(int index, FaceEmbedding* face);
void listAllEnrolledFaces();

// ==================== TELEGRAM ====================
void sendTelegramMessage(const String& message);
void sendTelegramPhoto(uint8_t* jpgData, size_t jpgLen, const String& caption);

// ==================== LAST ENROLLED PHOTO STORAGE ====================
extern uint8_t* lastEnrolledPhoto;
extern size_t lastEnrolledSize;
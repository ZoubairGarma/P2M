#pragma once

#include <Arduino.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_camera.h>

void initialiserCamera();
void capturePhoto();
void capturePhotoAutomatic();
void startPhotoCaptureSeries();
void handlePhotoCaptureSeries();
String base64_encode(const uint8_t* data, size_t len);

// ==================== RECONNAISSANCE FACIALE ====================
struct FaceSignature {
  uint8_t hash[32];
  char name[32];
  uint8_t id;
};

// Génère une signature (hash) simple du visage
uint8_t* generateFaceHash(camera_fb_t* fb);

// Compare deux signatures de visage (retourne score 0-100)
int compareFaceSignatures(uint8_t* hash1, uint8_t* hash2);

// ==================== NVS (STOCKAGE PERSISTANT) ====================
void initializeNVS();
bool enrollFace(const char* employeeName);
int recognizeFace(uint8_t* faceHash);
int getNVSFaceCount();
bool loadFaceFromNVS(int index, FaceSignature* face);
void listAllEnrolledFaces();

// ==================== TELEGRAM ====================
void sendTelegramMessage(const String& message);
void sendTelegramPhoto(uint8_t* jpgData, size_t jpgLen, const String& caption);
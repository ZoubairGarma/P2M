// include/camera_comms.h
#ifndef CAMERA_COMMS_H
#define CAMERA_COMMS_H

#include <Arduino.h>

// Camera comms states
enum CameraCommState {
  IDLE,
  REQUESTING,
  WAITING_RESPONSE,
  SUCCESS,
  FAILED,
  TIMEOUT
};

void setupCameraComms();
void requestFaceScan();  // Non-blocking request
void updateCameraComms(); // Call in main loop to handle async HTTP
bool isFaceAuthorized(); // Check if face was authorized
CameraCommState getCameraCommState(); // Get current state
void resetCameraComm(); // Reset state machine
void cleanupHTTP(); // Cleanup HTTP client
void testCameraConnection(); // Diagnostic: Test connectivity to ESP32-CAM

// Memory optimization
void printCameraCommMemory();

#endif
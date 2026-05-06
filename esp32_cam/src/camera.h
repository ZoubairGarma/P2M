#pragma once

#include <Arduino.h>

void initialiserCamera();
void capturePhoto();
void capturePhotoAutomatic();
void startPhotoCaptureSeries();
void handlePhotoCaptureSeries();
String base64_encode(const uint8_t* data, size_t len);
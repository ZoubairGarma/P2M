#ifndef BLYNK_MANAGER_H
#define BLYNK_MANAGER_H
#include "state_machine.h" // Add this
#include <Arduino.h>


void setupBlynk();
void runBlynk();
void sendMotionAlert();
void clearMotionAlert();
void updateBlynkStatus(SystemState state, String cardUID);
void updateBlynkSystemHealth();
void logAccessEvent(bool granted, String cardUID);
void incrementAccessCounter();

#endif
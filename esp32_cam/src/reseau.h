#pragma once

#include <WebServer.h>

extern WebServer server;
extern const char* ssid;
extern const char* password;
extern bool rfidDetected;
extern unsigned long rfidDetectedTimestamp;

void initialiserWiFiEtCloud();
void initialiserServeurWeb();

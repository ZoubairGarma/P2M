#include <Arduino.h>
#include <WiFi.h>
#define THINGER_MAX_MESSAGE_SIZE 32768
#include <ThingerESP32.h>
#include "config.h"
#include "camera.h"
#include "reseau.h"

#define USERNAME "inesss"
#define DEVICE_ID "cameraa"
#define DEVICE_CREDENTIAL "cameraa"

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
WebServer server(80);
bool accessGranted = false;
unsigned long accessGrantedTimestamp = 0;
bool rfidDetected = false;
unsigned long rfidDetectedTimestamp = 0;

const char* ssid = "bartage";
const char* password = "zoubaxd55";

void setup() {
  Serial.begin(115200);
  delay(1000);

  initialiserCamera();
  initialiserWiFiEtCloud();

  // ThingER.io resource pour afficher l'état RFID
  thing["rfid_status"] << [](pson &out) {
    if (rfidDetected && millis() - rfidDetectedTimestamp < RFID_STATUS_DURATION_MS) {
      out = "OK";
    } else {
      out = "WAIT";
    }
  };

  initialiserServeurWeb();
}

void loop() {
  thing.handle();
  server.handleClient();
}
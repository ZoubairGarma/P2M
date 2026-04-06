#include <Arduino.h>
#include <WiFi.h> // On ajoute la bibliothèque WiFi ici

// THINGER DOIT ÊTRE UNIQUEMENT ICI !
#include <ThingerESP32.h>
#include "config.h"
#include "camera.h"
#include "reseau.h"

// --- TES IDENTIFIANTS THINGER.IO ---
#define USERNAME "inesss"
#define DEVICE_ID "cameraa" // ou cameraa
#define DEVICE_CREDENTIAL "cameraa"

// --- VARIABLES GLOBALES ---
const char* ssid = "bartage";       
const char* password = "zoubaxd55";

ThingerESP32 thing(USERNAME, DEVICE_ID, DEVICE_CREDENTIAL);
WebServer server(80);

void setup() {
  Serial.begin(115200);
  delay(1000);

  initialiserCamera();

  // --- NOUVEAU : ON CONNECTE LE WIFI NOUS-MÊMES D'ABORD ---
  Serial.print("\nConnexion au WiFi " + String(ssid) + " ...");
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi Connecte !");

  // On donne les identifiants à Thinger (qui utilisera la connexion active)
  thing.add_wifi(ssid, password);

  // On lance le serveur Web
  initialiserServeurWeb();
}

void loop() {
  thing.handle();
  server.handleClient();
}
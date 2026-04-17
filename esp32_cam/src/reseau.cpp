#include "reseau.h"
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include <ThingerESP32.h>

// On importe uniquement l'instance Thinger définie dans main.cpp
extern ThingerESP32 thing; 

void initialiserWiFiEtCloud() {
  Serial.print("Connexion WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("\nWiFi connecté !");

  Serial.print(">>> Adresse IP de la caméra : ");
  Serial.println(WiFi.localIP());

  thing.add_wifi(ssid, password);
}

void initialiserServeurWeb() {
  server.on("/", HTTP_GET, []() {
    server.send(200, "text/plain", "Camera prete. En attente d'autorisation RFID du WROVER.");
  });

  // 1. Le WROVER appelle cette route (IL NE REÇOIT PAS DE PHOTO, JUSTE L'AUTORISATION)
  server.on("/authorize", HTTP_GET, []() {
    accessGranted = true;
    accessGrantedTimestamp = millis();
    rfidDetected = true;
    rfidDetectedTimestamp = millis();
    Serial.println(">>> Badge RFID valide reçu du WROVER ! Accès photo déverrouillé pour 10s.");
    
    // On répond au WROVER que l'accès est déverrouillé
    server.send(200, "text/plain", "ACCESS_GRANTED");
  });

  // 2. Thinger.io appelle cette route (IL REÇOIT L'IMAGE JPEG Binaire)
  server.on("/photo", HTTP_GET, capturePhoto);

  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);
  });
  
  server.begin();
  Serial.println("Serveur HTTP lancé !");
}
#include "reseau.h"
#include "config.h"
#include "camera.h"
#include <Arduino.h>
#include <WiFi.h>

void initialiserWiFiEtCloud() {
  // Vide
}

void initialiserServeurWeb() {
  // Le WiFi est déjà connecté, on affiche directement l'IP !
  Serial.println("\n====== SUCCES ======");
  Serial.print("Adresse IP Locale : http://");
  Serial.println(WiFi.localIP()); 
  Serial.println("====================");

  server.on("/", HTTP_GET, capturePhoto);
  server.on("/capture", HTTP_GET, actionCapteurIR);
  server.begin();
  Serial.println("Serveur Web demarre !");
}
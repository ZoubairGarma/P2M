
#include "reseau.h"
#include <WiFi.h>
#include "config.h"
#include "camera.h"
#include <ThingerESP32.h>
#include <esp_camera.h>

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
    server.send(200, "text/plain", "Camera prete.");
  });

  // ============================================
  // 🔄 NOUVEAU: Endpoint RFID du WROVER
  // ============================================
  // Le WROVER appelle cette route quand RFID authentifié
  server.on("/rfid/trigger", HTTP_GET, []() {
    Serial.println("\n🚀 Endpoint /rfid/trigger appelé par WROVER!");
    rfidDetected = true;
    rfidDetectedTimestamp = millis();
    Serial.println("✅ rfidDetected = true - Capture va démarrer automatiquement!");
    server.send(200, "text/plain", "RFID_TRIGGERED");
  });

  // Routes désactivées pour éviter capture à chaque refresh
  // Les photos sont maintenant capturées automatiquement au détection RFID
  server.on("/photo", HTTP_GET, []() {
    server.send(200, "text/plain", "Photos capturées automatiquement au RFID");
  });
  server.on("/authorize", HTTP_GET, []() {
    server.send(200, "text/plain", "Photos capturées automatiquement au RFID");
  });

  // Route pour servir l'image directement (pas de capture, juste l'image stockée)
  server.on("/image", HTTP_GET, []() {
    Serial.println(">>> /image requested!");
    camera_fb_t * fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("❌ Erreur capture pour /image");
      server.send(500, "text/plain", "Erreur capture");
      return;
    }
    Serial.printf("Image capturée: %d bytes\n", fb->len);
    WiFiClient client = server.client();
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: image/jpeg");
    client.println("Connection: close");
    client.printf("Content-Length: %d\r\n", fb->len);
    client.println();
    client.write(fb->buf, fb->len);
    esp_camera_fb_return(fb);
    Serial.println("Image envoyée!");
  });

  server.on("/favicon.ico", HTTP_GET, []() {
    server.send(204);
  });

  server.onNotFound([]() {
    Serial.print("Route introuvable: ");
    Serial.println(server.uri());
    server.send(404, "text/plain", "Route not found");
  });
  
  server.begin();
  Serial.println("Serveur HTTP lancé !");
}
}
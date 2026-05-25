# Configuration pour Reconnaissance Faciale ESP32-CAM

## 📋 Dépendances à Installer

### Option 1: Arduino IDE (recommandé pour démarrer rapidement)

**Board Manager URL:**
```
https://dl.espressif.com/dl/package_esp32_index.json
```

**Librairies à installer** (Arduino IDE > Sketch > Include Library > Manage Libraries):

| Librairie | Version | Source |
|-----------|---------|--------|
| ESP-DL | Latest | Espressif |
| Arduino ESP32 Core | 2.0+ | Espressif |
| WiFi | Built-in | Arduino |
| HTTPClient | Built-in | Arduino |

### Option 2: PlatformIO (recommandé pour production)

**platformio.ini:**
```ini
[env:esp32cam]
platform = espressif32
board = esp32cam
framework = arduino

# Dépendances
lib_deps =
    https://github.com/espressif/esp-dl.git
    https://github.com/espressif/arduino-esp32.git#master

# Configuration
board_build.partitions = default.csv
upload_speed = 921600
monitor_speed = 115200

# IDF-specific flags
build_flags =
    -DCORE_DEBUG_LEVEL=1
    -mfix-esp32-psram-cache-issue
```

---

## 🔌 Configuration Pins ESP32-CAM

Ajouter au fichier `board_config.h` ou définir dans votre code:

```cpp
// ESP32-CAM Camera Pins
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1  // (-1 = non utilisé)
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26  // SDA
#define SIOC_GPIO_NUM     27  // SCL
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

#define LED_GPIO_NUM      -1  // (-1 = pas de LED flash)
```

---

## 📦 Includes Obligatoires

```cpp
// ESP-IDF & Arduino
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// ESP-DL (Détection)
#include "human_face_detect.hpp"

// ESP-DL (Reconnaissance - optionnel)
#include "human_face_recognition.hpp"

// Utilitaires
#include "dl_image_jpeg.hpp"
#include "dl_image_convert.hpp"
#include "spiflash_fatfs.hpp"

// WiFi & HTTP
#include <WiFi.h>
#include "esp_http_client.h"

// STL
#include <vector>
#include <filesystem>
#include <stdexcept>
```

---

## ⚙️ Configurations MenuConfig (idf.py)

Si vous utilisez ESP-IDF directement:

```
idf.py menuconfig
```

Naviguer vers:

### 1. Component → ESP-DL → Human Face Detection
```
[*] Enable Human Face Detection
[*] Component DL
[*] Face Detection Model (select: MSR, MNP, ou ESPDet)
```

### 2. Component → PSRAM
```
[*] Enable PSRAM
    PSRAM clock frequency: 80MHz
    SPI mode: Quad
```

### 3. Component → Camera
```
[*] Enable Camera
    Camera Sensor: OV2640, OV5640, ou OV3660
```

---

## 🚀 Structure de Projet Minimale

```
mon_projet/
├── platformio.ini           # Configuration PlatformIO
├── src/
│   ├── main.cpp            # Point d'entrée
│   ├── camera_config.h     # Configuration caméra
│   ├── face_detection.h    # Détection
│   └── telegram_alert.h    # Alert Telegram
├── lib/                    # (vide - dépendances depuis platformio.ini)
└── data/                   # (optionnel - images de test)
```

---

## 🔋 Requis Hardware

| Composant | Specs | Notes |
|-----------|-------|-------|
| ESP32 | Minimum 4MB Flash, 4MB PSRAM | S3 ou P4 recommandé |
| Caméra | OV2640, OV3660, ou OV5640 | Module CAM classique |
| Alimentation | 5V / 500mA+ | Adapter USB ou batterie |
| Antenne WiFi | Built-in | Pour notifications Telegram |

---

## 🧪 Tests Minimaux (Arduino IDE)

### Test 1: Vérifier caméra
```cpp
void test_camera() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (fb) {
        Serial.printf("Frame: %d x %d\n", fb->width, fb->height);
        esp_camera_fb_return(fb);
    } else {
        Serial.println("ERROR: Camera not working");
    }
}
```

### Test 2: Vérifier détecteur
```cpp
void test_detector() {
    HumanFaceDetect *detector = new HumanFaceDetect();
    // ... créer image ...
    auto results = detector->run(image);
    Serial.printf("Faces found: %d\n", results.size());
    delete detector;
}
```

### Test 3: Vérifier WiFi + Telegram
```cpp
void test_telegram() {
    WiFi.begin("SSID", "PASSWORD");
    // ... attendre connection ...
    
    // Envoyer test message
    send_telegram_alert("Test+message");
}
```

---

## 📊 Benchmarks (ESP32-S3)

| Operation | Time | Memory |
|-----------|------|--------|
| Face Detection (QVGA) | 150-200ms | 2MB |
| Face Recognition (112x112) | 250ms | 3MB |
| WiFi POST Telegram | 500-1000ms | 50KB |
| JPEG Encoding | 100-150ms | 1MB |

**Total Memory:** ~10-15MB (PSRAM required)

---

## 🐛 Troubleshooting

### Erreur: "human_face_detect.hpp not found"
```bash
# Solution 1: Vérifier que ESP-DL est installé
idf.py component-list

# Solution 2: Ajouter explicitement
# Dans platformio.ini:
lib_deps =
    espressif/esp-dl @ ^3.2.0
```

### Erreur: "PSRAM not available"
```cpp
// Vérifier si PSRAM détecté
if (psramFound()) {
    Serial.println("PSRAM OK");
} else {
    Serial.println("ERROR: No PSRAM!");
    // Réduire taille frame ou models
}
```

### Erreur: "Stack overflow"
```cpp
// Augmenter stack dans xTaskCreate:
xTaskCreate(task, "task", 16384, NULL, 3, NULL);  // 16KB au lieu de 8KB
```

### Caméra freeze
```cpp
// Solution: Vérifier pins SDA/SCL et configuration XCLK
// Ajouter delays:
vTaskDelay(150 / portTICK_PERIOD_MS);  // Avant esp_camera_fb_get()
```

---

## 📝 Exemple platformio.ini Complet

```ini
[env:esp32cam]
platform = espressif32 @ ^6.0.0
board = esp32cam
framework = arduino
board_build.f_cpu = 240000000L
board_build.f_flash = 40000000L
board_build.flash_mode = qio
board_build.partitions = huge_app.csv

monitor_speed = 115200
upload_speed = 921600

lib_deps =
    espressif/esp-dl @ ^3.2.0
    bblanchon/ArduinoJson @ ^6.21.0

build_flags =
    -DCORE_DEBUG_LEVEL=2
    -DMQTT_MAX_PACKET_SIZE=1024
    -mfix-esp32-psram-cache-issue

# Ajouter si vous utilisez SD Card
extra_scripts =
    post:extra_script.py
```

---

## 🔐 Sécurité - Credentials

**Ne JAMAIS commiter les credentials** (tokens, WiFi password):

```cpp
// ❌ MAUVAIS
const char *TELEGRAM_TOKEN = "123456:ABCDEFgh...";

// ✅ BON: Utiliser credentials.h en .gitignore
#include "credentials.h"
// credentials.h (NOT in git):
// #define TELEGRAM_TOKEN "..."
```

---

## 📚 Ressources Officielles

| Ressource | URL |
|-----------|-----|
| ESP-DL Docs | https://docs.espressif.com/projects/esp-dl |
| Face Detection Example | https://github.com/espressif/esp-dl/tree/master/examples/human_face_detect |
| Face Recognition Example | https://github.com/espressif/esp-dl/tree/master/examples/human_face_recognition |
| Arduino ESP32 Core | https://github.com/espressif/arduino-esp32 |
| Telegram Bot API | https://core.telegram.org/bots/api |

---

## ✅ Checklist Avant Déploiement

- [ ] Caméra testé et image stable
- [ ] WiFi connecté avec bon signal
- [ ] Token Telegram valide et chat_id correct
- [ ] PSRAM détecté et fonctionnel
- [ ] Stack RTOS suffisant (minimum 8KB)
- [ ] Face detection model charge sans erreur
- [ ] Seuil de confiance réglé (0.5-0.7 recommandé)
- [ ] Telegram alert testé manuellement
- [ ] Cooldown entre alerts configuré

---

**Créé: 2026-05-25** | **Espressif Official**

# Guide Pratique: Reconnaissance Faciale ESP32-CAM pour espressif32@3.5.0

## Contexte Projet
- **Plateforme**: ESP32-CAM
- **PlatformIO**: espressif32@3.5.0 (ou arduino-esp32 3.5.0)
- **Objectif**: Reconnaissance faciale en temps réel

---

## Option 1: ✅ RECOMMANDÉE - ESP-DL + Modèle Personnalisé

### Avantages
- Support officiel Espressif (maintenu)
- Performance optimisée
- Flexible avec différents modèles
- Compatible espressif32@3.5.0

### Inconvénients
- Apprentissage courbe plus raide
- Configuration initiale plus complexe

### Setup PlatformIO

**platformio.ini**
```ini
[env:esp32cam-espdl]
platform = espressif32@3.5.0
board = esp32-cam
framework = arduino

lib_deps =
    esp32-camera
    espressif/esp-dl

build_flags =
    -DCONFIG_ESP32_SPIRAM_SUPPORT=1
    -DCONFIG_ESP32_DEFAULT_CPU_FREQ_240=1

monitor_speed = 115200
upload_speed = 921600
```

### Code Minimal (Detection seulement)
```cpp
#include <Arduino.h>
#include "esp_camera.h"
#include "esp_dl.hpp"
#include "dl_image.hpp"

// Configuration caméra
#define CAM_PIN_PWDN    -1
#define CAM_PIN_RESET   -1
#define CAM_PIN_XCLK    21
#define CAM_PIN_SIOD    26
#define CAM_PIN_SIOC    27
#define CAM_PIN_D7      35
#define CAM_PIN_D6      34
#define CAM_PIN_D5      39
#define CAM_PIN_D4      36
#define CAM_PIN_D3      19
#define CAM_PIN_D2      18
#define CAM_PIN_D1       5
#define CAM_PIN_D0       4
#define CAM_PIN_VSYNC   25
#define CAM_PIN_HREF    23
#define CAM_PIN_PCLK    22

static camera_config_t camera_config = {
    .pin_pwdn = CAM_PIN_PWDN,
    .pin_reset = CAM_PIN_RESET,
    .pin_xclk = CAM_PIN_XCLK,
    .pin_sccb_sda = CAM_PIN_SIOD,
    .pin_sccb_scl = CAM_PIN_SIOC,
    .pin_d7 = CAM_PIN_D7,
    .pin_d6 = CAM_PIN_D6,
    .pin_d5 = CAM_PIN_D5,
    .pin_d4 = CAM_PIN_D4,
    .pin_d3 = CAM_PIN_D3,
    .pin_d2 = CAM_PIN_D2,
    .pin_d1 = CAM_PIN_D1,
    .pin_d0 = CAM_PIN_D0,
    .pin_vsync = CAM_PIN_VSYNC,
    .pin_href = CAM_PIN_HREF,
    .pin_pclk = CAM_PIN_PCLK,
    .xclk_freq_hz = 20000000,
    .ledc_timer = LEDC_TIMER_0,
    .ledc_channel = LEDC_CHANNEL_0,
    .pixel_format = PIXFORMAT_RGB565,
    .frame_size = FRAMESIZE_QVGA,  // 320x240
    .jpeg_quality = 10,
    .fb_count = 2,
    .grab_mode = CAMERA_GRAB_WHEN_EMPTY,
};

void setup() {
    Serial.begin(115200);
    Serial.println("\n\nESP32-CAM Face Detection (ESP-DL)");
    
    // Initialiser caméra
    esp_err_t err = esp_camera_init(&camera_config);
    if (err != ESP_OK) {
        Serial.printf("Camera init failed: %d\n", err);
        while(1);
    }
    
    Serial.println("Camera initialized");
    
    // Configurer PSRAM si disponible
    Serial.printf("PSRAM: %d bytes free\n", esp_get_free_psram());
}

void loop() {
    // Capturer frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("Frame capture failed");
        delay(100);
        return;
    }
    
    // À faire: Charger et exécuter modèle ESP-DL ici
    // Pour l'instant, afficher taille du frame
    Serial.printf("Frame: %dx%d, size: %d bytes\n", 
                  fb->width, fb->height, fb->len);
    
    esp_camera_fb_return(fb);
    delay(1000);
}
```

### Pas Suivant: Intégrer Modèle
```cpp
// TODO: Ajouter modèle ESP-DL
// 1. Créer/convertir modèle en .espdl
// 2. Inclure data/model.espdl
// 3. Charger via ESP-DL API
// 4. Exécuter inférence sur frames
```

---

## Option 2: ✅ BONNE - TensorFlow Lite pour Microcontrôleurs

### Avantages
- Modèles de reconnaissance faciale disponibles
- Support cross-plateforme
- Bonne documentation
- Flexible

### Inconvénients
- Espace mémoire limité
- Performance moindre qu'ESP-DL

### Setup PlatformIO

**platformio.ini**
```ini
[env:esp32cam-tflite]
platform = espressif32@3.5.0
board = esp32-cam
framework = arduino

lib_deps =
    esp32-camera
    tensorflow/tensorflow-lite-esp32

build_flags =
    -DCONFIG_ESP32_SPIRAM_SUPPORT=1
    -DTF_LITE_MICRO_USE_TFLM_CONTEXT=1

monitor_speed = 115200
```

### Code Minimal
```cpp
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"

// À ajouter: model_data.h contenant le modèle quantisé
extern const unsigned char model_data[];
extern const unsigned int model_data_len;

tflite::MicroInterpreter* interpreter;

void setup() {
    Serial.begin(115200);
    
    // Charger modèle
    const tflite::Model* model = tflite::GetModel(model_data);
    
    static tflite::MicroMutableOpResolver<4> resolver;
    resolver.AddConv2D();
    resolver.AddMaxPool2D();
    resolver.AddFullyConnected();
    resolver.AddSoftmax();
    
    static uint8_t tensor_arena[200 * 1024];  // 200KB
    
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, sizeof(tensor_arena), nullptr);
    
    interpreter = &static_interpreter;
    
    if (interpreter->AllocateTensors() != kTfLiteOk) {
        Serial.println("AllocateTensors failed");
        while(1);
    }
    
    Serial.println("TFLite initialized");
}

void loop() {
    // Capturer
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;
    
    // Redimensionner et copier en input tensor
    float* input = interpreter->typed_input_tensor<float>(0);
    // ... conversion image ...
    
    // Inférer
    if (interpreter->Invoke() != kTfLiteOk) {
        Serial.println("Invoke failed");
        esp_camera_fb_return(fb);
        return;
    }
    
    // Traiter output
    float* output = interpreter->typed_output_tensor<float>(0);
    
    esp_camera_fb_return(fb);
    delay(100);
}
```

---

## Option 3: ⚠️ ALTERNATIVE - Edge Impulse

### Avantages
- Plateforme visuelle complète
- Entraînement facile (pas d'expertise ML)
- Modèles optimisés automatiquement
- Excellent pour prototypage

### Inconvénients
- Gratuit limité (~3 projets)
- Dépendance à service cloud
- Performance variable

### Workflow
1. Créer projet sur https://studio.edgeimpulse.com
2. Uploader dataset de visages
3. Entraîner modèle
4. Exporter pour Arduino-ESP32
5. Intégrer dans PlatformIO

### Code Minimal Edge Impulse
```cpp
#include <your_project_name_inferencing.h>

void loop() {
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) return;
    
    // Convertir en float32 RGB
    float features[320*240*3];
    // ... conversion ...
    
    // Créer signal
    signal_t signal;
    signal.total_length = 320 * 240 * 3;
    signal.get_data = &get_feature_data;
    signal.user_data = features;
    
    // Exécuter
    ei_impulse_result_t result = { 0 };
    EI_IMPULSE_ERROR err = run_classifier(&signal, &result, false);
    
    // Résultats
    for (uint16_t i = 0; i < EI_CLASSIFIER_OBJECT_DETECTION_COUNT; i++) {
        ei_impulse_result_bounding_box_t bb = result.bounding_boxes[i];
        if (bb.confidence > 0.5) {
            Serial.printf("Detected face at (%d,%d) size %dx%d\n",
                         bb.x, bb.y, bb.width, bb.height);
        }
    }
    
    esp_camera_fb_return(fb);
}
```

---

## Option 4: ❌ NON RECOMMANDÉE - esp-face (Dépréciée)

### Problèmes
- Plus maintenue
- Incompatible avec espressif32@3.5.0
- Dépendances cassées
- Pas de support

### Si vraiment nécessaire: Rétrograder
```ini
[env:legacy]
platform = espressif32@2.6.2  ; Version ancienne
board = esp32-cam
framework = arduino
lib_deps =
    esp32-camera
    esp-face  ; Ancienne version
```

**Verdict**: Ne pas faire cela en production.

---

## Comparaison Options

| Critère | ESP-DL | TFLite | Edge Impulse | esp-face |
|---------|--------|--------|--------------|----------|
| Maintenance | ✅ Actif | ✅ Actif | ✅ Actif | ❌ Archivé |
| Performance | ⭐⭐⭐ | ⭐⭐ | ⭐⭐ | ⭐⭐⭐ |
| Flexibilité | ⭐⭐⭐ | ⭐⭐⭐ | ⭐ | ⭐⭐ |
| Apprentissage | Moyen | Facile | Très Facile | Facile |
| Coût | Gratuit | Gratuit | Gratuit/Payant | Gratuit |
| esp32@3.5.0 | ✅ | ✅ | ✅ | ❌ |

---

## Checklist Implementation

### Avant de commencer
- [ ] ESP32-CAM fonctionnelle et testée
- [ ] Camera WebServer basique qui marche
- [ ] PSRAM activé et utilisable
- [ ] PlatformIO configuré correctement

### Choix approach
- [ ] Décider entre ESP-DL / TFLite / Edge Impulse
- [ ] Lire la doc officielle du choix
- [ ] Vérifier espace mémoire disponible

### Implémentation
- [ ] Configurer platformio.ini
- [ ] Charger library dépendante
- [ ] Compiler et tester build
- [ ] Implémenter code minimal
- [ ] Tester avec frames réelles
- [ ] Optimiser performance

### Production
- [ ] Tester stabilité sur longue durée
- [ ] Mesurer consommation mémoire/CPU
- [ ] Documenter les résultats
- [ ] Mettre à jour pour versions futures

---

## Ressources Utiles

### Documentation Officielle
- **ESP-DL**: https://docs.espressif.com/projects/esp-dl/
- **TensorFlow Lite Micro**: https://github.com/tensorflow/tensorflow/tree/master/tensorflow/lite/micro
- **ESP32-Camera**: https://github.com/espressif/esp32-camera

### Exemples
- **ESP-DL Examples**: https://github.com/espressif/esp-dl/tree/master/examples
- **Arduino-esp32**: https://github.com/espressif/arduino-esp32/tree/master/libraries/ESP32/examples

### Modèles Pré-entraînés
- ESP-DL Model Zoo: https://github.com/espressif/esp-dl/tree/master/models
- TFLite Hub: https://www.tensorflow.org/lite/models
- Edge Impulse Registry: https://www.edgeimpulse.com/

---

## Dépannage Courant

### Problème: "Camera init failed"
**Solution**: 
- Vérifier pins GPIO
- Activer PSRAM dans menuconfig
- Vérifier tension alimentation

### Problème: "Out of memory"
**Solution**:
- Réduire taille frame (QVGA au lieu de VGA)
- Réduire taille buffer ESP-DL
- Utiliser modèle quantisé (int8 au lieu float32)

### Problème: "Model load failed"
**Solution**:
- Vérifier chemin du modèle
- Vérifier format .espdl vs .tflite
- Vérifier SPIFFS/LittleFS

### Problème: "Performance lente"
**Solution**:
- Profiler avec tools intégrés
- Réduire résolution image
- Utiliser quantization
- Activer optimisations CPU

---

## Code Complet Exemple (Framework)

```cpp
#include <Arduino.h>
#include "esp_camera.h"

// TODO: Ajouter includes ESP-DL ou TFLite

const char* TAG = "MAIN";

void setup() {
    Serial.begin(115200);
    delay(100);
    
    Serial.println("\n\nESP32-CAM Face Recognition");
    Serial.println("Version: 1.0");
    Serial.println("Platform: PlatformIO espressif32@3.5.0");
    
    // 1. Initialiser caméra
    if (init_camera() != ESP_OK) {
        Serial.println("Camera init failed!");
        while(1) delay(100);
    }
    
    // 2. Initialiser modèle
    if (init_model() != ESP_OK) {
        Serial.println("Model init failed!");
        while(1) delay(100);
    }
    
    Serial.println("Setup complete!");
}

void loop() {
    // Capturer frame
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        delay(50);
        return;
    }
    
    // Exécuter détection
    int num_faces = detect_faces(fb);
    
    // Afficher résultats
    if (num_faces > 0) {
        Serial.printf("Detected %d face(s)\n", num_faces);
        // TODO: Traiter détections
    }
    
    // Cleanup
    esp_camera_fb_return(fb);
    
    // FPS throttle
    delay(50);  // ~20 FPS
}

// Stubs à implémenter
esp_err_t init_camera() {
    // Retourner ESP_OK si succès
    return ESP_OK;
}

esp_err_t init_model() {
    // Retourner ESP_OK si succès
    return ESP_OK;
}

int detect_faces(camera_fb_t *fb) {
    // Retourner nombre de visages détectés
    return 0;
}
```

---

## Conclusion

**Pour espressif32@3.5.0 + ESP32-CAM en 2026**:

1. **Éviter**: esp-face (dépréciée)
2. **Préférer**: ESP-DL (officiel, moderne)
3. **Alternative**: TFLite ou Edge Impulse

Suivre la documentation officielle de votre choix et adapter au projet.

---

**Guide généré**: Mai 2026  
**Compatible**: PlatformIO espressif32@3.5.0+

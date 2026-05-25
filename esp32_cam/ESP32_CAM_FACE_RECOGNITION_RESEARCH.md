# Recherche: Headers et API de Reconnaissance Faciale ESP32-CAM - espressif32@3.5.0

## ⚠️ Information Importante

**Constat clé**: Espressif a **migré son approche** de reconnaissance faciale. La bibliothèque `esp-face` (anciennement disponible avec MTMN/MobileNet) a été **archivée/remplacée** par **ESP-DL** (Espressif Deep Learning) à partir de 2024-2025.

---

## 1. État Actuel - Approche Moderne (ESP-DL)

### Sources Officielles
- **GitHub**: https://github.com/espressif/esp-dl
- **Documentation**: https://docs.espressif.com/projects/esp-dl/en/latest/index.html
- **Component Registry**: https://components.espressif.com/components/espressif/esp-dl
- **Version Récente**: ESP-DL v3.2.0 (2025-10-20)

### Architecture ESP-DL
```
ESP-DL = Framework de Deep Learning léger pour ESP32
- Charge des modèles au format .espdl (FlatBuffers, pas Protobuf)
- Support des opérateurs: Conv, Gemm, Add, Mul, etc.
- Planneur mémoire statique avec allocation optimisée
- Planification dual-core pour Conv2D et DepthwiseConv2D
- Activation par LUT 8-bit (LookUp Table)
```

### Headers ESP-DL (modèle moderne)
```c
// Inclusions typiques pour ESP-DL
#include "esp_dl.hpp"  // ou autres headers génériques de modèles
#include "dl_image.hpp"
#include "dl_math.hpp"
```

### Modèles de reconnaissance disponibles dans ESP-DL
- **YOLO11n** (détection d'objets)
- **YOLO26** (nouvelle architecture, contributeurs)
- **MobileNetV2** (classification)
- **ESPDet-Pico** (détection légère)
- Cat Detection (exemple spécialisé)

---

## 2. Approche Historique - API Dépréciée (esp-face)

### ⚠️ Statut: ARCHIVÉE/REMPLACÉE
Les anciennes API suivantes **ne sont plus maintenues** pour espressif32@3.5.0:

### Headers Anciens (Non disponibles en 3.5.0)
```c
// DÉPRÉCIÉS - Ne pas utiliser pour 3.5.0
#include "fd_forward.h"      // Face Detection Forward
#include "fr_forward.h"      // Face Recognition Forward
#include "dl_lib.h"          // DL Library
#include "dl_image.hpp"      // Image processing
```

### Structures DL Historiques (Obsolètes)
```c
// Ces structures existaient dans l'ancienne API esp-face
typedef struct {
    uint16_t w;
    uint16_t h;
    uint8_t c;  // Canaux
    uint8_t *data;
} dl_matrix3du_t;  // 3D unsigned 8-bit matrix

typedef struct {
    int x;
    int y;
    int w;
    int h;
} box_t;  // Boîte de détection

typedef struct {
    int count;
    box_t *boxes;
} box_array_t;  // Tableau de boîtes
```

### Fonctions Anciennes (Dépréciées)
```c
// Face Detection - MTMN Algorithm
// DÉPRÉCIÉE - Ne pas utiliser
// esp_err_t face_detect(dl_matrix3du_t *image, box_array_t *boxes);
// esp_err_t detect_face_network(...);

// Face Recognition - MobileNet
// DÉPRÉCIÉE - Ne pas utiliser
// esp_err_t face_recognition(...);
// esp_err_t get_face_embeddings(...);

// Alignment
// DÉPRÉCIÉE - Ne pas utiliser
// esp_err_t align_face(...);
```

---

## 3. Ce qui est Réellement Disponible dans espressif32@3.5.0

### Option 1: Camera Driver Basique
```c
#include "esp_camera.h"

typedef struct {
    // Configuration pin
    int pin_pwdn;
    int pin_reset;
    // ... autres pins
    pixformat_t pixel_format;
    framesize_t frame_size;
    int jpeg_quality;
} camera_config_t;

// API Camera
esp_err_t esp_camera_init(const camera_config_t *config);
camera_fb_t *esp_camera_fb_get(void);
void esp_camera_fb_return(camera_fb_t *fb);
```

### Option 2: ESP-DL pour Modèles Génériques
```cpp
#include "esp_dl.hpp"

// Modèles disponibles:
// - Detection (YOLO)
// - Classification (MobileNet)
// - Custom models (.espdl format)
```

---

## 4. Solutions Recommandées pour ESP32-CAM 3.5.0

### ✅ Solution A: Utiliser ESP-DL avec Modèle YOLO (Détection)
```cpp
#include "esp_camera.h"
#include "esp_dl.hpp"

// Charger un modèle de détection
// Passer une image de la caméra au modèle
// Récupérer les boîtes de détection
```

**Avantages**: Moderne, maintenu, flexible  
**Inconvénient**: Pas de reconnaissance faciale spécifique (mais détection générale)

### ✅ Solution B: Edge Impulse ou TensorFlow Lite
```c
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"

// Modèles TFLM pour reconnaissance faciale
// Méthode: Détection puis reconnaissance
```

### ❌ Solution C: Chercher esp-face sur des versions anciennes
- Versions historiques: < 2024 (arduino-esp32 < 2.0.0)
- Recommandation: **Ne pas utiliser** - maintenance arrêtée

---

## 5. Chemins d'Investigation Complémentaires

### A. Exemple Arduino-esp32 (Caméra WebServer)
- Fichier: https://github.com/espressif/arduino-esp32/blob/master/libraries/ESP32/examples/Camera/CameraWebServer/CameraWebServer.ino
- **Contient**: Exemple de capture caméra UNIQUEMENT
- **N'inclut PAS**: Reconnaissance faciale

### B. Documentation Officielle
- Docs Espressif: https://docs.espressif.com/
- ESP-IDF 5.3+: Requis pour ESP-DL
- PlatformIO: https://docs.platformio.org/

### C. Modèles ESP-DL Disponibles
- Repository: https://github.com/espressif/esp-dl/tree/master/models
- Contient: Face recognition models (if any)
- Format: `.espdl` (quantisé, optimisé)

---

## 6. Signatures de Fonction Actuelles (Modernes)

### Camera Capture
```c
esp_err_t esp_camera_init(const camera_config_t *config);
camera_fb_t* esp_camera_fb_get(void);
void esp_camera_fb_return(camera_fb_t *fb);
esp_err_t esp_camera_sensor_get_id(uint32_t *id);
```

### Format Image Modern (si utilisation de modèles)
```c
typedef struct {
    uint8_t *buf;        // Image buffer
    size_t len;          // Buffer length
    size_t width;        // Width
    size_t height;       // Height
    pixformat_t format;  // Format (JPEG, RGB565, etc)
} camera_fb_t;
```

### Détection Générique (ESP-DL ou équivalent)
```cpp
// Signature générique (pas spécifique à reconnaissance faciale)
// DL Model Load
// Input: preprocessed image (usually 224x224, RGB)
// Output: detection results (boxes, scores)
```

---

## 7. URLs Officielles pour Sources

### Repositories Clés
| Source | URL | Statut |
|--------|-----|--------|
| ESP32-Camera Driver | https://github.com/espressif/esp32-camera | ✅ Actif |
| Arduino-ESP32 | https://github.com/espressif/arduino-esp32 | ✅ Actif |
| ESP-DL (Moderne) | https://github.com/espressif/esp-dl | ✅ Actif |
| esp-face (Historique) | https://github.com/espressif/esp-face | ⚠️ Redirect → ESP-DL |
| ESP-Detection | https://github.com/espressif/esp-detection | ✅ Nouveau (2025) |

### Documentation
- **ESP-DL Docs**: https://docs.espressif.com/projects/esp-dl/en/latest/
- **ESP-IDF Docs**: https://docs.espressif.com/projects/esp-idf/
- **PlatformIO Docs**: https://docs.platformio.org/

---

## 8. Conclusion et Recommandations

### Pour ESP32-CAM avec espressif32@3.5.0:

1. **❌ Ne pas chercher**: `fd_forward.h`, `fr_forward.h` - **Dépréciées**
2. **✅ Utiliser**: ESP-DL ou un modèle TensorFlow Lite personnalisé
3. **✅ Pour détection simple**: YOLO11n via ESP-DL
4. **✅ Pour reconnaissance faciale personnalisée**: Edge Impulse + modèle TFLM
5. **📚 Documentation clé**: https://docs.espressif.com/projects/esp-dl/en/latest/

### Prochaines Étapes
- [ ] Cloner/consulter: https://github.com/espressif/esp-dl
- [ ] Consulter exemples: `/models/` et `/examples/`
- [ ] Adapter un modèle de détection ou de reconnaissance face
- [ ] Compiler avec ESP-IDF 5.3+ ou PlatformIO espressif32 3.5.0+

---

**Dernière mise à jour**: Mai 2026  
**Status**: Recherche complète - esp-face est archivée, recommandation vers ESP-DL

# Appendice: Signatures API Historiques esp-face (Référence Archivée)

## ⚠️ Avertissement: Références Historiques Uniquement

Les informations suivantes sont **archivées** et ne sont plus supportées dans:
- `espressif32@3.5.0`
- `esp32-camera` versions récentes
- Arduino-ESP32 versions 2.0.0+

**Usage**: Documentation historique, migration de projets anciens, compréhension du contexte.

---

## 1. Headers Anciens esp-face (Structure Historique)

### Structure de Repository esp-face (avant archivage)
```
esp-face/
├── face_detection/
│   ├── include/
│   │   ├── fd_forward.h
│   │   ├── face_detect.h
│   │   └── ...
│   ├── lib/
│   └── src/
├── face_recognition/
│   ├── include/
│   │   ├── fr_forward.h
│   │   ├── face_recognition.h
│   │   └── ...
│   ├── lib/
│   └── src/
├── face_alignment/
├── dl_lib/
│   ├── include/
│   │   ├── dl_lib.h
│   │   ├── dl_image.hpp
│   │   ├── dl_matrix.hpp
│   │   └── ...
│   └── src/
└── idf_component.yml
```

### Headers Principaux (Dépréciés)
```c
// Face Detection - fd_forward.h
#ifndef _FD_FORWARD_H_
#define _FD_FORWARD_H_

#include "dl_lib.h"

typedef struct {
    uint16_t w;
    uint16_t h;
    uint8_t c;
    uint8_t *data;
} dl_matrix3du_t;

typedef struct {
    int x;
    int y;
    int w;
    int h;
} box_t;

typedef struct {
    int count;
    box_t *boxes;
    float *scores;
} box_array_t;

// Détection faciale MTCNN (Multi-Task Cascaded Convolutional Networks)
// Retourne un tableau de boîtes de détection
box_array_t *face_detect(dl_matrix3du_t *image);
void free_box_array(box_array_t *box_array);

#endif
```

```c
// Face Recognition - fr_forward.h
#ifndef _FR_FORWARD_H_
#define _FR_FORWARD_H_

#include "dl_lib.h"

// Structure pour embeddings de visage (128 dimensions típico)
typedef struct {
    float *data;
    int len;  // Taille du vecteur (ex: 128)
} face_embedding_t;

// Reconnaissance faciale via MobileNet
face_embedding_t face_recognition(dl_matrix3du_t *aligned_face);
float face_distance(face_embedding_t *emb1, face_embedding_t *emb2);

#endif
```

---

## 2. Structures de Données Historiques

### dl_matrix3du_t (3D Unsigned 8-bit Matrix)
```c
/**
 * Matrice 3D pour images 8-bit
 * Utilisée pour stocker les images RGB/YUV depuis la caméra
 */
typedef struct {
    uint16_t w;      // Largeur
    uint16_t h;      // Hauteur
    uint8_t c;       // Nombre de canaux (3 pour RGB, 1 pour grayscale)
    uint8_t *data;   // Données image (w * h * c bytes)
} dl_matrix3du_t;

// Exemple allocation:
// dl_matrix3du_t *image = dl_matrix3du_alloc(320, 240, 3);  // 320x240 RGB
```

### box_t (Boîte de Détection)
```c
/**
 * Représente une boîte englobante pour un visage détecté
 */
typedef struct {
    int x;  // Coordonnée X (coin supérieur gauche)
    int y;  // Coordonnée Y (coin supérieur gauche)
    int w;  // Largeur de la boîte
    int h;  // Hauteur de la boîte
} box_t;
```

### box_array_t (Tableau de Boîtes)
```c
/**
 * Tableau de résultats de détection
 */
typedef struct {
    int count;            // Nombre de visages détectés
    box_t *boxes;         // Tableau des boîtes
    float *scores;        // Scores de confiance pour chaque détection
    float *landmarks;     // Points repères faciaux (optionnel)
} box_array_t;

// Exemple usage:
// box_array_t *detection = face_detect(image);
// for (int i = 0; i < detection->count; i++) {
//     printf("Face %d: (%d,%d) %dx%d - score: %.2f\n",
//            i, detection->boxes[i].x, detection->boxes[i].y,
//            detection->boxes[i].w, detection->boxes[i].h,
//            detection->scores[i]);
// }
// free_box_array(detection);
```

### Face Embedding (Vecteur de Caractéristiques)
```c
/**
 * Vecteur de caractéristiques extraits d'un visage
 * Dimension standard: 128 éléments (float)
 * Utilisé pour comparaison et identification
 */
typedef struct {
    float *data;    // Vecteur des caractéristiques
    int len;        // Nombre d'éléments (128 typiquement)
    float norm;     // Norme L2 du vecteur
} face_embedding_t;

// Distance euclidienne entre deux embeddings:
// distance = sqrt(sum((emb1[i] - emb2[i])^2))
// Seuil typique: distance < 0.6 pour même personne
```

---

## 3. Fonctions Historiques (Dépréciées)

### Face Detection API
```c
/**
 * Détecte tous les visages dans une image
 * @param image: Matrice image RGB 320x240 ou 160x120
 * @return: box_array_t avec tous les visages détectés
 * 
 * Algorithme: MTCNN (Multi-Task Cascaded CNN)
 * Étapes: P-Net → R-Net → O-Net
 */
box_array_t *face_detect(dl_matrix3du_t *image);

/**
 * Détecte avec NMS (Non-Maximum Suppression)
 */
box_array_t *face_detect_nms(dl_matrix3du_t *image,
                             float nms_threshold);

/**
 * Libère la mémoire d'un box_array
 */
void free_box_array(box_array_t *box_array);
```

### Face Alignment (Recadrage)
```c
/**
 * Aligne un visage détecté à 112x112 pixels
 * Utilise les landmarks (points repères)
 * Retourne une image alignée prête pour reconnaissance
 */
dl_matrix3du_t *face_alignment(dl_matrix3du_t *image,
                                box_t *box,
                                float *landmarks);

/**
 * Variante simple sans landmarks
 */
dl_matrix3du_t *crop_face(dl_matrix3du_t *image,
                           box_t *box);
```

### Face Recognition API (MobileNet)
```c
/**
 * Extrait un vecteur de caractéristiques (embedding) d'un visage
 * @param face_image: Image alignée 112x112 RGB
 * @return: Vecteur de 128 floats
 * 
 * Modèle: MobileNet v1 (compacte)
 * Temps d'inférence: ~100ms sur ESP32
 */
face_embedding_t *face_recognition(dl_matrix3du_t *face_image);

/**
 * Calcule la distance entre deux visages
 * Retour < 0.6: même personne
 * Retour > 0.6: personnes différentes
 */
float face_similarity(face_embedding_t *emb1,
                      face_embedding_t *emb2);

/**
 * Identifie le visage parmi une base de données
 */
int face_identify(face_embedding_t *face_emb,
                  face_embedding_t *database[],
                  int db_size,
                  float threshold);

/**
 * Libère un embedding
 */
void free_face_embedding(face_embedding_t *embedding);
```

---

## 4. Exemple de Code Complet (Historique)

### Workflow Typique Ancien (esp-face)
```c
#include "esp_camera.h"
#include "fd_forward.h"
#include "fr_forward.h"
#include "dl_lib.h"

void face_recognition_task() {
    // 1. Capturer une image
    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(TAG, "Camera capture failed");
        return;
    }
    
    // 2. Convertir en format DL (RGB, 320x240)
    dl_matrix3du_t *image = dl_matrix3du_alloc(fb->width, fb->height, 3);
    // ... conversion JPEG/raw vers RGB ...
    
    // 3. Détecter les visages
    box_array_t *detections = face_detect(image);
    
    if (detections && detections->count > 0) {
        ESP_LOGI(TAG, "Detected %d faces", detections->count);
        
        // 4. Pour chaque visage détecté
        for (int i = 0; i < detections->count; i++) {
            box_t *box = &detections->boxes[i];
            
            // 5. Aligner le visage
            dl_matrix3du_t *aligned = crop_face(image, box);
            
            // 6. Extraire les caractéristiques
            face_embedding_t *embedding = face_recognition(aligned);
            
            // 7. Comparer avec base de données (exemple)
            float min_distance = FLT_MAX;
            int matched_id = -1;
            
            for (int j = 0; j < known_faces_count; j++) {
                float dist = face_similarity(embedding, 
                                            known_embeddings[j]);
                if (dist < 0.6 && dist < min_distance) {
                    min_distance = dist;
                    matched_id = j;
                }
            }
            
            if (matched_id >= 0) {
                ESP_LOGI(TAG, "Face %d matches known person %d (distance: %.2f)",
                        i, matched_id, min_distance);
            } else {
                ESP_LOGI(TAG, "Face %d is unknown", i);
            }
            
            // Nettoyage
            dl_free(aligned);
            free_face_embedding(embedding);
        }
        
        free_box_array(detections);
    }
    
    // Nettoyage
    dl_free(image);
    esp_camera_fb_return(fb);
}
```

---

## 5. PlatformIO Configuration Historique (< 3.0.0)

### platformio.ini (Exemple Ancien)
```ini
[env:esp32-cam-old]
platform = espressif32@2.6.2  ; Version avec esp-face supporté
board = esp32-cam
framework = arduino

lib_deps =
    esp32-camera
    esp-face
    
board_build.f_cpu = 240000000L
board_psram_mode = octal

monitor_speed = 115200
```

### idf_component.yml (Ancien)
```yaml
version: 1.0.0
dependencies:
  esp32-camera: "^2.0.0"
  esp-face: "^1.0.0"
  esp-idf: ">=4.4"
```

---

## 6. Structures de Compilation Historiques

### Commande Build ESP-IDF (Ancienne)
```bash
# Configurer
idf.py set-target esp32
idf.py menuconfig
# Cocher: Component config → ESP32-CAM → Enable Face Detection/Recognition

# Builder
idf.py build
idf.py flash monitor
```

### CMakeLists.txt (Ancien)
```cmake
cmake_minimum_required(VERSION 3.5)

project(esp32_face_recognition)

set(COMPONENTS esp32 esp_camera esp_face)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project_name(esp32_face_recognition)
```

---

## 7. Performance et Caractéristiques Historiques

### MTCNN Face Detection
| Paramètre | Valeur |
|-----------|--------|
| Résolution d'entrée | 320×240 ou 160×120 |
| Temps de détection | 150-200ms (ESP32) |
| Nombres de visages max | Illimité (RAM dépendant) |
| Score de confiance | 0.0 - 1.0 |
| Landmarks | 5 points (yeux, nez, bouche) |

### MobileNet Face Recognition
| Paramètre | Valeur |
|-----------|--------|
| Résolution d'entrée | 112×112 |
| Dimension embedding | 128 |
| Temps d'inférence | 100-150ms (ESP32) |
| Type de distance | Euclidienne ou Cosinus |
| Seuil identification | 0.6 (typique) |
| Modèle | MobileNetV1 |

---

## 8. Migration vers ESP-DL (Moderne)

### Remplacement API
| Ancienne (esp-face) | Nouvelle (ESP-DL) |
|-------------------|------------------|
| `face_detect()` | Modèle YOLO ou personnalisé |
| `face_recognition()` | Modèle MobileNet via ESP-DL |
| `dl_matrix3du_t` | Images natives ou tenseurs |
| `box_array_t` | Résultats génériques de modèle |
| Compilation manuelle | `.espdl` format quantisé |

### Exemple Migration
```cpp
// ANCIEN CODE (esp-face)
// box_array_t *boxes = face_detect(image);

// NOUVEAU CODE (ESP-DL)
#include "esp_dl.hpp"
#include "dl_image.hpp"

// Charger modèle
auto model = Model::load("model.espdl");

// Inférer
auto output = model.run(input_tensor);

// Traiter résultats
for (auto& detection : output.detections) {
    // ...
}
```

---

## 9. Ressources Historiques Archivées

### Pages Web Archivées (Wayback Machine)
- esp-face repository: Sauvegardé entre 2020-2025
- Dernière version: Archive du 2025-10-15
- État: Redirige vers esp-dl

### Versions Support Historiques
- Arduino-ESP32 1.0.x - 1.6.x : esp-face supporté
- Arduino-ESP32 2.0.x - 2.6.x : esp-face optionnel
- Arduino-ESP32 3.0.x+ : esp-face dépréciée

---

## 10. Conclusion pour Développeurs

### Si vous avez du code ancien esp-face:

**Option 1: Migration vers ESP-DL** ✅
- Gain: Maintenance active, support moderne
- Effort: Moyen (refactoring des API)
- Temps: 1-2 jours

**Option 2: Rester sur version ancienne** ⚠️
- Gain: Code existant compatible
- Risque: Plus de support, sécurité
- Recommandation: Non

**Option 3: Utiliser TensorFlow Lite** ✅
- Gain: Modèles customisables
- Effort: Fort (setup complet)
- Temps: 3-5 jours

---

**Documentation générée**: Mai 2026  
**Statut**: Archive historique - esp-face remplacé par ESP-DL

# Factory 4.0: Smart Parking Gate and Security Unit

> Two-ESP32 access-control system for a smart factory parking: vehicle detection, RFID badge check, face recognition on an ESP32-CAM, and real-time cloud supervision.

**Context:** P2M project (Projet de Préparation Métier), SUP'COM, 2025–2026
**Team:** Zoubair Garma, Ines Besbes · **Supervisor:** Maha Cherif

---

## Overview

The project applies the **three-layer Industry 4.0 model** (perception, network, application) to the entrance of a factory parking:

- **Perception:** infrared vehicle detection, PIR motion sensing, MFRC522 RFID reader, ESP32-CAM camera.
- **Network:** both ESP32 boards on Wi-Fi, talking over a small HTTP protocol.
- **Application:** Blynk cloud dashboard (gate state, access count, system health) and Telegram notifications with photos.

A car is let in only when **two factors** match: an authorized RFID badge **and** a recognized face.

## Architecture

```
            ┌──────────── ESP32-WROVER (controller) ────────────┐
 IR sensor ─►  7-state machine                                    │
 PIR       ─►  IDLE → CAR_DETECTED → RFID_SCANNED → REQUESTING_CAM │──► gate
 MFRC522   ─►  → WAITING_CAM → ACCESS_GRANTED / ACCESS_DENIED      │
            │  non-blocking HTTP client          Blynk cloud ◄─────┤
            └───────────────┬─────────────────────────────────────┘
                            │ HTTP GET /authorize (Wi-Fi)
            ┌───────────────▼──────── ESP32-CAM (vision) ─────────┐
            │  capture → face detection → 128-d embedding          │
            │  → match against enrolled employees (NVS)            │──► Telegram (alerts, photos)
            └──────────────────────────────────────────────────────┘
```

**Access sequence**

1. The IR sensor detects a car; the driver has 10 s to badge.
2. The WROVER checks the RFID UID against the authorized list.
3. If the badge is valid, the WROVER calls `/authorize` on the ESP32-CAM without blocking its main loop.
4. The CAM captures the driver, computes a face embedding and compares it with the enrolled employees.
5. `FACE_OK` → the gate opens for 5 s; any mismatch or timeout → access denied, with a cooldown before the next car.

## Hardware

| Component | Role |
|---|---|
| ESP32-WROVER (with PSRAM) | Main controller: sensors, state machine, gate, cloud |
| ESP32-CAM (AI-Thinker, 4 MB PSRAM) | Image capture, face recognition, Telegram |
| MFRC522 RFID reader | Employee badge identification |
| Infrared obstacle sensor | Vehicle presence at the gate |
| PIR sensor | Motion detection around the unit |
| Servo motor | Barrier actuation |

## Software

- **Firmware:** C++ with the Arduino framework on PlatformIO (two projects: `P2Mesp32/` and `esp32_cam/`)
- **Libraries:** MFRC522, ESP32Servo, Blynk, esp32-camera, ArduinoJson, UniversalTelegramBot, Thinger.io
- **Cloud:** Blynk dashboard (state, last UID, access count, health score); Telegram bot for alerts and photos
- **Face recognition:** embeddings of aligned 160×120 faces, up to 10 employees enrolled and stored in NVS flash

## Key engineering points

- **Non-blocking design:** the HTTP request to the camera runs asynchronously, so the main loop never freezes while face recognition runs.
- **Explicit 7-state machine** with a timeout at every stage (badge window, camera response, gate open time, cooldown).
- **Resilience:** Wi-Fi auto-reconnect, camera connectivity self-test at boot, heap and PSRAM monitoring.
- **Memory:** frame buffers in PSRAM and a large-app partition to fit the camera and AI code.

## Repository structure

```
P2Mesp32/     ESP32-WROVER controller (state machine, RFID, IR, PIR, Blynk, camera client)
esp32_cam/    ESP32-CAM firmware (camera, face recognition, HTTP endpoints, Telegram)
*.md          Technical docs: protocol, implementation guide, audit and refactoring notes
```

More detail: [TECHNICAL_OVERVIEW.md](TECHNICAL_OVERVIEW.md), [PROTOCOL_REFERENCE.md](PROTOCOL_REFERENCE.md), [IMPLEMENTATION_GUIDE.md](IMPLEMENTATION_GUIDE.md).

## Getting started

1. In both `P2Mesp32/include/` and `esp32_cam/src/`, copy `secrets.example.h` to `secrets.h` and fill in your Wi-Fi, Blynk and Telegram credentials. `secrets.h` is git-ignored.
2. Flash `esp32_cam/` first and note the IP address printed on the serial monitor (115200 baud).
3. Set that IP in `P2Mesp32/src/camera_comms.cpp`, then flash `P2Mesp32/`.
4. Present a car to the IR sensor, badge, and look at the camera.

## Author

**Zoubair Garma**, SUP'COM. [LinkedIn](https://www.linkedin.com/in/zoubair-garma-96a635337) · [GitHub](https://github.com/ZoubairGarma)

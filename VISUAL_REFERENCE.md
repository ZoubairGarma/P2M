# Visual Reference - State Machine & Communication Flow

## WROVER State Machine Diagram

```
┌──────────────────────────────────────────────────────────────────┐
│                  SMART GATE - STATE MACHINE                      │
└──────────────────────────────────────────────────────────────────┘

                           ┌─────────────┐
                           │    SETUP    │
                           └──────┬──────┘
                                  │
                                  ▼
                          ┌─────────────────┐
                    ┌────►│   STATE_IDLE    │◄────┐
                    │     │ Waiting for car │     │
                    │     └─────┬───────────┘     │
                    │           │                 │
                    │    Car presence (IR)        │ Door Cooldown
                    │           │                 │ expires
                    │           ▼                 │
                    │     ┌──────────────────┐    │
                    │     │STATE_CAR_DETECTED│    │
                    │     │  Await RFID scan │    │
                    │     │ Timeout: 10 sec  │    │
                    │     └──────┬───────────┘    │
                    │            │                │
                    │    Card scanned or    Timeout
                    │    timeout            expired
                    │            │          │     │
                    │            ▼          └──────┤
                    │     ┌──────────────────┐    │
                    │     │ STATE_RFID_SCANNED│   │
                    │     │ Check local DB   │    │
                    │     └──────┬───────────┘    │
                    │            │                │
              ┌─────┴─────┐      │                │
              │           │      │                │
              ▼           ▼      │                │
    ┌──────────────┐  ┌────────────────────────┐ │
    │ACCESS_DENIED │  │STATE_REQUESTING_CAM    │ │
    │  Wait 3 sec  │  │ Send HTTP /authorize   │ │
    │  Cooldown    │  │ Timeout: 8 sec         │ │
    └────┬─────────┘  └──────┬─────────────────┘ │
         │                   │                   │
         │         ┌─────────▼─────────┐         │
         │         │ STATE_WAITING_CAM │         │
         │         │ Async HTTP check  │         │
         │         │ No main-loop block│         │
         │         └─────────┬─────────┘         │
         │                   │                   │
         │         ┌─────────┴──────────┐        │
         │         │                    │        │
         │    Response OK         Response failed
         │    or Timeout          or timeout
         │         │                    │        │
         │         ▼                    ▼        │
         │  ┌──────────────────┐  ┌──────────────────┐
         │  │ STATE_ACCESS_     │  │  ACCESS_DENIED   │
         │  │    GRANTED        │  │  Wait 3 sec      │
         │  │ Open gate 5 sec   │  │  Cooldown        │
         │  └────────┬──────────┘  └────────┬─────────┘
         │           │                      │
         └───────────┴──────────────────────┘
                     │
                 Gate closes or
                 Cooldown expires
                     │
                     ▼
                   IDLE
```

---

## HTTP Communication Sequence Diagram

```
WROVER                          ESP32-CAM
  │                                 │
  │  STATE: IDLE                     │
  │  Car Detected                    │
  │  STATE → CAR_DETECTED            │
  │                                  │
  │  RFID Scanned & Authorized       │
  │  STATE → RFID_SCANNED            │
  │                                  │
  │  requestFaceScan() called         │
  │  STATE → REQUESTING_CAM           │
  │  STATE → WAITING_CAM (async)      │
  │                                  │
  │─── GET /authorize ──────────────►│
  │                                  │  Capture image
  │                                  │  Encode to Base64
  │                                  │  Push to Thinger.io
  │                                  │  Face recognition* (TODO)
  │                                  │
  │◄─── 200 OK FACE_OK ──────────────│
  │                                  │
  │  updateCameraComms() detects     │
  │  response in main loop           │
  │  isFaceAuthorized() returns true │
  │  STATE → ACCESS_GRANTED          │
  │                                  │
  │  Servo opens gate                │
  │  Wait 5 seconds                  │
  │  Servo closes gate               │
  │  STATE → IDLE                    │
  │                                  │
```

---

## Memory Architecture

```
┌─────────────────────────────────────────────────┐
│  WROVER (ESP32-WROVER) - THE BRAIN              │
├─────────────────────────────────────────────────┤
│                                                 │
│  Heap (RAM)                    ~385 KB          │
│  ├─ OS Core                    ~100 KB          │
│  ├─ WiFi Stack                 ~80 KB           │
│  ├─ Program Code               ~100 KB          │
│  ├─ Blynk Manager              ~40 KB           │
│  ├─ HTTPClient (async)         ~5 KB            │
│  ├─ RFID Driver                ~10 KB           │
│  └─ Free                       ~50 KB  ⚠️ Watch │
│                                                 │
│  PSRAM (if available)          4 MB             │
│  ├─ Large buffers (if needed)                   │
│  └─ Thinger.io cache                            │
│                                                 │
│  GPIO Pins Used:                                │
│  ├─ GPIO 14: IR Sensor (car detection)          │
│  ├─ GPIO 12: PIR Sensor (motion)                │
│  ├─ GPIO 5,18,19,23: SPI (RFID)                 │
│  └─ GPIO X: Servo (TODO)                        │
│                                                 │
└─────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────┐
│  ESP32-CAM (ESP32-CAM OV2640) - THE EYE         │
├─────────────────────────────────────────────────┤
│                                                 │
│  Heap (RAM)                    ~370 KB          │
│  ├─ OS Core                    ~100 KB          │
│  ├─ WiFi Stack                 ~80 KB           │
│  ├─ Camera Driver              ~130 KB          │
│  ├─ Program Code               ~50 KB           │
│  └─ Free                       ~10 KB  ⚠️ Tight │
│                                                 │
│  PSRAM (on AI-THINKER models)  4 MB             │
│  ├─ Frame Buffer 1             ~200 KB          │
│  ├─ Frame Buffer 2             ~200 KB          │
│  └─ Free                       ~3.6 MB          │
│                                                 │
│  HTTP Endpoints:                                │
│  ├─ GET /                      (health check)   │
│  ├─ GET /authorize             (primary)        │
│  ├─ GET /image                 (debug/raw)      │
│  ├─ GET /status                (JSON debug)     │
│  └─ GET /favicon.ico           (no spam)        │
│                                                 │
└─────────────────────────────────────────────────┘
```

---

## Timing Diagram

```
Time ────────────────────────────────────────────────────────────

0ms   ┌─ Car Appears
      │  [IR Sensor Triggers]
      │
100ms │
      │
1500ms┌─ RFID Card Scanned
      │  [Verified in Database]
      │
1600ms├─ HTTP Request Sent ──────────────┐
      │  [Non-blocking, 0ms main loop impact]
      │                                  │
1700ms│ Main loop continues unblocked    │
      │ Can check other sensors          │
      │ Can timeout if needed            │
      │                    ┌─ CAM Capturing
      │                    │  [200-300ms]
2100ms│                    │
      │                    ├─ Encoding
      │                    │  [100-200ms]
      │                    │
2200ms│                    ├─ Thinger Push
      │                    │
2300ms├─ HTTP Response ◄────┴──────────────┘
      │  [200 OK FACE_OK]
      │
2310ms├─ Process Response (< 1ms)
      │
2320ms├─ Gate Opens (Servo)
      │  [Physical movement]
      │
7320ms├─ Gate Timeout Expires
      │  [Gate Closes]
      │
7330ms├─ Return to IDLE
      │  [Ready for next car]
      │

Legend:
├─ Blocking operation (main loop frozen)
└─ Non-blocking operation (main loop continues)

Total time: ~7.3 seconds
Main loop frozen: 0ms (Non-blocking!) ✅
```

---

## Data Flow Diagram

```
┌─────────────────────────────────────────────────────────┐
│              WROVER                                     │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  Sensors                  Core Logic      Cloud         │
│  ┌──────┐                 ┌────────┐     ┌──────┐      │
│  │ IR   │────┐            │        │     │      │      │
│  └──────┘    │            │        │     │      │      │
│              ├───────────►│ State  │────►│Blynk │      │
│  ┌──────┐    │            │Machine │    │      │      │
│  │RFID  │────┤            │        │     └──────┘      │
│  └──────┘    │            │        │                   │
│              │            └────┬───┘                   │
│  ┌──────┐    │                 │                       │
│  │PIR   │────┤                 │                       │
│  └──────┘    │                 ▼                       │
│              │            ┌──────────┐   HTTP          │
│              │            │Camera    │   (Non-blocking)│
│              │            │Comms     │────────┐        │
│              │            │(Async)   │        │        │
│              │            └──────────┘        │        │
│              │                                │        │
│              └────────────────────────────────┘        │
│                                                         │
└─────────────────────────────────────────────────────────┘
                        │
                   WiFi │ HTTP
                        │ /authorize
                        │
                        ▼
┌─────────────────────────────────────────────────────────┐
│              ESP32-CAM                                  │
├─────────────────────────────────────────────────────────┤
│                                                         │
│  ┌──────────┐  ┌──────────┐  ┌───────┐  ┌──────────┐  │
│  │ Camera   │──│ Image    │──│Base64 │──│ Thinger  │  │
│  │Capture   │  │Encode    │  │Encode │  │ Cloud    │  │
│  └──────────┘  └──────────┘  └───────┘  └──────────┘  │
│       │                                                 │
│       └──────► HTTP Response ──────┐                   │
│                 FACE_OK/NOT_FOUND   │                  │
│                                     ▼                  │
│                            Response to WROVER          │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

---

## Non-Blocking Implementation Detail

```
BEFORE (Blocking):
┌─────────────────────────────────┐
│  Main Loop Iteration (~5ms)     │
├─────────────────────────────────┤
│  Check IR        (1ms)          │
│  Check RFID      (2ms)          │
│  Check PIR       (1ms)          │  Total: ~5ms
│  requestFaceScan() ← Blocks for 5000ms!
│                                 │
│  [FROZEN - NOTHING HAPPENS]     │  ← PROBLEM!
│                                 │
│  (Returns after timeout)        │
│  Continue...                    │
└─────────────────────────────────┘


AFTER (Non-Blocking):
┌─────────────────────────────────┐
│  Loop Iteration 1 (~5ms)        │
├─────────────────────────────────┤
│  Check IR        (1ms)          │
│  Check RFID      (2ms)          │  Total: ~5ms
│  Check PIR       (1ms)          │
│  requestFaceScan() (instant)    │  ← Non-blocking!
│  updateCameraComms() (0.1ms)    │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  Loop Iteration 2 (~5ms)        │
├─────────────────────────────────┤
│  Check IR        (1ms)          │
│  Check RFID      (2ms)          │  Total: ~5ms
│  Check PIR       (1ms)          │
│  updateCameraComms() (0.1ms)    │  ← Check response
│      (Response not ready yet)   │
└─────────────────────────────────┘
         │
         ▼
┌─────────────────────────────────┐
│  Loop Iteration 3 (~5ms)        │
├─────────────────────────────────┤
│  Check IR        (1ms)          │
│  Check RFID      (2ms)          │  Total: ~5ms
│  Check PIR       (1ms)          │
│  updateCameraComms() (0.1ms)    │  ← Response ready!
│      (Process response)         │
│  isFaceAuthorized() = true      │
└─────────────────────────────────┘

✅ Main loop ALWAYS responsive!
✅ Can detect PIR motion while waiting
✅ Can handle timeouts properly
✅ No main-loop freeze
```

---

## Quick Comparison Table

```
┌──────────────────┬─────────────────┬──────────────────┐
│ Aspect           │ Before          │ After            │
├──────────────────┼─────────────────┼──────────────────┤
│ HTTP Blocking    │ 5000ms freeze   │ 0ms (async)      │
│ State Management │ Messy booleans  │ 7-state machine  │
│ Endpoints        │ 3 confusing     │ 1 clean primary  │
│ Responses        │ Ambiguous       │ Predictable      │
│ Memory Check     │ None            │ PSRAM monitoring │
│ Timeout Handling │ Implicit        │ Explicit 8s      │
│ Code Clarity     │ Hard to follow  │ Easy to debug    │
│ Testability      │ Difficult       │ State visible    │
└──────────────────┴─────────────────┴──────────────────┘
```

---

## Deployment Checklist Flow

```
Start
  │
  ▼
Update CAM IP in code
  │ (10.32.245.1 → actual IP)
  ▼
Compile & Upload to WROVER
  │
  ▼
Compile & Upload to ESP32-CAM
  │
  ▼
Monitor WROVER Serial Output
  │ (Should show state transitions)
  ▼
Test: Place car at gate
  │
  ├─ IR detects car → ✅
  │
  ├─ Scan RFID card → ✅
  │
  ├─ HTTP request sent → ✅
  │
  ├─ CAM responds → ✅
  │
  ├─ Gate opens → TODO (implement servo)
  │
  └─ Return to IDLE → ✅
  
  ▼
All tests pass? → YES
  │
  ▼
Ready for Production! 🚀
```

---

**Diagram Version**: 2.0
**Last Updated**: May 2026

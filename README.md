# pi-top PULSE Voice Assistant & 7x7 RGB LED Matrix Satellite (ESPHome & ESP32-S3)
## Comprehensive Technical Project Summary & Documentation

---

## 1. Project Overview & Features

The goal of this project is to implement a **native, two-way Voice Assistant satellite** with an integrated **$7 \times 7$ RGB LED Matrix visual display** for Home Assistant on modern ESPHome running on an **ESP32-S3** microcontroller paired with the **pi-top PULSE** smart speaker/microphone/LED add-on board.

### Key Capabilities:
- **Continuous On-Device Wake Word Detection**: Local offline keyword spotting (*"Okay Nabu"*, *"Hey Jarvis"*, *"Alexa"*) using TensorFlow Lite Micro (`micro_wake_word`).
- **Two-Way Voice Pipeline**: Seamless natural voice interactions with Home Assistant Assist (Speech-to-Text $\rightarrow$ Intent Handling $\rightarrow$ Text-to-Speech playback).
- **High-Fidelity Audio Streaming**: 4096-byte ring buffer on `Serial2` RX @ 250k baud with priority task processing, preventing packet drops and missed phrases.
- **I2S Audio Output**: High-fidelity 16,000Hz mono speaker playback with dynamic hardware state management via I2C.
- **Notification Chimes**: Native RTTTL playback on Home Assistant connection and Wake Word detection at a pleasant, balanced gain.
- **Voice Response Volume**: User-adjustable volume slider in Home Assistant for spoken TTS and media responses.
- **$7 \times 7$ RGB LED Matrix Visual Display**: Full hardware protocol implementation over `Serial2` TX (GPIO 6 @ 250k baud) with gamma correction, brightness control, and 10 dynamic visual effects.
- **Reactive Voice Animations**:
  - **Connected Animation**: Expanding glowing cyan wave that automatically turns off after 2.0 seconds.
  - **Listening Animation**: Rotating amber/cyan glowing halo orbiting the matrix during wake-word detection.
  - **Speaking Animation**: Dynamic equalizer spectrum bars bouncing while the voice assistant speaks.
- **On-Device Voice Intent Parsing**: Local STT parsing directly on ESP32 in `on_stt_end` with priority-ordered keyword matching for instantaneous matrix effect switching via voice.
- **Decoupled Architecture**: `LED Matrix Power` is master hardware enable (ON by default) while `LED Matrix Effect` defaults to `Off` (dark standby).

---

## 2. Hardware Architecture & Pinout Specifications

| Subsystem | Parameter / Pin | Specification |
| :--- | :--- | :--- |
| **Microcontroller** | Model | ESP32-S3 (Board: `esp32-s3-devkitc-1`) |
| **Flash & Memory** | Flash / PSRAM | 16MB Flash, 80MHz Octal PSRAM |
| **Microphone Source** | UART RX | GPIO 7 (RX) @ **250,000 baud 8N1** (4096-byte buffer) |
| **Microphone Format** | Raw Stream | Unsigned 8-bit PCM (midpoint @ 128) $\rightarrow$ Converted to signed 16-bit PCM @ 16kHz |
| **LED Matrix Output** | UART TX | GPIO 6 (TX) @ **250,000 baud 8N1** (Sync frame + 7 column packets + ambient pixel) |
| **Speaker (I2S Output)**| DOUT / BCK / WS | GPIO 16 (DOUT), GPIO 15 (BCK), GPIO 17 (WS/LRCK) |
| **Speaker Format** | I2S Standard | 16,000Hz, 16-bit Mono, External DAC |
| **I2C Control Bus** | SDA / SCL | GPIO 4 (SDA), GPIO 5 (SCL) @ Address `0x24` |
| **I2C Initializations** | `configure.h` | Speaker Enabled, MCU Enabled, EEPROM Enabled, Mic Rate set to 16kHz |

---

## 3. Configuration Summary

### Firmware Files:
- [`esp_pi-top.yaml`](file:///home/shivela/pi-top/esp_pi-top.yaml): ESPHome configuration containing I2S Audio, Micro Wake Word, Voice Assistant with on-device `on_stt_end` parser, and UI entities.
- [`pitop_matrix.h`](file:///home/shivela/pi-top/pitop_matrix.h): $7 \times 7$ Matrix driver with prioritized effect name matching, hardware delay latching, and voice animations.
- [`serial_voice_mic.h`](file:///home/shivela/pi-top/serial_voice_mic.h): High-speed UART PCM microphone receiver on `Serial2` RX with 4KB buffer.
- [`configure.h`](file:///home/shivela/pi-top/configure.h): I2C register configuration for the pi-top PULSE MCU and DAC.



## Credits and Attribution
* Hardware Profile: [pi-topPULSE](https://github.com)
* Original Python Library License: Apache-2.0
* This project translates hardware initializations from the original 
  pi-top Python implementation into Arduino-compatible C++ files, 
  enabling native serial audio acquisition under ESPHome.

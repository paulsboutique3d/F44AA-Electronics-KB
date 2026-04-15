# F44AA Pulse Rifle Replica

> **⚠️ WORK IN PROGRESS — This project is under active development and has NOT been tested on hardware yet. Audio files are not included. Pin assignments, schematics, and code may change significantly. Use at your own risk.**

ESP32-S3 powered replica of the F44AA Pulse Rifle from Alien: Romulus (2024). Full-auto firing, ammo counter, muzzle flash, and audio effects.

## Features

- **Full-auto firing** at 900 RPM with trigger and magazine safety
- **6-bar ammo counter** (400 rounds) on ST7789 display
- **Muzzle flash LED** (WS2812) synced to firing
- **RGB torch LED** with web-controlled colors
- **Audio effects** via DFPlayer Pro (speaker or line out)
- **Bluetooth audio** via KCX-BT_EMITTER
- **AI person detection** using ESP-DL pedestrian model
- **Target tracking** with lock-on feedback
- **WiFi web interface** for configuration
- **OTA firmware updates**

## Required Hardware

| Component | Qty | Description | Link |
|-----------|-----|-------------|------|
| Freenove ESP32-S3 WROOM CAM | 1 | Main controller with built-in camera | [GitHub](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board) |
| ST7789 Display (170x320) | 2 | Ammo counter (landscape) + Camera feed (portrait) | [Generic 1.9" SPI module](https://www.aliexpress.com/item/1005007395375546.html?spm=a2g0o.order_list.order_list_main.277.1a4c1802Dj4rFP) |
| DFPlayer Pro | 1 | MP3 player with DAC line-out | [DFRobot Wiki](https://wiki.dfrobot.com/DFPlayer_PRO_SKU_DFR0768) |
| KCX-BT_EMITTER | 1 | Bluetooth audio transmitter | [Info](https://github.com/Mark-MDO47/BluetoothAudioTransmitter_KCX_BT_EMITTER) |
| WS2812 LEDs | 2 | Addressable RGB (muzzle + torch) | Generic 5V strips/rings |
| Hall Effect Sensor | 1 | Magazine detection | Generic 3-pin module |

## Hardware

**Board:** Freenove ESP32-S3 WROOM CAM

| Component | GPIO | Notes |
|-----------|------|-------|
| Counter Display (ST7789) | 1, 44, 42, 41, 40, 39 | SPI3, 320x170 landscape |
| Camera Display (ST7789) | 1, 44, 21, 33, 34, 35 | SPI3 shared, 170x320 portrait |
| Muzzle Flash (WS2812) | 2 | |
| Torch LED (WS2812) | 3 | |
| DFPlayer Pro | 47 (TX), 14 (RX) | UART1 |
| Trigger Switch | 0 | Pull-up |
| Magazine Sensor | 38 | Hall effect |
| Bluetooth (KCX-BT) | 45 (TX), 46 (RX), 36 (KEY), 48 (LED) | UART2 |
| Camera (OV2640) | 4-18 | Built-in, RGB565 |

Full pinout: [PINOUT.md](PINOUT.md)




## Web Interface

1. Connect to WiFi: `F44AA` (password: `aliens1986`)
2. Open: http://192.168.4.1
3. Control torch, audio mode, view status, update firmware

## Audio Setup

SD card files for DFPlayer Pro:
- `0001.mp3` - Fire sound
- `0002.mp3` - Reload sound

## Project Structure

```
main/
├── main.c               # App entry, firing logic, task management
├── counter_display.c    # 6-bar ammo counter (ST7789)
├── ws2812.c             # Muzzle flash + torch LEDs
├── dfplayer.c           # Audio playback (UART1)
├── trigger.c            # Trigger input handling
├── magazine.c           # Magazine sensor (Hall effect)
├── camera_esp32s3.c     # OV2640 camera driver (RGB565)
├── person_detection.cpp # ESP-DL pedestrian detection
├── ai_targeting.c       # Target lock/tracking system
├── bluetooth_transmitter.c # KCX-BT_EMITTER (UART2)
├── webserver.c          # WiFi AP + HTTP server
├── ota_update.c         # Firmware updates
├── color_calibration.c  # Display color settings
└── wifi_config.c        # Network configuration
```

## Status

**Working:**
- Firing system (900 RPM full-auto)
- Ammo counter display (6-bar)
- Muzzle flash + torch LEDs
- Audio (speaker & line out)
- Bluetooth audio streaming
- Web interface & OTA updates
- Camera driver (Freenove board)
- AI person detection (ESP-DL)
- Target tracking & lock-on

## AI Targeting

The rifle includes ESP-DL powered person detection:

| Setting | Value |
|---------|-------|
| Model | Pedestrian Detect PICO S8 |
| Resolution | QVGA (320x240) |
| Format | RGB565 |
| Frame rate | ~5 FPS |
| Confidence threshold | 70% |
| Lock requirement | 3 consecutive detections |

**Modes:**
- `MONITOR` - Detection only (default)
- `ASSIST` - Visual/audio feedback
- `AUTO` - Automatic targeting
- `TRAINING` - Debug mode

## Specs

| Spec | Value |
|------|-------|
| MCU | ESP32-S3 @ 240MHz |
| RAM | 512KB + 8MB PSRAM |
| Flash | 8MB (OTA dual partition) |
| Fire rate | 900 RPM |
| Magazine | 400 rounds |
| Trigger response | <10ms |
| AI inference | ~120ms/frame |
| WiFi | 2.4GHz AP mode |

## License

MIT

---

*Built for the Aliens prop replica community*


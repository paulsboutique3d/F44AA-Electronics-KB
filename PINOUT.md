# F44AA Pinout Guide - Freenove ESP32-S3 WROOM CAM

## Board: Freenove ESP32-S3 WROOM CAM

This project uses the **Freenove ESP32-S3 WROOM CAM** board. All pinouts are standardized for this board.

- **MCU:** ESP32-S3 @ 240MHz dual-core
- **Flash:** 8MB
- **PSRAM:** 8MB (required for camera + AI)
- **Camera:** OV2640 (built-in)

---

## Pin Assignments

### Camera (Hardware Fixed - Used for AI Detection)
| GPIO | Function |
|------|----------|
| 15 | XCLK |
| 4 | SIOD (I2C SDA) |
| 5 | SIOC (I2C SCL) |
| 16 | D7 |
| 17 | D6 |
| 18 | D5 |
| 12 | D4 |
| 10 | D3 |
| 8 | D2 |
| 9 | D1 |
| 11 | D0 |
| 6 | VSYNC |
| 7 | HREF |
| 13 | PCLK |

**Reserved by camera:** 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18

Camera runs at QVGA (320x240) RGB565 for AI person detection.

---

### Counter Display (ST7789 1.9" 170x320 - Ammo)
| GPIO | Function |
|------|----------|
| 1 | MOSI (shared) |
| 44 | SCLK (shared) |
| 42 | CS |
| 41 | DC |
| 40 | RST |
| 39 | BLK |

---

### Camera Display (ST7789 1.9" 170x320 - Feed)
| GPIO | Function |
|------|----------|
| 1 | MOSI (shared) |
| 44 | SCLK (shared) |
| 21 | CS |
| 33 | DC |
| 34 | RST |
| 35 | BLK |

Both displays share the SPI3 bus (MOSI/SCLK) with separate control pins.

#### ST7789 Display Module Pinout (typical 8-pin module)
```
ST7789 1.9" TFT Module (both displays)
┌─────────────────────────────────────┐
│         ┌─────────────────┐         │
│         │                 │         │
│         │   170 x 320     │         │
│         │    Display      │         │
│         │                 │         │
│         │                 │         │
│         └─────────────────┘         │
│                                     │
│  GND  VCC  SCL  SDA  RES  DC  CS  BLK
│  [1]  [2]  [3]  [4]  [5]  [6] [7] [8]
└─────────────────────────────────────┘
```

#### Counter Display (U3) Wiring
```
ST7789 Pin    ESP32 GPIO    Notes
──────────────────────────────────────────
GND [1]   →   GND           Ground
VCC [2]   →   3.3V          Power (3.3V only!)
SCL [3]   →   GPIO 44       SPI Clock (shared)
SDA [4]   →   GPIO 1        SPI MOSI (shared)
RES [5]   →   GPIO 40       Reset
DC  [6]   →   GPIO 41       Data/Command
CS  [7]   →   GPIO 42       Chip Select
BLK [8]   →   GPIO 39       Backlight (PWM capable)
```

#### Camera Display (U4) Wiring
```
ST7789 Pin    ESP32 GPIO    Notes
──────────────────────────────────────────
GND [1]   →   GND           Ground
VCC [2]   →   3.3V          Power (3.3V only!)
SCL [3]   →   GPIO 44       SPI Clock (shared)
SDA [4]   →   GPIO 1        SPI MOSI (shared)
RES [5]   →   GPIO 34       Reset
DC  [6]   →   GPIO 33       Data/Command
CS  [7]   →   GPIO 21       Chip Select
BLK [8]   →   GPIO 35       Backlight (PWM capable)
```

---

### LEDs (WS2812)
| GPIO | Function |
|------|----------|
| 2 | Muzzle Flash |
| 3 | Torch |

#### WS2812 LED Wiring
```
WS2812 Addressable LED Strip/Ring
┌───────────────────────────────────────┐
│                                       │
│   ┌───┐ ┌───┐ ┌───┐ ┌───┐ ┌───┐      │
│   │LED│→│LED│→│LED│→│LED│→│LED│→ ... │
│   └───┘ └───┘ └───┘ └───┘ └───┘      │
│     ↑                                 │
│   DIN                                 │
│                                       │
│   VCC  DIN  GND                       │
│   [+]  [D]  [-]                       │
└───────────────────────────────────────┘

Muzzle Flash LEDs:
  VCC [+]  →  5V            Power (5V for full brightness)
  DIN [D]  ←  GPIO 2        Data input
  GND [-]  →  GND           Ground

Torch LEDs:
  VCC [+]  →  5V            Power (5V for full brightness)
  DIN [D]  ←  GPIO 3        Data input
  GND [-]  →  GND           Ground

Note: Add 300-500Ω resistor on DIN line for signal protection
      Add 100-1000µF capacitor across VCC/GND for power stability
```

---

### DFPlayer Pro
| GPIO | Function |
|------|----------|
| 47 | TX (to DFPlayer RX) |
| 14 | RX (from DFPlayer TX) |

---

### Bluetooth (KCX-BT_EMITTER)
| GPIO | Function |
|------|----------|
| 45 | TX (to KCX RX) |
| 46 | RX (from KCX TX) |
| 36 | KEY (power/pair) |
| 48 | LED Status |

---

### Inputs
| GPIO | Function |
|------|----------|
| 0 | Trigger |
| 38 | Magazine Sensor |

---

## Summary Table

| GPIO | Assignment |
|------|------------|
| 0 | Trigger |
| 1 | Display MOSI (shared) |
| 2 | Muzzle LED |
| 3 | Torch LED |
| 4 | Camera SIOD |
| 5 | Camera SIOC |
| 6 | Camera VSYNC |
| 7 | Camera HREF |
| 8 | Camera D2 |
| 9 | Camera D1 |
| 10 | Camera D3 |
| 11 | Camera D0 |
| 12 | Camera D4 |
| 13 | Camera PCLK |
| 14 | DFPlayer RX |
| 15 | Camera XCLK |
| 16 | Camera D7 |
| 17 | Camera D6 |
| 18 | Camera D5 |
| 21 | Camera Display CS |
| 33 | Camera Display DC |
| 34 | Camera Display RST |
| 35 | Camera Display BLK |
| 38 | Magazine Sensor |
| 39 | Counter Display BLK |
| 40 | Counter Display RST |
| 41 | Counter Display DC |
| 42 | Counter Display CS |
| 44 | Display SCLK (shared) |
| 45 | Bluetooth TX |
| 36 | Bluetooth KEY |
| 46 | Bluetooth RX |
| 47 | DFPlayer TX |
| 48 | Bluetooth LED |

---

## Available GPIOs
19, 20, 37, 43

---

## Wiring Notes

### DFPlayer Pro (Full Pinout)
```
DFPlayer Pro Module
┌─────────────────────────────┐
│                             │
│  ┌─────────────────────┐    │
│  │     SD CARD SLOT    │    │
│  └─────────────────────┘    │
│                             │
│  VIN ──[1]         [10]── SPK1 ─────→ Speaker +
│   TX ──[2]         [9]─── SPK2 ─────→ Speaker -
│   RX ──[3]         [8]─── DAC_L ────→ KCX IN_L (pin 6)
│  GND ──[4]         [7]─── DAC_R ────→ KCX IN_R (pin 7)
│                             │
└─────────────────────────────┘

ESP32 Connections:
  GPIO 47 (TX) ───→ DFPlayer RX [3]
  GPIO 14 (RX) ←─── DFPlayer TX [2]
  5V ─────────────→ DFPlayer VIN [1]
  GND ────────────→ DFPlayer GND [4]

Audio Outputs:
  SPK1/SPK2 [10,9] → Internal Speaker (when Audio Mode = SPEAKER)
  DAC_L/DAC_R [8,7] → Line Out to KCX-BT_EMITTER (when Audio Mode = LINE OUT)
```

### KCX-BT_EMITTER (Full Pinout)
```
KCX-BT_EMITTER Module (Top View)
┌─────────────────────────────────────┐
│   Bluetooth Antenna                 │
│   ┌─────────────────────────────┐   │
│   │                             │   │
│   └─────────────────────────────┘   │
│                                     │
│  +5V ──[1]                  [9]── KEY ←─── GPIO 36 (Power/Pair)
│  PGND ─[2]                  [8]── AGND ←── DFPlayer GND
│  LED ──[3] ───→ GPIO 48     [7]── IN_R ←── DFPlayer DAC_R [7]
│  RX ───[4] ←─── GPIO 45     [6]── IN_L ←── DFPlayer DAC_L [8]
│  TX ───[5] ───→ GPIO 46                    
│                                     │
└─────────────────────────────────────┘

ESP32 Connections:
  GPIO 45 (TX) ───→ KCX RX [4]     (AT commands)
  GPIO 46 (RX) ←─── KCX TX [5]     (AT responses)
  GPIO 36 ────────→ KCX KEY [9]    (Power on/off, Pairing mode)
  GPIO 48 ←──────── KCX LED [3]    (Status indicator)
  5V ─────────────→ KCX +5V [1]
  GND ────────────→ KCX PGND [2]

Audio Input (from DFPlayer Pro):
  DFPlayer DAC_L [8] ───→ KCX IN_L [6]
  DFPlayer DAC_R [7] ───→ KCX IN_R [7]
  DFPlayer GND [4] ─────→ KCX AGND [8]    (Audio ground - IMPORTANT!)
```

### Complete Audio Signal Path
```
┌──────────────────────────────────────────────────────────────────────┐
│                                                                      │
│  ┌────────────┐         ┌─────────────┐         ┌─────────────────┐ │
│  │  SD Card   │  Audio  │  DFPlayer   │  Line   │  KCX-BT_EMITTER │ │
│  │  (MP3/WAV) │ ──────→ │    Pro      │ ──────→ │   (Bluetooth)   │ │
│  └────────────┘         └─────────────┘  Out    └─────────────────┘ │
│                               │                         │           │
│                               │ Speaker                 │ Wireless  │
│                               ▼ Output                  ▼           │
│                         ┌───────────┐           ┌───────────────┐   │
│                         │  Speaker  │           │  BT Headphones│   │
│                         │  (8Ω 2W)  │           │   or Speaker  │   │
│                         └───────────┘           └───────────────┘   │
│                                                                      │
└──────────────────────────────────────────────────────────────────────┘

Audio Mode = SPEAKER:   DFPlayer SPK1/SPK2 → Internal Speaker
Audio Mode = LINE OUT:  DFPlayer DAC_L/R → KCX → Bluetooth Device
```

### WS2812 LEDs
```
ESP32 GPIO 2  --> Muzzle WS2812 DIN
ESP32 GPIO 3  --> Torch WS2812 DIN
5V            --> WS2812 VCC
GND           --> WS2812 GND
```

---

## Task Distribution

| Core | Task | Priority |
|------|------|----------|
| Core 0 | Person Detection | 2 |
| Core 0 | WiFi/Network | 5 |
| Core 1 | Trigger/Firing | 4 |

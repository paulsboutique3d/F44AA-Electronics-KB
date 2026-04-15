# F44AA Pulse Rifle - Bill of Materials (BOM)
**EasyEDA Compatible Parts List**

Project: F44AA Pulse Rifle Replica with Dual Display System  
Date: January 14, 2026  
Version: 2.0 (Freenove ESP32-S3 WROOM CAM standardized)

---

## 📋 Complete Component List

### **Main Components**

| Ref | Qty | Component | Description | EasyEDA Part Number | Manufacturer Part# | Package | Price Est. |
|-----|-----|-----------|-------------|-------------------|-------------------|---------|-----------|
| **U1** | 1 | Freenove ESP32-S3 WROOM CAM | ESP32-S3 with OV2640 Camera + 8MB PSRAM | - | Freenove | Module | $15.00 |
| **U2** | 1 | DFPlayer Pro | Audio Module with Dual Output | C558840 | DFR0768 | Module | $12.00 |
| **U3** | 1 | ST7789 Display | 1.9" TFT LCD 170x320 (Ammo Counter) | C502008 | ST7789V | Module | $8.00 |
| **U4** | 1 | ST7789 Display | 1.9" TFT LCD 170x320 (Camera Feed, Portrait) | C502008 | ST7789V | Module | $8.00 |
| **U5** | 1 | KCX-BT_EMITTER | Bluetooth Audio Transmitter Module | C634902 | KCX-BT_EMITTER | Module | $6.00 |
| **LED1** | 1 | WS2812B | Addressable RGB LED (Muzzle) | C114586 | WS2812B | 5050 SMD | $0.50 |
| **LED2** | 1 | WS2812B | Addressable RGB LED (Torch) | C114586 | WS2812B | 5050 SMD | $0.50 |
| **U6** | 1 | Hall Sensor | Digital Hall Effect Sensor | C85019 | A3144 | TO-92 | $1.00 |
| **SW1** | 1 | Microswitch | Omron SS5GL111 Subminiature Microswitch | C5123513 | SS5GL111 | THT | $1.20 |

### **Passive Components**

| Ref | Qty | Component | Value | Description | EasyEDA Part Number | Package | Price Est. |
|-----|-----|-----------|-------|-------------|-------------------|---------|-----------|
| **R1** | 1 | Resistor | 10kΩ | Pull-up for trigger | C25804 | 0805 | $0.05 |
| **R2** | 1 | Resistor | 470Ω | WS2812 Data Protection | C25117 | 0805 | $0.05 |
| **R3** | 1 | Resistor | 470Ω | WS2812 Data Protection | C25117 | 0805 | $0.05 |
| **R4** | 1 | Resistor | 1kΩ | General purpose | C17513 | 0805 | $0.05 |
| **C1** | 1 | Capacitor | 100µF | Power Supply Decoupling | C16133 | 1206 | $0.15 |
| **C2** | 2 | Capacitor | 0.1µF | High Freq Decoupling | C14663 | 0805 | $0.10 |
| **C3** | 2 | Capacitor | 10µF | Local Decoupling | C15850 | 0805 | $0.20 |

### **Connectors & Headers**

| Ref | Qty | Component | Description | EasyEDA Part Number | Package | Price Est. |
|-----|-----|-----------|-------------|-------------------|---------|-----------|
| **J4** | 1 | Display Header | 8-pin 2.54mm Header (Primary Display) | C124375 | THT | $0.30 |
| **J7** | 1 | Display Header | 8-pin 2.54mm Header (Secondary Display) | C124375 | THT | $0.30 |
| **J8** | 1 | Bluetooth Header | 9-pin 2.54mm Header (KCX-BT_EMITTER) | C124377 | THT | $0.40 |
| **J5** | 2 | LED Connector | 3-pin JST Connector | C160404 | THT | $0.60 |
| **J6** | 1 | Sensor Connector | 3-pin 2.54mm Header | C124374 | THT | $0.20 |

### **Power Management**

| Ref | Qty | Component | Description | EasyEDA Part Number | Package | Price Est. |
|-----|-----|-----------|-------------|-------------------|---------|-----------|
| **U7** | 1 | Voltage Regulator | 3.3V LDO Regulator | C15849 | SOT-223 | $0.50 |
| **L1** | 1 | Inductor | 10µH Power Inductor | C85017 | 1210 | $0.30 |
| **D1** | 1 | Schottky Diode | Reverse Protection | C8598 | SMA | $0.20 |

### **Optional Components (Enhancement)**

| Ref | Qty | Component | Description | EasyEDA Part Number | Package | Price Est. |
|-----|-----|-----------|-------------|-------------------|---------|-----------|
| **SP1** | 1 | Speaker | 8Ω 2W Speaker | C192989 | THT | $3.00 |
| **F1** | 1 | Fuse | 1A Resettable Fuse | C15036 | 1206 | $0.30 |
| **LED3** | 1 | Status LED | Power Indicator | C72043 | 0805 | $0.05 |

---

## 🔌 Connector Pinouts

### **Primary Display Connector (J4) - 8 Pin - Ammo Counter**
```
Pin 1: GND     (Black)
Pin 2: VCC     (Red - 3.3V)
Pin 3: SCLK    (Yellow - GPIO 44 - Shared SPI)
Pin 4: MOSI    (Green - GPIO 1 - Shared SPI)
Pin 5: CS      (Blue - GPIO 42)
Pin 6: DC      (Purple - GPIO 41)
Pin 7: RST     (Gray - GPIO 40)
Pin 8: BLK     (White - GPIO 39)
```

### **Secondary Display Connector (J7) - 8 Pin - Camera Feed**
```
Pin 1: GND     (Black)
Pin 2: VCC     (Red - 3.3V)
Pin 3: SCLK    (Yellow - GPIO 44 - Shared SPI)
Pin 4: MOSI    (Green - GPIO 1 - Shared SPI)
Pin 5: CS      (Blue - GPIO 21)
Pin 6: DC      (Purple - GPIO 33)
Pin 7: RST     (Gray - GPIO 34)
Pin 8: BLK     (White - GPIO 35)
```

### **Bluetooth Transmitter Connector (J8) - 9 Pin - KCX-BT_EMITTER**
```
Pin 1: VCC     (Red - 5V)
Pin 2: PGND    (Black - Power Ground)  
Pin 3: LED     (Yellow - Status LED - GPIO 48 Input)
Pin 4: RX      (Blue - GPIO 45 - ESP TX to KCX RX)
Pin 5: TX      (Green - GPIO 46 - KCX TX to ESP RX)
Pin 6: IN_L    (White - Line Input Left from DFPlayer DAC_L)
Pin 7: IN_R    (Gray - Line Input Right from DFPlayer DAC_R)
Pin 8: AGND    (Brown - Audio Ground from DFPlayer GND)
Pin 9: KEY     (Orange - GPIO 36 - Power/Pair Control)
```

### **LED Connectors (J5) - 3 Pin Each**
```
Pin 1: VCC     (Red - 5V)
Pin 2: GND     (Black)
Pin 3: DATA    (White - GPIO 2/3)
```

### **Sensor Connector (J6) - 3 Pin**
```
Pin 1: VCC     (Red - 3.3V)
Pin 2: GND     (Black)
Pin 3: OUT     (White - GPIO 38 - Magazine Sensor)
```

### **Trigger Connector - 2 Pin**
```
Pin 1: GND     (Black)
Pin 2: TRIG    (White - GPIO 0 - Internal Pull-up)
```

---

## 📷 Freenove ESP32-S3 WROOM CAM Pinout

### **Camera Interface (Hardware Fixed - Reserved)**
```
GPIO 15 - XCLK (Camera Clock)
GPIO 4  - SIOD (I2C SDA)
GPIO 5  - SIOC (I2C SCL)
GPIO 16 - D7 (Camera Data 7)
GPIO 17 - D6 (Camera Data 6)
GPIO 18 - D5 (Camera Data 5)
GPIO 12 - D4 (Camera Data 4)
GPIO 10 - D3 (Camera Data 3)
GPIO 8  - D2 (Camera Data 2)
GPIO 9  - D1 (Camera Data 1)
GPIO 11 - D0 (Camera Data 0)
GPIO 6  - VSYNC (Vertical Sync)
GPIO 7  - HREF (Horizontal Reference)
GPIO 13 - PCLK (Pixel Clock)
```

**Reserved by camera:** 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 15, 16, 17, 18

### **F44AA Pin Assignments**
```
Component               GPIO Pin    Function
Trigger Input           GPIO 0      Microswitch (Pull-up)
Shared SPI MOSI         GPIO 1      SPI Data (Both Displays)
Muzzle Flash LED        GPIO 2      WS2812 Data
Torch LED               GPIO 3      WS2812 Data
DFPlayer RX             GPIO 14     UART1 RX (from DFPlayer TX)
Camera Display CS       GPIO 21     SPI Chip Select
Camera Display DC       GPIO 33     Data/Command
Camera Display RST      GPIO 34     Reset
Camera Display BLK      GPIO 35     Backlight
Bluetooth KEY           GPIO 36     Power/Pair Control
Magazine Sensor         GPIO 38     Hall Effect Input
Counter Display BLK     GPIO 39     Backlight
Counter Display RST     GPIO 40     Reset
Counter Display DC      GPIO 41     Data/Command
Counter Display CS      GPIO 42     SPI Chip Select
Shared SPI SCLK         GPIO 44     SPI Clock (Both Displays)
Bluetooth TX            GPIO 45     UART2 TX (to KCX RX)
Bluetooth RX            GPIO 46     UART2 RX (from KCX TX)
DFPlayer TX             GPIO 47     UART1 TX (to DFPlayer RX)
Bluetooth LED           GPIO 48     Status LED Input
```

### **Available GPIOs**
```
GPIO 19, 20, 37, 43
```

---

## � Cost Summary

### **Main Components Cost**
- ESP32-S3-CAM Module: $12.00
- DFPlayer Pro Audio: $12.00  
- Primary ST7789 Display: $8.00
- Secondary ST7789 Display: $8.00
- KCX-BT_EMITTER Bluetooth: $6.00
- 2x WS2812B LEDs: $1.00
- Hall Sensor + Microswitch: $2.20
- **Subtotal**: $49.20

### **Passive Components & Connectors**
- Resistors, Capacitors, Headers: $2.50
- Power Management (Regulator, etc): $1.00
- **Subtotal**: $3.50

### **Optional Components**
- Speaker, Fuse, Status LED: $3.35

### **Total Estimated Cost**
- **Core System**: $52.70
- **With Optional Components**: $56.05

*Prices are estimates based on EasyEDA/LCSC pricing as of August 2025*

---

## 🖥️ Dual Display System Architecture

### **Display Configuration**
The F44AA uses a dual display system with shared SPI3 bus architecture:

- **Primary Display (U3)**: 1.9" 170x320 ST7789 IPS - Ammo counter (Landscape: 320x170)
- **Secondary Display (U4)**: 1.9" 170x320 ST7789 IPS - Camera feed (Portrait: 170x320)

### **Shared SPI Bus Benefits**
- **MOSI/SCLK Shared**: GPIO 1/44 used by both displays
- **Independent Control**: Each display has unique CS/DC/RST/BLK pins
- **Efficient Resource Usage**: Saves 2 GPIO pins compared to separate buses
- **High Performance**: 80MHz SPI clock for smooth display updates

### **Display Functions**
```
Primary Display (Counter) - 320x170 Landscape:
- 6-bar ammo counter (400 rounds)
- Color-coded status (red bars)
- Low ammo warning

Secondary Display (Camera) - 170x320 Portrait:
- Live camera feed (scaled to 170x320)
- AI targeting overlay
- Person detection bounding box
- Target lock indicator
```

### **Bluetooth Audio System**
The KCX-BT_EMITTER provides wireless audio transmission capabilities:

- **Audio Source**: DFPlayer Pro DAC_L/DAC_R → KCX-BT_EMITTER IN_L/IN_R
- **Control Interface**: GPIO KEY pin for power/pair + UART AT commands for status
- **Power Management**: 5V power supply, GPIO 36 KEY pin for on/off control
- **Range**: Up to 10m Bluetooth transmission
- **Compatibility**: A2DP profile for high-quality audio stereo transmission

**Connection Workflow:**
1. Select "LINE OUT" in web interface audio settings
2. GPIO 36 pulses KEY pin - short press powers on module
3. GPIO 36 pulses KEY pin - long press enters pairing mode
4. Module scans for Bluetooth receivers and connects
5. Audio from DFPlayer Pro DAC outputs transmitted wirelessly
6. Select "SPEAKER" to power off Bluetooth and use internal speaker

**AT Command Examples:**
- `AT+` - Test communication
- `AT+RESET` - Reset module  
- `AT+PAIR` - Scan for Bluetooth devices
- `AT+STATUS?` - Check connection status
- `AT+VMLINK?` - Display stored auto-connect devices

---

## 📚 Additional Resources

### **Datasheets & Documentation**
- Freenove ESP32-S3: [GitHub](https://github.com/Freenove/Freenove_ESP32_S3_WROOM_Board)
- ESP32-S3: [Espressif Documentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32s3/)
- DFPlayer Pro: [DFRobot Wiki](https://wiki.dfrobot.com/DFPlayer_PRO_SKU_DFR0768)
- KCX-BT_EMITTER: [GitHub Info](https://github.com/Mark-MDO47/BluetoothAudioTransmitter_KCX_BT_EMITTER)
- ST7789: [Adafruit Guide](https://learn.adafruit.com/adafruit-1-3-color-tft-bonnet-for-raspberry-pi)
- WS2812B: [Datasheet](https://cdn-shop.adafruit.com/datasheets/WS2812B.pdf)

---

**Last Updated**: January 14, 2026  
**Revision**: v2.0 (Freenove ESP32-S3 WROOM CAM standardized)

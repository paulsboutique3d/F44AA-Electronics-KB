/**
 * @file pins_config.h
 * @brief Centralized GPIO Pin Definitions for F44AA Pulse Rifle
 *
 * All hardware pin assignments in one place to avoid duplication
 * and ensure consistency across modules.
 *
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 *
 * Hardware: Freenove ESP32-S3 WROOM CAM
 */

#pragma once

#include "driver/gpio.h"

// ── Trigger ──
#define PIN_TRIGGER         GPIO_NUM_0

// ── Magazine Hall Sensor ──
#define PIN_MAGAZINE        GPIO_NUM_38

// ── Shared SPI Bus (SPI3) ──
#define PIN_SPI_MOSI        GPIO_NUM_1
#define PIN_SPI_SCLK        GPIO_NUM_44

// ── Counter Display (Primary ST7789 – landscape 320x170) ──
#define PIN_COUNTER_CS      GPIO_NUM_42
#define PIN_COUNTER_DC      GPIO_NUM_41
#define PIN_COUNTER_RST     GPIO_NUM_40
#define PIN_COUNTER_BLK     GPIO_NUM_39

// ── Camera Display (Secondary ST7789 – portrait 170x320) ──
#define PIN_CAM_DISP_CS     GPIO_NUM_21
#define PIN_CAM_DISP_DC     GPIO_NUM_33
#define PIN_CAM_DISP_RST    GPIO_NUM_34
#define PIN_CAM_DISP_BLK    GPIO_NUM_35

// ── WS2812 LEDs (RMT) ──
#define PIN_WS2812_MUZZLE   GPIO_NUM_2
#define PIN_WS2812_TORCH    GPIO_NUM_3

// ── DFPlayer Pro (UART1) ──
#define PIN_DFPLAYER_TX     GPIO_NUM_47
#define PIN_DFPLAYER_RX     GPIO_NUM_14

// ── Bluetooth Transmitter KCX-BT (UART2) ──
#define PIN_BT_TX           GPIO_NUM_45
#define PIN_BT_RX           GPIO_NUM_46
#define PIN_BT_KEY          GPIO_NUM_36
#define PIN_BT_LED          GPIO_NUM_48

/**
 * @file counter_display.c
 * @brief F44AA Ammo Counter Display Module
 * 
 * Features:
 * - 400 round maximum capacity (movie accurate)
 * - 6 red bars representing ammo levels (~66.7 rounds per bar)
 * - Low ammo warning at 50 rounds or less
 * - Empty magazine detection and warning
 * - Reload functionality to reset to full capacity
 * - F44AA authentic red-only color scheme
 * 
 * Bar Display Logic:
 * - 6 bars = 336-400 rounds (84-100% capacity)
 * - 5 bars = 269-335 rounds (67-84% capacity)
 * - 4 bars = 202-268 rounds (51-67% capacity)
 * - 3 bars = 135-201 rounds (34-50% capacity)
 * - 2 bars = 68-134 rounds (17-34% capacity)
 * - 1 bar  = 1-67 rounds (0-17% capacity)
 * - 0 bars = 0 rounds (empty - reload required)
 * 
 * @author PaulsBoutique3D
 * @date January 2026
 * @version 2.0
 * 
 * Hardware: ST7789 1.9" 170x320 IPS (320x170 landscape)
 * GPIO: CS=42, DC=41, RST=40, BLK=39, MOSI=1, SCLK=44 (shared)
 */

#include "counter_display.h"
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

// Primary display pins (ammo counter) - Avoiding camera pins and PSRAM conflicts
#define DISPLAY_MOSI     1    // SPI Data (GPIO 1) - Safe pin, no conflicts
#define DISPLAY_SCLK     44   // SPI Clock (GPIO 44) - Safe pin, no conflicts  
#define DISPLAY_CS       42   // Chip Select (GPIO 42) - Safe pin
#define DISPLAY_DC       41   // Data/Command (GPIO 41) - Safe pin
#define DISPLAY_RST      40   // Reset (GPIO 40) - Safe pin
#define DISPLAY_BLK      39   // Backlight/Power (GPIO 39) - Safe pin

// Display configuration (1.9" ST7789 320x170 landscape orientation)
#define PANEL_WIDTH 320
#define PANEL_HEIGHT 170
#define MAX_AMMO 400        // Total ammunition capacity
#define BAR_COUNT 6         // 6 visual bars

// ST7789 Commands
#define ST7789_SWRESET     0x01
#define ST7789_SLPOUT      0x11
#define ST7789_COLMOD      0x3A
#define ST7789_MADCTL      0x36
#define ST7789_CASET       0x2A
#define ST7789_RASET       0x2B
#define ST7789_RAMWR       0x2C
#define ST7789_INVOFF      0x20
#define ST7789_INVON       0x21
#define ST7789_DISPON      0x29

#define ST7789_NORON      0x13

// Colors (RGB565) - Display uses custom color mapping for this panel
#define COLOR_BLACK   0xFFFF  // "black" for your inverted panel
#define COLOR_WHITE   0x0000  // "white" for your inverted panel  
#define COLOR_RED     0xF800  // Standard red
#define COLOR_GREEN   0x07E0  // Standard green
#define COLOR_BLUE    0x001F  // Standard blue
#define COLOR_YELLOW  0xFFE0  // Standard yellow
#define COLOR_CYAN    0x07FF  // Standard cyan
#define COLOR_MAGENTA 0xF81F  // Standard magenta


static const char *TAG = "COUNTER_DISPLAY";
static int current_ammo = MAX_AMMO;  // Current ammo count (0-400)
static counter_display_mode_t current_mode = BAR_F44AA;
static spi_device_handle_t display_spi;
static uint16_t current_bar_color = 0xF800;  // Default bar color: true RED (RGB565)

// Performance optimization - track display state
static bool display_needs_full_redraw = true;
static int last_drawn_bars = -1;

// Send command to display
void display_send_cmd(uint8_t cmd) {
    esp_err_t ret;
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = 8;
    t.tx_buffer = &cmd;
    gpio_set_level(DISPLAY_DC, 0);  // Command mode
    ret = spi_device_transmit(display_spi, &t);
    assert(ret == ESP_OK);
}

// Send data to display
void display_send_data(uint8_t *data, int len) {
    esp_err_t ret;
    spi_transaction_t t;
    if (len == 0) return;
    memset(&t, 0, sizeof(t));
    t.length = len * 8;
    t.tx_buffer = data;
    gpio_set_level(DISPLAY_DC, 1);  // Data mode
    ret = spi_device_transmit(display_spi, &t);
    assert(ret == ESP_OK);
}

// Initialize ST7789 display (RGB565, landscape, non-inverted)
void display_init_st7789(void) {
    // Hard reset
    gpio_set_level(DISPLAY_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    gpio_set_level(DISPLAY_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    // Software reset
    display_send_cmd(ST7789_SWRESET);
    vTaskDelay(pdMS_TO_TICKS(150));

    // Sleep out
    display_send_cmd(ST7789_SLPOUT);
    vTaskDelay(pdMS_TO_TICKS(120));

    // Interface pixel format: 16-bit (RGB565)
    display_send_cmd(ST7789_COLMOD);
    {
        uint8_t color_mode = 0x55; // 16-bit/pixel
        display_send_data(&color_mode, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // Memory data access control (MADCTL)
    // Bits: MY(0x80) MX(0x40) MV(0x20) ML(0x10) BGR(0x08) MH(0x04)
    display_send_cmd(ST7789_MADCTL);
    {
        uint8_t madctl = 0xE0; // MY | MX | MV, BGR=0
        display_send_data(&madctl, 1);
    }
    vTaskDelay(pdMS_TO_TICKS(10));

    // Normal display mode on
    display_send_cmd(ST7789_NORON);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Inversion off
    display_send_cmd(ST7789_INVOFF);
    vTaskDelay(pdMS_TO_TICKS(10));

    // Display on
    display_send_cmd(ST7789_DISPON);
    vTaskDelay(pdMS_TO_TICKS(100));

    ESP_LOGI(TAG, "ST7789 init complete: RGB565, landscape, inversion OFF");
}


// Set display window
void display_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    // Column address set
    display_send_cmd(ST7789_CASET);
    uint8_t col_data[4] = {x0 >> 8, x0 & 0xFF, x1 >> 8, x1 & 0xFF};
    display_send_data(col_data, 4);
    
    // Row address set
    display_send_cmd(ST7789_RASET);
    uint8_t row_data[4] = {y0 >> 8, y0 & 0xFF, y1 >> 8, y1 & 0xFF};
    display_send_data(row_data, 4);
    
    // Memory write
    display_send_cmd(ST7789_RAMWR);
}

// Fill display with color - Highly optimized version using large DMA transfers
void display_fill(uint16_t color) {
    display_set_window(0, 0, PANEL_WIDTH - 1, PANEL_HEIGHT - 1);
    
    // Use larger buffer for much more efficient DMA transfers
    static uint16_t *color_buffer = NULL;
    const size_t buffer_lines = 40; // 40 lines at once (25.6KB buffer)
    const size_t buffer_size = PANEL_WIDTH * buffer_lines;
    
    // Allocate DMA-capable buffer once
    if (color_buffer == NULL) {
        color_buffer = heap_caps_malloc(buffer_size * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (color_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer for display_fill");
            return;
        }
    }
    
    // Fill buffer with color
    for (int i = 0; i < buffer_size; i++) {
        color_buffer[i] = color;
    }
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.length = buffer_size * 16; // buffer_size pixels * 16 bits
    t.tx_buffer = color_buffer;
    gpio_set_level(DISPLAY_DC, 1);  // Data mode
    
    // Send in large chunks - much faster than line-by-line
    int lines_remaining = PANEL_HEIGHT;
    while (lines_remaining > 0) {
        int lines_this_chunk = (lines_remaining >= buffer_lines) ? buffer_lines : lines_remaining;
        t.length = PANEL_WIDTH * lines_this_chunk * 16;
        
        esp_err_t ret = spi_device_transmit(display_spi, &t);
        assert(ret == ESP_OK);
        
        lines_remaining -= lines_this_chunk;
    }
}

// Draw rectangle - Highly optimized version with large buffer transfers
void display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
    if (x + w > PANEL_WIDTH) w = PANEL_WIDTH - x;
    if (y + h > PANEL_HEIGHT) h = PANEL_HEIGHT - y;
    
    display_set_window(x, y, x + w - 1, y + h - 1);
    
    // Use much larger buffer for better performance
    static uint16_t *rect_buffer = NULL;
    const size_t max_buffer_size = 8192; // 16KB buffer (8K pixels)
    
    // Allocate DMA-capable buffer once
    if (rect_buffer == NULL) {
        rect_buffer = heap_caps_malloc(max_buffer_size * sizeof(uint16_t), MALLOC_CAP_DMA);
        if (rect_buffer == NULL) {
            ESP_LOGE(TAG, "Failed to allocate DMA buffer for display_draw_rect");
            return;
        }
    }
    
    // Calculate optimal chunk size
    size_t pixels_per_line = w;
    size_t lines_per_chunk = max_buffer_size / pixels_per_line;
    if (lines_per_chunk == 0) lines_per_chunk = 1;
    if (lines_per_chunk > h) lines_per_chunk = h;
    
    size_t pixels_per_chunk = pixels_per_line * lines_per_chunk;
    
    // Fill buffer with color
    for (int i = 0; i < pixels_per_chunk; i++) {
        rect_buffer[i] = color;
    }
    
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.tx_buffer = rect_buffer;
    gpio_set_level(DISPLAY_DC, 1);  // Data mode
    
    // Send in optimized chunks
    int lines_remaining = h;
    while (lines_remaining > 0) {
        int lines_this_chunk = (lines_remaining >= lines_per_chunk) ? lines_per_chunk : lines_remaining;
        size_t pixels_this_chunk = pixels_per_line * lines_this_chunk;
        
        t.length = pixels_this_chunk * 16; // pixels * 16 bits
        
        esp_err_t ret = spi_device_transmit(display_spi, &t);
        assert(ret == ESP_OK);
        
        lines_remaining -= lines_this_chunk;
    }
}

// Draw rounded rectangle - optimized with minimal delays
void display_draw_rounded_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t radius, uint16_t color) {
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
    if (x + w > PANEL_WIDTH) w = PANEL_WIDTH - x;
    if (y + h > PANEL_HEIGHT) h = PANEL_HEIGHT - y;
    
    // Clamp radius to prevent overlapping
    if (radius > w / 2) radius = w / 2;
    if (radius > h / 2) radius = h / 2;
    
    // Draw the main body (center rectangle) - most efficient single call
    display_draw_rect(x + radius, y + radius, w - 2 * radius, h - 2 * radius, color);
    
    // Draw the straight edges as single rectangles
    display_draw_rect(x + radius, y, w - 2 * radius, radius, color);          // Top edge
    display_draw_rect(x + radius, y + h - radius, w - 2 * radius, radius, color); // Bottom edge
    display_draw_rect(x, y + radius, radius, h - 2 * radius, color);          // Left edge
    display_draw_rect(x + w - radius, y + radius, radius, h - 2 * radius, color); // Right edge
    
    // Draw rounded corners using optimized batch approach
    static uint16_t *corner_buffer = NULL;
    if (corner_buffer == NULL) {
        corner_buffer = heap_caps_malloc(radius * 4 * sizeof(uint16_t), MALLOC_CAP_DMA);
    }
    
    if (corner_buffer != NULL) {
        // Pre-fill corner buffer with color
        for (int i = 0; i < radius * 4; i++) {
            corner_buffer[i] = color;
        }
        
        // Draw corners efficiently
        for (int i = 0; i < radius; i++) {
            int y_offset = radius - i;
            int circle_x = 0;
            
            // Calculate circle boundary using integer math
            for (int test_x = 0; test_x <= radius; test_x++) {
                if (test_x * test_x + y_offset * y_offset <= radius * radius) {
                    circle_x = test_x;
                } else {
                    break;
                }
            }
            
            if (circle_x > 0) {
                int scan_width = circle_x + 1;
                
                // Draw all corner scanlines at once
                display_draw_rect(x + radius - circle_x, y + i, scan_width, 1, color);     // Top-left
                display_draw_rect(x + w - radius, y + i, scan_width, 1, color);            // Top-right
                display_draw_rect(x + radius - circle_x, y + h - 1 - i, scan_width, 1, color); // Bottom-left
                display_draw_rect(x + w - radius, y + h - 1 - i, scan_width, 1, color);        // Bottom-right
            }
        }
    }
}

// Draw bar with half-circle top and bottom (perfect for ammo bars)
void display_draw_half_circle_bar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color) {
    if (x >= PANEL_WIDTH || y >= PANEL_HEIGHT) return;
    if (x + w > PANEL_WIDTH) w = PANEL_WIDTH - x;
    if (y + h > PANEL_HEIGHT) h = PANEL_HEIGHT - y;
    
    // Calculate radius as half the width for perfect half circles
    uint16_t radius = w / 2;
    
    // Ensure we have enough height for both half circles
    if (h < 2 * radius) radius = h / 2;
    
    // Draw the rectangular body (between the two half circles)
    if (h > 2 * radius) {
        display_draw_rect(x, y + radius, w, h - 2 * radius, color);
    }
    
    // Draw the top half circle
    int center_x = x + w / 2;
    int top_center_y = y + radius;
    
    for (int scan_y = y; scan_y < y + radius; scan_y++) {
        // Calculate half-width of circle at this y position
        int dy = top_center_y - scan_y;
        int half_width = 0;
        
        // Find the width of the half circle at this y using integer math
        for (int test_x = 0; test_x <= radius; test_x++) {
            if (test_x * test_x + dy * dy <= radius * radius) {
                half_width = test_x;
            } else {
                break;
            }
        }
        
        if (half_width > 0) {
            // Draw the full width scanline for the top half circle
            int start_x = center_x - half_width;
            int end_x = center_x + half_width;
            int scan_width = end_x - start_x + 1;
            
            // Clamp to bar bounds
            if (start_x < x) {
                scan_width -= (x - start_x);
                start_x = x;
            }
            if (start_x + scan_width > x + w) {
                scan_width = x + w - start_x;
            }
            
            if (scan_width > 0) {
                display_draw_rect(start_x, scan_y, scan_width, 1, color);
            }
        }
    }
    
    // Draw the bottom half circle
    int bottom_center_y = y + h - radius;
    
    for (int scan_y = bottom_center_y; scan_y < y + h; scan_y++) {
        // Calculate half-width of circle at this y position
        int dy = scan_y - bottom_center_y;
        int half_width = 0;
        
        // Find the width of the half circle at this y using integer math
        for (int test_x = 0; test_x <= radius; test_x++) {
            if (test_x * test_x + dy * dy <= radius * radius) {
                half_width = test_x;
            } else {
                break;
            }
        }
        
        if (half_width > 0) {
            // Draw the full width scanline for the bottom half circle
            int start_x = center_x - half_width;
            int end_x = center_x + half_width;
            int scan_width = end_x - start_x + 1;
            
            // Clamp to bar bounds
            if (start_x < x) {
                scan_width -= (x - start_x);
                start_x = x;
            }
            if (start_x + scan_width > x + w) {
                scan_width = x + w - start_x;
            }
            
            if (scan_width > 0) {
                display_draw_rect(start_x, scan_y, scan_width, 1, color);
            }
        }
    }
}

// Calculate how many bars should be visible based on current ammo
static int calculate_visible_bars(void) {
    if (current_ammo <= 0) return 0;
    if (current_ammo >= 336) return 6;  // Match CLI/webserver threshold
    if (current_ammo >= 269) return 5;  // Match CLI/webserver threshold
    if (current_ammo >= 202) return 4;  // Match CLI/webserver threshold
    if (current_ammo >= 135) return 3;  // Match CLI/webserver threshold
    if (current_ammo >= 68) return 2;   // Match CLI/webserver threshold
    return 1;                           // 1-67 rounds
}

// Add SPI initialization for primary display
esp_err_t display_spi_init(void) {
    // Configure SPI bus for primary display
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = DISPLAY_MOSI,
        .sclk_io_num = DISPLAY_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = PANEL_WIDTH * PANEL_HEIGHT * 2,  // Full screen buffer size
        .flags = SPICOMMON_BUSFLAG_MASTER,
    };
    
    ESP_ERROR_CHECK(spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO));
    
    // Configure SPI device for primary display
    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 80*1000*1000,     // Increased from 40MHz to 80MHz for faster transfers
        .mode = 0,
        .spics_io_num = DISPLAY_CS,
        .queue_size = 8,                     // Increased queue size for better DMA performance
        .flags = SPI_DEVICE_HALFDUPLEX,      // Half-duplex mode for better performance
    };
    
    ESP_ERROR_CHECK(spi_bus_add_device(SPI3_HOST, &devcfg, &display_spi));
    
    // Initialize control pins (DC, RST, and Backlight)
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DISPLAY_DC) | (1ULL << DISPLAY_RST) | (1ULL << DISPLAY_BLK),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);
    
    // Enable backlight
    gpio_set_level(DISPLAY_BLK, 1);
    
    return ESP_OK;
}

esp_err_t counter_display_init(void) {
    ESP_LOGI(TAG, "Counter Display initialized - F44AA style (400 rounds, 6 bars)");
    
    // Initialize SPI for primary display
    esp_err_t ret = display_spi_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display SPI: %s", esp_err_to_name(ret));
        return ret;
    }
    
    // Initialize ST7789 display
    display_init_st7789();
    
    // Clear display with black background
    display_fill(COLOR_BLACK);
    
    current_ammo = MAX_AMMO;
    counter_display_draw_bars();
    
    return ESP_OK;
}

void counter_display_set_mode(counter_display_mode_t mode) {
    current_mode = mode;
    ESP_LOGI(TAG, "Counter Display mode set to %d", mode);
    counter_display_draw_bars();
}

void counter_display_draw_bars(void) {
    static int last_visible_bars = -1;  // Track previous bar count
    int visible_bars = calculate_visible_bars();
    
    // Only log every 10 rounds to reduce spam during full auto
    static int last_logged_ammo = -1;  // Initialize to -1 to force first log
    if (current_ammo % 10 == 0 || current_ammo != last_logged_ammo || current_ammo == 0) {
        ESP_LOGI(TAG, "Drawing %d RED bars (Ammo: %d/400):", visible_bars, current_ammo);
        last_logged_ammo = current_ammo;
    }
    
    // Performance optimization - avoid unnecessary redraws
    bool needs_full_redraw = (display_needs_full_redraw || 
                             last_visible_bars == -1 || 
                             visible_bars > last_visible_bars);
    
    if (needs_full_redraw) {
        // Full redraw needed (first time, color change, or ammo increased)
        ESP_LOGI(TAG, "Full display redraw starting...");
        
        display_fill(COLOR_BLACK);
        
        // Draw title bar (top strip in landscape) - single efficient call
        display_draw_rect(0, 0, PANEL_WIDTH, 30, current_bar_color);
        
        // Skip drawing ammo text area to save time - just leave black
        
        // Draw all visible bars in optimized manner
        int bar_width = 40;
        int bar_height = 120;
        int bar_spacing = 16;
        int start_x = (PANEL_WIDTH - (BAR_COUNT * bar_width + (BAR_COUNT - 1) * bar_spacing)) / 2;
        int start_y = 50;
        
        // Draw all bars at once for better performance
        for (int bar = 0; bar < visible_bars; bar++) {
            int x = start_x + bar * (bar_width + bar_spacing);
            int y = start_y;
            display_draw_half_circle_bar(x, y, bar_width, bar_height, current_bar_color);
        }
        
        display_needs_full_redraw = false;
        ESP_LOGI(TAG, "Full display redraw completed - %d bars drawn", visible_bars);
        
    } else if (visible_bars < last_visible_bars) {
        // Partial redraw - erase some bars (ammo decreased) - much faster
        ESP_LOGI(TAG, "Erasing %d bars (partial redraw)", last_visible_bars - visible_bars);
        
        int bar_width = 40;
        int bar_height = 120;
        int bar_spacing = 16;
        int start_x = (PANEL_WIDTH - (BAR_COUNT * bar_width + (BAR_COUNT - 1) * bar_spacing)) / 2;
        int start_y = 50;
        
        // Only erase the bars that need to disappear
        for (int bar = visible_bars; bar < last_visible_bars; bar++) {
            int x = start_x + bar * (bar_width + bar_spacing);
            int y = start_y;
            display_draw_half_circle_bar(x, y, bar_width, bar_height, COLOR_BLACK);
        }
    }
    // If visible_bars == last_visible_bars, do nothing - performance optimization
    
    last_visible_bars = visible_bars;
    last_drawn_bars = visible_bars;
    
    // ASCII art representation for logging (only every 10 rounds)
    if (current_ammo % 10 == 0 || current_ammo == 0 || current_ammo == MAX_AMMO) {
        char bar_display[8][32];
        
        // Clear the display buffer
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 32; col++) {
                bar_display[row][col] = ' ';
            }
        }
        
        // Draw bars (each bar is 4 characters wide, 6 characters tall)
        for (int bar = 0; bar < 6; bar++) {
            int start_col = bar * 6;  // 4 chars + 2 spaces between bars for better separation
            
            if (bar < visible_bars) {
                // Draw filled RED bar using ASCII characters
                for (int row = 1; row < 7; row++) {
                    for (int col = start_col; col < start_col + 4; col++) {
                        if (col < 30) {
                            bar_display[row][col] = '#';  // Solid block for RED bar
                        }
                    }
                }
            } else {
                // Empty bars remain completely black (no ASCII representation)
                // Leave the bar area as spaces
            }
        }
        
        ESP_LOGI(TAG, "+------------------------------+");
        for (int row = 0; row < 8; row++) {
            bar_display[row][30] = '\0';  // Null terminate
            ESP_LOGI(TAG, "|%s|", bar_display[row]);
        }
        ESP_LOGI(TAG, "+------------------------------+");
        ESP_LOGI(TAG, "AMMO: %d/400 rounds (%d bars)", current_ammo, visible_bars);
    }
}

void counter_display_decrement(void) {
    if (current_ammo <= 0) {
        ESP_LOGW(TAG, "AMMO EMPTY! Reload needed!");
        return;
    }
    
    current_ammo--;  // Decrease by 1 round
    int visible_bars = calculate_visible_bars();
    
    // Update display more frequently for better visual feedback, but not every shot for performance
    bool should_update_display = false;
    
    if (current_ammo == 0) {
        ESP_LOGW(TAG, "*** MAGAZINE EMPTY - RELOAD REQUIRED ***");
        should_update_display = true;
    } else if (current_ammo <= 50 && current_ammo % 5 == 0) {
        // Update every 5 rounds when low on ammo
        ESP_LOGW(TAG, "*** LOW AMMO WARNING - %d rounds left ***", current_ammo);
        should_update_display = true;
    } else if (current_ammo % 10 == 0) {
        // Update every 10 rounds during normal firing for responsive visual feedback
        ESP_LOGI(TAG, "Ammo: %d/400 (%d bars)", current_ammo, visible_bars);
        should_update_display = true;
    } else if (current_ammo % 67 == 0) {
        // Bar change notification (approximately every 67 rounds)
        ESP_LOGI(TAG, "Bar decreased! Ammo: %d/400 (%d bars)", current_ammo, visible_bars);
        should_update_display = true;
    }
    
    if (should_update_display) {
        counter_display_draw_bars();
    }
}

int counter_display_get_ammo_count(void) {
    return current_ammo;
}

bool counter_display_is_empty(void) {
    return current_ammo == 0;
}

int counter_display_get_ammo_percentage(void) {
    return (current_ammo * 100) / MAX_AMMO;
}

bool counter_display_can_reload(void) {
    return current_ammo == 0;  // Can only reload when completely empty
}

void counter_display_reset_ammo(void) {
    if (!counter_display_can_reload()) {
        ESP_LOGW(TAG, "RELOAD DENIED - Magazine not empty (%d rounds left)", current_ammo);
        return;
    }
    
    current_ammo = MAX_AMMO;
    ESP_LOGI(TAG, "RELOADED! Ammo reset to %d rounds", current_ammo);
    counter_display_draw_bars();
}

uint16_t counter_color_from_level(int level) {
    // F44AA authentic red-only color scheme
    if (level <= 0) return 0x0000;      // Black (empty)
    if (level <= 2) return 0x8000;      // Dark red (low ammo)
    return 0xF800;                      // Full red (normal ammo)
}

// Function to set the bar color dynamically - optimized
void counter_display_set_bar_color(uint16_t color) {
    current_bar_color = color;
    ESP_LOGI(TAG, "Bar color updated to 0x%04X", color);
    
    // Mark that we need a full redraw for color change
    display_needs_full_redraw = true;
    
    // Force redraw with new color
    counter_display_draw_bars();
    
    ESP_LOGI(TAG, "Color change applied successfully - display updated");
}

// Force reset ammo for testing purposes (bypasses empty check) - optimized
void counter_display_force_reset_ammo(void) {
    current_ammo = MAX_AMMO;
    display_needs_full_redraw = true;  // Force full redraw for reload
    ESP_LOGI(TAG, "FORCE RELOADED! Ammo reset to %d rounds (testing mode)", current_ammo);
    counter_display_draw_bars();
}

// Set ammo to 0 (for magazine removal or boot initialization)
void counter_display_set_empty(void) {
    current_ammo = 0;
    ESP_LOGI(TAG, "Ammo set to EMPTY (0 rounds) - No magazine detected");
    counter_display_draw_bars();
}

// Sync display with current ammo count (ensures accuracy after firing stops)
void counter_display_sync(void) {
    int visible_bars = calculate_visible_bars();
    ESP_LOGI(TAG, "Syncing display - Ammo: %d/400 (%d bars)", current_ammo, visible_bars);
    counter_display_draw_bars();
}
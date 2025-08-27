/**
 * @file pin_display.h
 * @brief Pin Display System Header
 */

#pragma once

#include "esp_err.h"
#include "fpc_a005.h"

#ifdef __cplusplus
extern "C" {
#endif

// Pin color definitions (mapped to FPC-A005 colors)
typedef enum {
    PIN_COLOR_BLACK = FPC_A005_COLOR_BLACK,
    PIN_COLOR_WHITE = FPC_A005_COLOR_WHITE,
    PIN_COLOR_RED = FPC_A005_COLOR_RED,
    PIN_COLOR_YELLOW = FPC_A005_COLOR_YELLOW,
    PIN_COLOR_BLUE = FPC_A005_COLOR_BLUE,
    PIN_COLOR_GREEN = FPC_A005_COLOR_GREEN,
    PIN_COLOR_ORANGE = FPC_A005_COLOR_ORANGE,
} pin_color_t;

// Pin refresh modes
typedef enum {
    PIN_REFRESH_FULL = FPC_A005_REFRESH_FULL,
    PIN_REFRESH_PARTIAL = FPC_A005_REFRESH_PARTIAL,
    PIN_REFRESH_FAST = FPC_A005_REFRESH_FAST,
} pin_refresh_mode_t;

// Font sizes
typedef enum {
    PIN_FONT_SMALL = 12,
    PIN_FONT_MEDIUM = 16,
    PIN_FONT_LARGE = 24,
    PIN_FONT_XLARGE = 32
} pin_font_size_t;

// Widget region definition is now in pin_plugin.h

// Display configuration
typedef struct {
    uint32_t fast_refresh_interval;      // Fast refresh interval (seconds)
    uint32_t partial_refresh_interval;   // Partial refresh interval (seconds)
    uint32_t full_refresh_interval;      // Full refresh interval (seconds)
    uint32_t sleep_after_inactive;       // Sleep after inactive time (seconds)
    uint8_t max_partial_refresh;         // Max partial refreshes before full
    bool auto_refresh_enabled;           // Auto refresh enabled
    bool power_save_enabled;             // Power save mode enabled
} pin_display_config_t;

/**
 * @brief Initialize Pin display system
 * @return ESP_OK on success
 */
esp_err_t pin_display_init(void);

/**
 * @brief Deinitialize Pin display system
 * @return ESP_OK on success
 */
esp_err_t pin_display_deinit(void);

/**
 * @brief Clear the display with specified color
 * @param color Fill color
 * @return ESP_OK on success
 */
esp_err_t pin_display_clear(pin_color_t color);

/**
 * @brief Set pixel at coordinates
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
 * @return ESP_OK on success
 */
esp_err_t pin_display_set_pixel(uint16_t x, uint16_t y, pin_color_t color);

/**
 * @brief Draw line
 * @param x0 Start X
 * @param y0 Start Y
 * @param x1 End X
 * @param y1 End Y
 * @param color Line color
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, pin_color_t color);

/**
 * @brief Draw rectangle
 * @param x X coordinate
 * @param y Y coordinate
 * @param w Width
 * @param h Height
 * @param color Rectangle color
 * @param filled Fill rectangle
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, pin_color_t color, bool filled);

/**
 * @brief Draw circle
 * @param x Center X
 * @param y Center Y
 * @param r Radius
 * @param color Circle color
 * @param filled Fill circle
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_circle(uint16_t x, uint16_t y, uint16_t r, pin_color_t color, bool filled);

/**
 * @brief Draw text
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text to draw
 * @param font_size Font size
 * @param color Text color
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_text(uint16_t x, uint16_t y, const char* text, pin_font_size_t font_size, pin_color_t color);

/**
 * @brief Draw WiFi icon
 * @param x X coordinate
 * @param y Y coordinate
 * @param rssi WiFi signal strength
 * @param color Icon color
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_wifi_icon(uint16_t x, uint16_t y, int8_t rssi, pin_color_t color);

/**
 * @brief Draw battery icon
 * @param x X coordinate
 * @param y Y coordinate
 * @param percentage Battery percentage
 * @param color Icon color
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_battery_icon(uint16_t x, uint16_t y, uint8_t percentage, pin_color_t color);

/**
 * @brief Draw loading animation
 * @param x Center X coordinate
 * @param y Center Y coordinate
 * @param size Animation size
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_loading_animation(uint16_t x, uint16_t y, uint8_t size);

/**
 * @brief Draw QR code
 * @param x X coordinate
 * @param y Y coordinate
 * @param text Text to encode
 * @param size QR code size
 * @return ESP_OK on success
 */
esp_err_t pin_display_draw_qr_code(uint16_t x, uint16_t y, const char* text, uint8_t size);

/**
 * @brief Refresh the display
 * @param mode Refresh mode
 * @return ESP_OK on success
 */
esp_err_t pin_display_refresh(pin_refresh_mode_t mode);

/**
 * @brief Enter sleep mode
 * @return ESP_OK on success
 */
esp_err_t pin_display_sleep(void);

/**
 * @brief Wake from sleep mode
 * @return ESP_OK on success
 */
esp_err_t pin_display_wake(void);

/**
 * @brief Get battery voltage
 * @return Battery voltage in volts
 */
float pin_battery_get_voltage(void);

/**
 * @brief Get battery percentage
 * @param voltage Battery voltage
 * @return Battery percentage (0-100)
 */
uint8_t pin_battery_get_percentage(float voltage);

/**
 * @brief Check if system should enter sleep
 * @return true if should sleep
 */
bool pin_should_enter_sleep(void);

/**
 * @brief Enter deep sleep mode
 */
void pin_enter_deep_sleep(void);

/**
 * @brief Get display handle
 * @return Display handle
 */
fpc_a005_handle_t pin_display_get_handle(void);

#ifdef __cplusplus
}
#endif
/**
 * @file fpc_a005.h
 * @brief FPC-A005 2.9inch 7-color e-ink display driver
 */

#pragma once

#include "esp_err.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"

#ifdef __cplusplus
extern "C" {
#endif

// Display specifications
#define FPC_A005_WIDTH          600
#define FPC_A005_HEIGHT         448
#define FPC_A005_BUFFER_SIZE    ((FPC_A005_WIDTH * FPC_A005_HEIGHT) / 2)  // 4 bits per pixel

// Color definitions
typedef enum {
    FPC_A005_COLOR_BLACK = 0x00,
    FPC_A005_COLOR_WHITE = 0x01,
    FPC_A005_COLOR_RED = 0x02,
    FPC_A005_COLOR_YELLOW = 0x03,
    FPC_A005_COLOR_BLUE = 0x04,
    FPC_A005_COLOR_GREEN = 0x05,
    FPC_A005_COLOR_ORANGE = 0x06,
} fpc_a005_color_t;

// Refresh modes
typedef enum {
    FPC_A005_REFRESH_FULL,     // Full refresh with clear
    FPC_A005_REFRESH_PARTIAL,  // Partial refresh
    FPC_A005_REFRESH_FAST      // Fast refresh (local update)
} fpc_a005_refresh_mode_t;

// Configuration structure
typedef struct {
    spi_host_device_t spi_host;
    int sck_io;
    int mosi_io;
    int cs_io;
    int dc_io;
    int rst_io;
    int busy_io;
    int spi_clock_speed_hz;
} fpc_a005_config_t;

// Device handle
typedef struct fpc_a005_dev_s* fpc_a005_handle_t;

/**
 * @brief Initialize FPC-A005 display
 * @param config Configuration structure
 * @param handle Pointer to device handle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_init(const fpc_a005_config_t *config, fpc_a005_handle_t *handle);

/**
 * @brief Deinitialize FPC-A005 display
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_deinit(fpc_a005_handle_t handle);

/**
 * @brief Clear the display with specified color
 * @param handle Device handle
 * @param color Fill color
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_clear(fpc_a005_handle_t handle, fpc_a005_color_t color);

/**
 * @brief Set pixel color at specified coordinates
 * @param handle Device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pixel color
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_set_pixel(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t color);

/**
 * @brief Get pixel color at specified coordinates
 * @param handle Device handle
 * @param x X coordinate
 * @param y Y coordinate
 * @param color Pointer to store pixel color
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_get_pixel(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t *color);

/**
 * @brief Draw line from (x0, y0) to (x1, y1)
 * @param handle Device handle
 * @param x0 Start X coordinate
 * @param y0 Start Y coordinate
 * @param x1 End X coordinate
 * @param y1 End Y coordinate
 * @param color Line color
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_draw_line(fpc_a005_handle_t handle, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, fpc_a005_color_t color);

/**
 * @brief Draw rectangle
 * @param handle Device handle
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Rectangle width
 * @param h Rectangle height
 * @param color Rectangle color
 * @param filled Whether to fill the rectangle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_draw_rect(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, fpc_a005_color_t color, bool filled);

/**
 * @brief Draw circle
 * @param handle Device handle
 * @param x Center X coordinate
 * @param y Center Y coordinate
 * @param r Circle radius
 * @param color Circle color
 * @param filled Whether to fill the circle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_draw_circle(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t r, fpc_a005_color_t color, bool filled);

/**
 * @brief Draw bitmap
 * @param handle Device handle
 * @param x Top-left X coordinate
 * @param y Top-left Y coordinate
 * @param w Bitmap width
 * @param h Bitmap height
 * @param bitmap Bitmap data
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_draw_bitmap(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap);

/**
 * @brief Refresh the display
 * @param handle Device handle
 * @param mode Refresh mode
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_refresh(fpc_a005_handle_t handle, fpc_a005_refresh_mode_t mode);

/**
 * @brief Enter sleep mode
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_sleep(fpc_a005_handle_t handle);

/**
 * @brief Wake up from sleep mode
 * @param handle Device handle
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_wake(fpc_a005_handle_t handle);

/**
 * @brief Check if display is busy
 * @param handle Device handle
 * @param is_busy Pointer to store busy status
 * @return ESP_OK on success
 */
esp_err_t fpc_a005_is_busy(fpc_a005_handle_t handle, bool *is_busy);

/**
 * @brief Wait until display is not busy
 * @param handle Device handle
 * @param timeout_ms Timeout in milliseconds (0 for infinite)
 * @return ESP_OK on success, ESP_ERR_TIMEOUT on timeout
 */
esp_err_t fpc_a005_wait_ready(fpc_a005_handle_t handle, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
/**
 * @file fpc_a005.c
 * @brief FPC-A005 2.9inch 7-color e-ink display driver implementation
 */

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "fpc_a005.h"

static const char *TAG = "FPC_A005";

// Command definitions
#define FPC_A005_CMD_PANEL_SETTING              0x00
#define FPC_A005_CMD_POWER_SETTING              0x01
#define FPC_A005_CMD_POWER_OFF                  0x02
#define FPC_A005_CMD_POWER_OFF_SEQUENCE         0x03
#define FPC_A005_CMD_POWER_ON                   0x04
#define FPC_A005_CMD_POWER_ON_MEASURE           0x05
#define FPC_A005_CMD_BOOSTER_SOFT_START         0x06
#define FPC_A005_CMD_DEEP_SLEEP                 0x07
#define FPC_A005_CMD_DATA_START_TRANSMISSION_1  0x10
#define FPC_A005_CMD_DATA_STOP                  0x11
#define FPC_A005_CMD_DISPLAY_REFRESH            0x12
#define FPC_A005_CMD_IMAGE_PROCESS              0x13
#define FPC_A005_CMD_LUT_FOR_VCOM               0x20
#define FPC_A005_CMD_LUT_BLUE                   0x21
#define FPC_A005_CMD_LUT_WHITE                  0x22
#define FPC_A005_CMD_LUT_GRAY_1                 0x23
#define FPC_A005_CMD_LUT_GRAY_2                 0x24
#define FPC_A005_CMD_LUT_RED_0                  0x25
#define FPC_A005_CMD_LUT_RED_1                  0x26
#define FPC_A005_CMD_LUT_RED_2                  0x27
#define FPC_A005_CMD_LUT_RED_3                  0x28
#define FPC_A005_CMD_LUT_XON                    0x29
#define FPC_A005_CMD_PLL_CONTROL                0x30
#define FPC_A005_CMD_TEMPERATURE_CALIBRATION    0x40
#define FPC_A005_CMD_TEMPERATURE_SELECTION      0x41
#define FPC_A005_CMD_VCOM_DATA_INTERVAL         0x50
#define FPC_A005_CMD_LOW_POWER_DETECTION        0x51
#define FPC_A005_CMD_TCON_SETTING               0x60
#define FPC_A005_CMD_TCON_RESOLUTION            0x61
#define FPC_A005_CMD_SPI_FLASH_CONTROL          0x65
#define FPC_A005_CMD_REVISION                   0x70
#define FPC_A005_CMD_GET_STATUS                 0x71
#define FPC_A005_CMD_AUTO_MEASUREMENT_VCOM      0x80
#define FPC_A005_CMD_READ_VCOM                  0x81
#define FPC_A005_CMD_VCM_DC_SETTING             0x82
#define FPC_A005_CMD_PARTIAL_WINDOW             0x90
#define FPC_A005_CMD_PARTIAL_IN                 0x91
#define FPC_A005_CMD_PARTIAL_OUT                0x92
#define FPC_A005_CMD_PROGRAM_MODE               0xA0
#define FPC_A005_CMD_ACTIVE_PROGRAMMING         0xA1
#define FPC_A005_CMD_READ_OTP                   0xA2
#define FPC_A005_CMD_POWER_SAVING               0xE3

// Device structure
struct fpc_a005_dev_s {
    spi_device_handle_t spi_handle;
    fpc_a005_config_t config;
    uint8_t *framebuffer;
    bool is_initialized;
    bool is_sleeping;
};

// Helper functions
static esp_err_t fpc_a005_write_cmd(fpc_a005_handle_t handle, uint8_t cmd);
static esp_err_t fpc_a005_write_data(fpc_a005_handle_t handle, const uint8_t *data, size_t len);
static esp_err_t fpc_a005_reset(fpc_a005_handle_t handle);
static void fpc_a005_set_pixel_in_buffer(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t color);
static fpc_a005_color_t fpc_a005_get_pixel_from_buffer(fpc_a005_handle_t handle, uint16_t x, uint16_t y);

esp_err_t fpc_a005_init(const fpc_a005_config_t *config, fpc_a005_handle_t *handle) {
    if (!config || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Initializing FPC-A005 display");

    // Allocate device structure
    struct fpc_a005_dev_s *dev = calloc(1, sizeof(struct fpc_a005_dev_s));
    if (!dev) {
        ESP_LOGE(TAG, "Failed to allocate device structure");
        return ESP_ERR_NO_MEM;
    }

    // Copy configuration
    memcpy(&dev->config, config, sizeof(fpc_a005_config_t));

    // Allocate framebuffer
    dev->framebuffer = heap_caps_malloc(FPC_A005_BUFFER_SIZE, MALLOC_CAP_DMA);
    if (!dev->framebuffer) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        free(dev);
        return ESP_ERR_NO_MEM;
    }

    // Initialize GPIO pins
    gpio_config_t gpio_conf = {
        .pin_bit_mask = (1ULL << config->dc_io) | (1ULL << config->rst_io),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    // Configure BUSY pin as input
    gpio_conf.pin_bit_mask = (1ULL << config->busy_io);
    gpio_conf.mode = GPIO_MODE_INPUT;
    gpio_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ESP_ERROR_CHECK(gpio_config(&gpio_conf));

    // Initialize SPI
    spi_device_interface_config_t dev_cfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .mode = 0,
        .duty_cycle_pos = 0,
        .cs_ena_pretrans = 0,
        .cs_ena_posttrans = 0,
        .clock_speed_hz = config->spi_clock_speed_hz,
        .spics_io_num = config->cs_io,
        .flags = 0,
        .queue_size = 1,
        .pre_cb = NULL,
        .post_cb = NULL,
    };

    esp_err_t ret = spi_bus_add_device(config->spi_host, &dev_cfg, &dev->spi_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to add SPI device: %s", esp_err_to_name(ret));
        free(dev->framebuffer);
        free(dev);
        return ret;
    }

    // Reset the display
    ret = fpc_a005_reset(dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reset display: %s", esp_err_to_name(ret));
        spi_bus_remove_device(dev->spi_handle);
        free(dev->framebuffer);
        free(dev);
        return ret;
    }

    // Initialize display settings
    ret = fpc_a005_write_cmd(dev, FPC_A005_CMD_POWER_SETTING);
    if (ret == ESP_OK) ret = fpc_a005_write_data(dev, (uint8_t[]){0x07, 0x07, 0x3f, 0x3f}, 4);
    
    if (ret == ESP_OK) ret = fpc_a005_write_cmd(dev, FPC_A005_CMD_POWER_ON);
    if (ret == ESP_OK) ret = fpc_a005_wait_ready(dev, 5000);

    if (ret == ESP_OK) ret = fpc_a005_write_cmd(dev, FPC_A005_CMD_PANEL_SETTING);
    if (ret == ESP_OK) ret = fpc_a005_write_data(dev, (uint8_t[]){0x1F}, 1);

    if (ret == ESP_OK) ret = fpc_a005_write_cmd(dev, FPC_A005_CMD_TCON_RESOLUTION);
    if (ret == ESP_OK) ret = fpc_a005_write_data(dev, (uint8_t[]){
        (FPC_A005_WIDTH >> 8) & 0xFF, FPC_A005_WIDTH & 0xFF,
        (FPC_A005_HEIGHT >> 8) & 0xFF, FPC_A005_HEIGHT & 0xFF
    }, 4);

    if (ret == ESP_OK) ret = fpc_a005_write_cmd(dev, FPC_A005_CMD_VCM_DC_SETTING);
    if (ret == ESP_OK) ret = fpc_a005_write_data(dev, (uint8_t[]){0x0E}, 1);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display settings: %s", esp_err_to_name(ret));
        spi_bus_remove_device(dev->spi_handle);
        free(dev->framebuffer);
        free(dev);
        return ret;
    }

    // Clear framebuffer
    memset(dev->framebuffer, 0x11, FPC_A005_BUFFER_SIZE); // Fill with white

    dev->is_initialized = true;
    dev->is_sleeping = false;
    *handle = dev;

    ESP_LOGI(TAG, "FPC-A005 display initialized successfully");
    return ESP_OK;
}

esp_err_t fpc_a005_deinit(fpc_a005_handle_t handle) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Deinitializing FPC-A005 display");

    // Enter sleep mode
    fpc_a005_sleep(handle);

    // Remove SPI device
    spi_bus_remove_device(handle->spi_handle);

    // Free memory
    free(handle->framebuffer);
    free(handle);

    ESP_LOGI(TAG, "FPC-A005 display deinitialized");
    return ESP_OK;
}

static esp_err_t fpc_a005_write_cmd(fpc_a005_handle_t handle, uint8_t cmd) {
    gpio_set_level(handle->config.dc_io, 0); // Command mode
    
    spi_transaction_t trans = {
        .length = 8,
        .tx_buffer = &cmd,
    };
    
    return spi_device_transmit(handle->spi_handle, &trans);
}

static esp_err_t fpc_a005_write_data(fpc_a005_handle_t handle, const uint8_t *data, size_t len) {
    if (len == 0) return ESP_OK;
    
    gpio_set_level(handle->config.dc_io, 1); // Data mode
    
    spi_transaction_t trans = {
        .length = len * 8,
        .tx_buffer = data,
    };
    
    return spi_device_transmit(handle->spi_handle, &trans);
}

static esp_err_t fpc_a005_reset(fpc_a005_handle_t handle) {
    ESP_LOGI(TAG, "Resetting display");
    
    gpio_set_level(handle->config.rst_io, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(handle->config.rst_io, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    
    return fpc_a005_wait_ready(handle, 5000);
}

esp_err_t fpc_a005_is_busy(fpc_a005_handle_t handle, bool *is_busy) {
    if (!handle || !is_busy) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *is_busy = (gpio_get_level(handle->config.busy_io) == 1);
    return ESP_OK;
}

esp_err_t fpc_a005_wait_ready(fpc_a005_handle_t handle, uint32_t timeout_ms) {
    if (!handle) {
        return ESP_ERR_INVALID_ARG;
    }
    
    uint32_t start_time = xTaskGetTickCount();
    bool is_busy = true;
    
    while (is_busy) {
        esp_err_t ret = fpc_a005_is_busy(handle, &is_busy);
        if (ret != ESP_OK) {
            return ret;
        }
        
        if (timeout_ms > 0) {
            uint32_t elapsed = (xTaskGetTickCount() - start_time) * portTICK_PERIOD_MS;
            if (elapsed >= timeout_ms) {
                ESP_LOGW(TAG, "Wait ready timeout after %d ms", elapsed);
                return ESP_ERR_TIMEOUT;
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    return ESP_OK;
}

esp_err_t fpc_a005_clear(fpc_a005_handle_t handle, fpc_a005_color_t color) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Clearing display with color %d", color);
    
    // Fill framebuffer with specified color
    uint8_t pixel_data = (color << 4) | color;
    memset(handle->framebuffer, pixel_data, FPC_A005_BUFFER_SIZE);
    
    return ESP_OK;
}

static void fpc_a005_set_pixel_in_buffer(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t color) {
    if (x >= FPC_A005_WIDTH || y >= FPC_A005_HEIGHT) {
        return;
    }
    
    uint32_t pixel_index = y * FPC_A005_WIDTH + x;
    uint32_t byte_index = pixel_index / 2;
    bool is_high_nibble = (pixel_index % 2) == 0;
    
    if (is_high_nibble) {
        handle->framebuffer[byte_index] = (handle->framebuffer[byte_index] & 0x0F) | (color << 4);
    } else {
        handle->framebuffer[byte_index] = (handle->framebuffer[byte_index] & 0xF0) | (color & 0x0F);
    }
}

static fpc_a005_color_t fpc_a005_get_pixel_from_buffer(fpc_a005_handle_t handle, uint16_t x, uint16_t y) {
    if (x >= FPC_A005_WIDTH || y >= FPC_A005_HEIGHT) {
        return FPC_A005_COLOR_WHITE;
    }
    
    uint32_t pixel_index = y * FPC_A005_WIDTH + x;
    uint32_t byte_index = pixel_index / 2;
    bool is_high_nibble = (pixel_index % 2) == 0;
    
    if (is_high_nibble) {
        return (handle->framebuffer[byte_index] >> 4) & 0x0F;
    } else {
        return handle->framebuffer[byte_index] & 0x0F;
    }
}

esp_err_t fpc_a005_set_pixel(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t color) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    fpc_a005_set_pixel_in_buffer(handle, x, y, color);
    return ESP_OK;
}

esp_err_t fpc_a005_get_pixel(fpc_a005_handle_t handle, uint16_t x, uint16_t y, fpc_a005_color_t *color) {
    if (!handle || !handle->is_initialized || !color) {
        return ESP_ERR_INVALID_ARG;
    }
    
    *color = fpc_a005_get_pixel_from_buffer(handle, x, y);
    return ESP_OK;
}

esp_err_t fpc_a005_draw_line(fpc_a005_handle_t handle, uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, fpc_a005_color_t color) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Bresenham's line algorithm
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = x0 < x1 ? 1 : -1;
    int sy = y0 < y1 ? 1 : -1;
    int err = dx - dy;
    
    int x = x0, y = y0;
    
    while (true) {
        fpc_a005_set_pixel_in_buffer(handle, x, y, color);
        
        if (x == x1 && y == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x += sx;
        }
        if (e2 < dx) {
            err += dx;
            y += sy;
        }
    }
    
    return ESP_OK;
}

esp_err_t fpc_a005_draw_rect(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, fpc_a005_color_t color, bool filled) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (filled) {
        for (uint16_t py = y; py < y + h && py < FPC_A005_HEIGHT; py++) {
            for (uint16_t px = x; px < x + w && px < FPC_A005_WIDTH; px++) {
                fpc_a005_set_pixel_in_buffer(handle, px, py, color);
            }
        }
    } else {
        // Draw outline
        fpc_a005_draw_line(handle, x, y, x + w - 1, y, color);           // Top
        fpc_a005_draw_line(handle, x, y, x, y + h - 1, color);           // Left
        fpc_a005_draw_line(handle, x + w - 1, y, x + w - 1, y + h - 1, color); // Right
        fpc_a005_draw_line(handle, x, y + h - 1, x + w - 1, y + h - 1, color); // Bottom
    }
    
    return ESP_OK;
}

esp_err_t fpc_a005_draw_circle(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t r, fpc_a005_color_t color, bool filled) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Bresenham's circle algorithm
    int cx = 0;
    int cy = r;
    int d = 3 - 2 * r;
    
    while (cy >= cx) {
        if (filled) {
            fpc_a005_draw_line(handle, x - cx, y - cy, x + cx, y - cy, color);
            fpc_a005_draw_line(handle, x - cx, y + cy, x + cx, y + cy, color);
            fpc_a005_draw_line(handle, x - cy, y - cx, x + cy, y - cx, color);
            fpc_a005_draw_line(handle, x - cy, y + cx, x + cy, y + cx, color);
        } else {
            fpc_a005_set_pixel_in_buffer(handle, x + cx, y + cy, color);
            fpc_a005_set_pixel_in_buffer(handle, x - cx, y + cy, color);
            fpc_a005_set_pixel_in_buffer(handle, x + cx, y - cy, color);
            fpc_a005_set_pixel_in_buffer(handle, x - cx, y - cy, color);
            fpc_a005_set_pixel_in_buffer(handle, x + cy, y + cx, color);
            fpc_a005_set_pixel_in_buffer(handle, x - cy, y + cx, color);
            fpc_a005_set_pixel_in_buffer(handle, x + cy, y - cx, color);
            fpc_a005_set_pixel_in_buffer(handle, x - cy, y - cx, color);
        }
        
        cx++;
        if (d > 0) {
            cy--;
            d = d + 4 * (cx - cy) + 10;
        } else {
            d = d + 4 * cx + 6;
        }
    }
    
    return ESP_OK;
}

esp_err_t fpc_a005_draw_bitmap(fpc_a005_handle_t handle, uint16_t x, uint16_t y, uint16_t w, uint16_t h, const uint8_t *bitmap) {
    if (!handle || !handle->is_initialized || !bitmap) {
        return ESP_ERR_INVALID_ARG;
    }
    
    for (uint16_t py = 0; py < h && (y + py) < FPC_A005_HEIGHT; py++) {
        for (uint16_t px = 0; px < w && (x + px) < FPC_A005_WIDTH; px++) {
            uint32_t bitmap_index = (py * w + px) / 2;
            bool is_high_nibble = ((py * w + px) % 2) == 0;
            
            fpc_a005_color_t color;
            if (is_high_nibble) {
                color = (bitmap[bitmap_index] >> 4) & 0x0F;
            } else {
                color = bitmap[bitmap_index] & 0x0F;
            }
            
            fpc_a005_set_pixel_in_buffer(handle, x + px, y + py, color);
        }
    }
    
    return ESP_OK;
}

esp_err_t fpc_a005_refresh(fpc_a005_handle_t handle, fpc_a005_refresh_mode_t mode) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->is_sleeping) {
        esp_err_t ret = fpc_a005_wake(handle);
        if (ret != ESP_OK) {
            return ret;
        }
    }
    
    ESP_LOGI(TAG, "Refreshing display with mode %d", mode);
    
    esp_err_t ret;
    
    // Send image data
    ret = fpc_a005_write_cmd(handle, FPC_A005_CMD_DATA_START_TRANSMISSION_1);
    if (ret == ESP_OK) {
        ret = fpc_a005_write_data(handle, handle->framebuffer, FPC_A005_BUFFER_SIZE);
    }
    
    if (ret == ESP_OK) {
        ret = fpc_a005_write_cmd(handle, FPC_A005_CMD_DISPLAY_REFRESH);
    }
    
    if (ret == ESP_OK) {
        ret = fpc_a005_wait_ready(handle, 30000); // Wait up to 30 seconds for refresh
    }
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Display refresh failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Display refresh completed");
    }
    
    return ret;
}

esp_err_t fpc_a005_sleep(fpc_a005_handle_t handle) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (handle->is_sleeping) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Entering sleep mode");
    
    esp_err_t ret = fpc_a005_write_cmd(handle, FPC_A005_CMD_POWER_OFF);
    if (ret == ESP_OK) {
        ret = fpc_a005_wait_ready(handle, 5000);
    }
    
    if (ret == ESP_OK) {
        ret = fpc_a005_write_cmd(handle, FPC_A005_CMD_DEEP_SLEEP);
        if (ret == ESP_OK) {
            ret = fpc_a005_write_data(handle, (uint8_t[]){0xA5}, 1);
        }
    }
    
    if (ret == ESP_OK) {
        handle->is_sleeping = true;
        ESP_LOGI(TAG, "Display entered sleep mode");
    } else {
        ESP_LOGE(TAG, "Failed to enter sleep mode: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t fpc_a005_wake(fpc_a005_handle_t handle) {
    if (!handle || !handle->is_initialized) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!handle->is_sleeping) {
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Waking up from sleep mode");
    
    // Reset and reinitialize
    esp_err_t ret = fpc_a005_reset(handle);
    
    if (ret == ESP_OK) {
        ret = fpc_a005_write_cmd(handle, FPC_A005_CMD_POWER_ON);
        if (ret == ESP_OK) {
            ret = fpc_a005_wait_ready(handle, 5000);
        }
    }
    
    if (ret == ESP_OK) {
        handle->is_sleeping = false;
        ESP_LOGI(TAG, "Display woke up successfully");
    } else {
        ESP_LOGE(TAG, "Failed to wake up display: %s", esp_err_to_name(ret));
    }
    
    return ret;
}
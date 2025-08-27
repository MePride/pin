/**
 * @file pin_display.c
 * @brief Pin Display System Implementation
 */

#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "pin_display.h"
#include "fpc_a005.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_sleep.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "driver/gpio.h"

static const char* TAG = "PIN_DISPLAY";

// Global display handle
static fpc_a005_handle_t g_display_handle = NULL;

// ADC handles for new API
static adc_oneshot_unit_handle_t adc1_handle;
static adc_cali_handle_t adc1_cali_handle;
static pin_display_config_t g_display_config;
static SemaphoreHandle_t g_display_mutex = NULL;

// Display refresh statistics
static struct {
    uint32_t total_refreshes;
    uint32_t full_refreshes;
    uint32_t partial_refreshes;
    uint32_t last_refresh_time;
    uint32_t last_full_refresh_time;
    uint8_t partial_refresh_count;
} g_refresh_stats = {0};

// Simple font definitions (bitmap fonts)
typedef struct {
    uint8_t width;
    uint8_t height;
    const uint8_t* data;
} pin_font_t;

// Simple 8x16 ASCII font data (simplified)
static const uint8_t font_8x16_data[] = {
    // Space (0x20)
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    // Add more characters as needed...
};

static pin_font_t fonts[] = {
    [PIN_FONT_SMALL]  = {8, 12, font_8x16_data},
    [PIN_FONT_MEDIUM] = {8, 16, font_8x16_data},
    [PIN_FONT_LARGE]  = {12, 24, font_8x16_data},
    [PIN_FONT_XLARGE] = {16, 32, font_8x16_data},
};

esp_err_t pin_display_init(void) {
    ESP_LOGI(TAG, "Initializing Pin display system");
    
    // Create display mutex
    g_display_mutex = xSemaphoreCreateMutex();
    if (!g_display_mutex) {
        ESP_LOGE(TAG, "Failed to create display mutex");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize display configuration
    g_display_config = (pin_display_config_t) {
        .fast_refresh_interval = 30,      // 30 seconds
        .partial_refresh_interval = 300,  // 5 minutes
        .full_refresh_interval = 1800,    // 30 minutes
        .sleep_after_inactive = 600,      // 10 minutes
        .max_partial_refresh = 10,        // 10 partial refreshes before full
        .auto_refresh_enabled = true,
        .power_save_enabled = true
    };
    
    // Configure FPC-A005 display
    fpc_a005_config_t display_config = {
        .spi_host = SPI2_HOST,
        .sck_io = 2,   // SCK GPIO
        .mosi_io = 3,  // MOSI GPIO
        .cs_io = 10,   // CS GPIO
        .dc_io = 4,    // DC GPIO
        .rst_io = 5,   // RST GPIO
        .busy_io = 6,  // BUSY GPIO
        .spi_clock_speed_hz = 4 * 1000 * 1000, // 4 MHz
    };
    
    // Initialize SPI bus
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = display_config.mosi_io,
        .sclk_io_num = display_config.sck_io,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(display_config.spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize SPI bus: %s", esp_err_to_name(ret));
        vSemaphoreDelete(g_display_mutex);
        return ret;
    }
    
    // Initialize display driver
    ret = fpc_a005_init(&display_config, &g_display_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to initialize display driver: %s", esp_err_to_name(ret));
        spi_bus_free(display_config.spi_host);
        vSemaphoreDelete(g_display_mutex);
        return ret;
    }
    
    // Initialize ADC for battery monitoring
    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = ADC_ATTEN_DB_12,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, ADC_CHANNEL_0, &config));
    
    // Initialize calibration handle
    adc_cali_curve_fitting_config_t cali_config = {
        .unit_id = ADC_UNIT_1,
        .chan = ADC_CHANNEL_0,
        .atten = ADC_ATTEN_DB_12,
        .bitwidth = ADC_BITWIDTH_DEFAULT,
    };
    ESP_ERROR_CHECK(adc_cali_create_scheme_curve_fitting(&cali_config, &adc1_cali_handle));
    
    ESP_LOGI(TAG, "Pin display system initialized successfully");
    return ESP_OK;
}

esp_err_t pin_display_deinit(void) {
    ESP_LOGI(TAG, "Deinitializing Pin display system");
    
    if (g_display_handle) {
        fpc_a005_deinit(g_display_handle);
        g_display_handle = NULL;
    }
    
    spi_bus_free(SPI2_HOST);
    
    if (g_display_mutex) {
        vSemaphoreDelete(g_display_mutex);
        g_display_mutex = NULL;
    }
    
    ESP_LOGI(TAG, "Pin display system deinitialized");
    return ESP_OK;
}

esp_err_t pin_display_clear(pin_color_t color) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_clear(g_display_handle, (fpc_a005_color_t)color);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_set_pixel(uint16_t x, uint16_t y, pin_color_t color) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_set_pixel(g_display_handle, x, y, (fpc_a005_color_t)color);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, pin_color_t color) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_draw_line(g_display_handle, x0, y0, x1, y1, (fpc_a005_color_t)color);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, pin_color_t color, bool filled) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_draw_rect(g_display_handle, x, y, w, h, (fpc_a005_color_t)color, filled);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_draw_circle(uint16_t x, uint16_t y, uint16_t r, pin_color_t color, bool filled) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_draw_circle(g_display_handle, x, y, r, (fpc_a005_color_t)color, filled);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_draw_text(uint16_t x, uint16_t y, const char* text, pin_font_size_t font_size, pin_color_t color) {
    if (!g_display_handle || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Simple text rendering (placeholder implementation)
    pin_font_t* font = &fonts[font_size];
    uint16_t cur_x = x;
    uint16_t cur_y = y;
    
    for (const char* c = text; *c; c++) {
        if (*c == '\n') {
            cur_x = x;
            cur_y += font->height + 2;
            continue;
        }
        
        // Draw character (simplified - just draw a rectangle for now)
        fpc_a005_draw_rect(g_display_handle, cur_x, cur_y, font->width, font->height, 
                          (fpc_a005_color_t)color, false);
        
        cur_x += font->width + 1;
        
        // Line wrap
        if (cur_x > FPC_A005_WIDTH - font->width) {
            cur_x = x;
            cur_y += font->height + 2;
        }
    }
    
    xSemaphoreGive(g_display_mutex);
    return ESP_OK;
}

esp_err_t pin_display_draw_wifi_icon(uint16_t x, uint16_t y, int8_t rssi, pin_color_t color) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Draw WiFi icon based on signal strength
    uint8_t bars = 1;
    if (rssi >= -30) bars = 4;
    else if (rssi >= -50) bars = 3;
    else if (rssi >= -70) bars = 2;
    
    // Draw signal bars
    for (uint8_t i = 0; i < 4; i++) {
        uint8_t bar_height = (i + 1) * 4;
        pin_color_t bar_color = (i < bars) ? color : PIN_COLOR_WHITE;
        
        fpc_a005_draw_rect(g_display_handle, x + i * 6, y + 16 - bar_height, 
                          4, bar_height, (fpc_a005_color_t)bar_color, true);
    }
    
    xSemaphoreGive(g_display_mutex);
    return ESP_OK;
}

esp_err_t pin_display_draw_battery_icon(uint16_t x, uint16_t y, uint8_t percentage, pin_color_t color) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Draw battery outline
    fpc_a005_draw_rect(g_display_handle, x, y, 24, 12, (fpc_a005_color_t)color, false);
    
    // Draw battery tip
    fpc_a005_draw_rect(g_display_handle, x + 24, y + 3, 2, 6, (fpc_a005_color_t)color, true);
    
    // Draw battery fill
    uint8_t fill_width = (percentage * 22) / 100;
    if (fill_width > 0) {
        pin_color_t fill_color = (percentage > 20) ? PIN_COLOR_GREEN : PIN_COLOR_RED;
        fpc_a005_draw_rect(g_display_handle, x + 1, y + 1, fill_width, 10, 
                          (fpc_a005_color_t)fill_color, true);
    }
    
    xSemaphoreGive(g_display_mutex);
    return ESP_OK;
}

esp_err_t pin_display_draw_loading_animation(uint16_t x, uint16_t y, uint8_t size) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    // Simple spinning dots animation
    static uint8_t animation_frame = 0;
    animation_frame = (animation_frame + 1) % 8;
    
    for (uint8_t i = 0; i < 8; i++) {
        float angle = i * M_PI / 4.0;
        uint16_t dot_x = x + (cos(angle) * size / 2);
        uint16_t dot_y = y + (sin(angle) * size / 2);
        
        pin_color_t dot_color = (i == animation_frame) ? PIN_COLOR_BLUE : PIN_COLOR_WHITE;
        fpc_a005_draw_circle(g_display_handle, dot_x, dot_y, 2, (fpc_a005_color_t)dot_color, true);
    }
    
    xSemaphoreGive(g_display_mutex);
    return ESP_OK;
}

esp_err_t pin_display_draw_qr_code(uint16_t x, uint16_t y, const char* text, uint8_t size) {
    if (!g_display_handle || !text) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // QR code generation would require a QR code library
    // For now, just draw a placeholder rectangle
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(1000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    fpc_a005_draw_rect(g_display_handle, x, y, size, size, FPC_A005_COLOR_BLACK, false);
    
    // Draw some pattern to represent QR code
    for (int i = 0; i < size; i += 4) {
        for (int j = 0; j < size; j += 4) {
            if ((i + j) % 8 == 0) {
                fpc_a005_draw_rect(g_display_handle, x + i, y + j, 2, 2, FPC_A005_COLOR_BLACK, true);
            }
        }
    }
    
    xSemaphoreGive(g_display_mutex);
    return ESP_OK;
}

esp_err_t pin_display_refresh(pin_refresh_mode_t mode) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Refreshing display with mode %d", mode);
    
    uint32_t start_time = esp_timer_get_time() / 1000;
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(30000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_refresh(g_display_handle, (fpc_a005_refresh_mode_t)mode);
    
    xSemaphoreGive(g_display_mutex);
    
    if (ret == ESP_OK) {
        uint32_t refresh_time = (esp_timer_get_time() / 1000) - start_time;
        
        g_refresh_stats.total_refreshes++;
        g_refresh_stats.last_refresh_time = start_time;
        
        if (mode == PIN_REFRESH_FULL) {
            g_refresh_stats.full_refreshes++;
            g_refresh_stats.last_full_refresh_time = start_time;
            g_refresh_stats.partial_refresh_count = 0;
        } else {
            g_refresh_stats.partial_refreshes++;
            g_refresh_stats.partial_refresh_count++;
        }
        
        ESP_LOGI(TAG, "Display refresh completed in %d ms", refresh_time);
    } else {
        ESP_LOGE(TAG, "Display refresh failed: %s", esp_err_to_name(ret));
    }
    
    return ret;
}

esp_err_t pin_display_sleep(void) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Putting display to sleep");
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_sleep(g_display_handle);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

esp_err_t pin_display_wake(void) {
    if (!g_display_handle) {
        return ESP_ERR_INVALID_STATE;
    }
    
    ESP_LOGI(TAG, "Waking display from sleep");
    
    if (xSemaphoreTake(g_display_mutex, pdMS_TO_TICKS(5000)) != pdTRUE) {
        return ESP_ERR_TIMEOUT;
    }
    
    esp_err_t ret = fpc_a005_wake(g_display_handle);
    
    xSemaphoreGive(g_display_mutex);
    return ret;
}

float pin_battery_get_voltage(void) {
    int adc_raw;
    int voltage_mv;
    
    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, ADC_CHANNEL_0, &adc_raw));
    ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_handle, adc_raw, &voltage_mv));
    
    // Convert to voltage and apply voltage divider correction
    float voltage = (voltage_mv / 1000.0f) * 2.0f;  // Assuming 1:1 voltage divider
    
    return voltage;
}

uint8_t pin_battery_get_percentage(float voltage) {
    // LiPo battery voltage range: 3.0V-4.2V
    if (voltage >= 4.2f) return 100;
    if (voltage <= 3.0f) return 0;
    
    // Simple linear conversion
    return (uint8_t)((voltage - 3.0f) / 1.2f * 100.0f);
}

bool pin_should_enter_sleep(void) {
    // Simple sleep logic - sleep if no activity for configured time
    uint32_t current_time = esp_timer_get_time() / 1000000; // Convert to seconds
    uint32_t last_activity = g_refresh_stats.last_refresh_time / 1000; // Convert to seconds
    
    if (g_display_config.power_save_enabled && 
        (current_time - last_activity) > g_display_config.sleep_after_inactive) {
        return true;
    }
    
    return false;
}

void pin_enter_deep_sleep(void) {
    ESP_LOGI(TAG, "Entering deep sleep mode");
    
    // Put display to sleep first
    pin_display_sleep();
    
    // Configure wake up sources
    esp_sleep_enable_timer_wakeup(10 * 60 * 1000000ULL); // Wake up after 10 minutes
    esp_sleep_enable_gpio_wakeup(); // Enable GPIO wake up source
    
    // Enter deep sleep
    esp_deep_sleep_start();
}

fpc_a005_handle_t pin_display_get_handle(void) {
    return g_display_handle;
}
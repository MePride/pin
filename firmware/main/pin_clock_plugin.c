/**
 * @file pin_clock_plugin.c 
 * @brief Simple clock plugin for Pin device
 */

#include <time.h>
#include <sys/time.h>
#include "pin_plugin.h"
#include "esp_log.h"

static const char* TAG = "CLOCK_PLUGIN";

// Plugin initialization
static esp_err_t clock_init(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Clock plugin initialized");
    if (ctx && ctx->api.display_set_font_size) {
        ctx->api.display_set_font_size(32); // XL font for readability
    }
    return ESP_OK;
}

// Plugin start
static esp_err_t clock_start(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Clock plugin started");
    return ESP_OK;
}

// Plugin update - called periodically 
static esp_err_t clock_update(pin_plugin_context_t* ctx) {
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%H:%M:%S", &timeinfo);
    
    // Update display content through context API if available
    if (ctx && ctx->api.display_update_content) {
        ctx->api.display_update_content(strftime_buf);
    }
    
    return ESP_OK;
}

// Plugin render
static esp_err_t clock_render(pin_plugin_context_t* ctx, pin_widget_region_t* region) {
    if (!region) {
        return ESP_ERR_INVALID_ARG;
    }
    
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    
    time(&now);
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%H:%M", &timeinfo);
    
    // Update region content
    if (region->content) {
        free(region->content);
    }
    region->content = strdup(strftime_buf);
    region->dirty = true;
    
    return ESP_OK;
}

// Plugin cleanup
static esp_err_t clock_cleanup(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Clock plugin cleaned up");
    return ESP_OK;
}

// Clock plugin definition
pin_plugin_t clock_plugin = {
    .metadata = {
        .name = "clock",
        .version = "1.0.0", 
        .author = "Pin Team",
        .description = "Simple clock display plugin",
        .homepage = "https://github.com/pin-project",
        .min_firmware_version = 100  // v1.0.0
    },
    .config = {
        .memory_limit = 4096,
        .update_interval = 30,  // Update every 30 seconds
        .api_rate_limit = 10,
        .auto_start = true,
        .persistent = true
    },
    .init = clock_init,
    .start = clock_start,
    .update = clock_update,
    .render = clock_render,
    .cleanup = clock_cleanup,
    .state = PLUGIN_STATE_UNLOADED,
    .enabled = false,
    .initialized = false,
    .running = false,
    .private_data = NULL
};

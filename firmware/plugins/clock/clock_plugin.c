/**
 * @file clock_plugin.c
 * @brief Pin Clock Plugin Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "pin_plugin.h"

static const char* TAG = "CLOCK_PLUGIN";

// Clock plugin private data
typedef struct {
    char time_format[32];       // Time format string
    bool show_seconds;          // Show seconds flag
    bool use_24hour;           // Use 24-hour format
    uint8_t font_size;         // Font size
    uint8_t text_color;        // Text color
    char last_time_str[32];    // Last displayed time string
    bool time_changed;         // Time changed flag
} clock_private_data_t;

// Plugin lifecycle functions
static esp_err_t clock_init(pin_plugin_context_t* ctx);
static esp_err_t clock_start(pin_plugin_context_t* ctx);
static esp_err_t clock_update(pin_plugin_context_t* ctx);
static esp_err_t clock_render(pin_plugin_context_t* ctx, pin_widget_region_t* region);
static esp_err_t clock_config_changed(pin_plugin_context_t* ctx, const char* key, const char* value);
static esp_err_t clock_stop(pin_plugin_context_t* ctx);
static esp_err_t clock_cleanup(pin_plugin_context_t* ctx);

// Helper functions
static void clock_load_config(pin_plugin_context_t* ctx, clock_private_data_t* data);
static bool clock_is_time_format_valid(const char* format);

static esp_err_t clock_init(pin_plugin_context_t* ctx) {
    ctx->api.log_info(TAG, "Initializing clock plugin");
    
    // Allocate private data
    clock_private_data_t* data = pin_plugin_malloc(ctx, sizeof(clock_private_data_t));
    if (!data) {
        ctx->api.log_error(TAG, "Failed to allocate private data");
        return ESP_ERR_NO_MEM;
    }
    
    // Initialize default configuration
    strcpy(data->time_format, "%H:%M");
    data->show_seconds = false;
    data->use_24hour = true;
    data->font_size = 24;
    data->text_color = 0; // Black
    data->last_time_str[0] = '\0';
    data->time_changed = true;
    
    ctx->plugin->private_data = data;
    
    // Load configuration from storage
    clock_load_config(ctx, data);
    
    ctx->api.log_info(TAG, "Clock plugin initialized with format: %s", data->time_format);
    return ESP_OK;
}

static esp_err_t clock_start(pin_plugin_context_t* ctx) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    ctx->api.log_info(TAG, "Starting clock plugin");
    
    // Configure display widget
    ctx->widget_region.x = 100;
    ctx->widget_region.y = 180;
    ctx->widget_region.width = 400;
    ctx->widget_region.height = 80;
    ctx->widget_region.font_size = data->font_size;
    ctx->widget_region.color = data->text_color;
    ctx->widget_region.visible = true;
    ctx->widget_region.dirty = true;
    
    ctx->api.log_info(TAG, "Clock plugin started");
    return ESP_OK;
}

static esp_err_t clock_update(pin_plugin_context_t* ctx) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    if (!data) {
        ctx->api.log_error(TAG, "Private data is NULL");
        return ESP_ERR_INVALID_STATE;
    }
    
    // Get current time string
    char current_time[32];
    esp_err_t ret = ctx->api.get_time_string(current_time, sizeof(current_time), data->time_format);
    if (ret != ESP_OK) {
        ctx->api.log_error(TAG, "Failed to get time string");
        return ret;
    }
    
    // Check if time has changed
    if (strcmp(current_time, data->last_time_str) != 0) {
        strcpy(data->last_time_str, current_time);
        data->time_changed = true;
        
        // Update widget content
        if (ctx->widget_region.content) {
            free(ctx->widget_region.content);
        }
        ctx->widget_region.content = strdup(current_time);
        ctx->widget_region.dirty = true;
        
        // Update display content through API
        ret = ctx->api.display_update_content(current_time);
        if (ret == ESP_OK) {
            ctx->api.log_info(TAG, "Time updated: %s", current_time);
        } else {
            ctx->api.log_warn(TAG, "Display update not implemented yet");
        }
    }
    
    return ESP_OK;
}

static esp_err_t clock_render(pin_plugin_context_t* ctx, pin_widget_region_t* region) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    if (!data || !region) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Set display properties
    esp_err_t ret = ctx->api.display_set_font_size(data->font_size);
    if (ret == ESP_OK) {
        ret = ctx->api.display_set_color(data->text_color);
    }
    
    if (ret != ESP_OK) {
        ctx->api.log_warn(TAG, "Display API calls not fully implemented");
    }
    
    return ESP_OK;
}

static esp_err_t clock_config_changed(pin_plugin_context_t* ctx, const char* key, const char* value) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    if (!data || !key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ctx->api.log_info(TAG, "Configuration changed: %s = %s", key, value);
    
    bool config_updated = false;
    
    if (strcmp(key, "time_format") == 0) {
        if (clock_is_time_format_valid(value)) {
            strncpy(data->time_format, value, sizeof(data->time_format) - 1);
            data->time_format[sizeof(data->time_format) - 1] = '\0';
            config_updated = true;
        } else {
            ctx->api.log_warn(TAG, "Invalid time format: %s", value);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (strcmp(key, "font_size") == 0) {
        int font_size = atoi(value);
        if (font_size >= 12 && font_size <= 48) {
            data->font_size = font_size;
            ctx->widget_region.font_size = font_size;
            config_updated = true;
        } else {
            ctx->api.log_warn(TAG, "Invalid font size: %s", value);
            return ESP_ERR_INVALID_ARG;
        }
    } else if (strcmp(key, "show_seconds") == 0) {
        data->show_seconds = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        
        // Update time format based on show_seconds setting
        if (data->show_seconds) {
            if (data->use_24hour) {
                strcpy(data->time_format, "%H:%M:%S");
            } else {
                strcpy(data->time_format, "%I:%M:%S %p");
            }
        } else {
            if (data->use_24hour) {
                strcpy(data->time_format, "%H:%M");
            } else {
                strcpy(data->time_format, "%I:%M %p");
            }
        }
        config_updated = true;
    } else if (strcmp(key, "use_24hour") == 0) {
        data->use_24hour = (strcmp(value, "true") == 0 || strcmp(value, "1") == 0);
        
        // Update time format based on use_24hour setting
        if (data->show_seconds) {
            if (data->use_24hour) {
                strcpy(data->time_format, "%H:%M:%S");
            } else {
                strcpy(data->time_format, "%I:%M:%S %p");
            }
        } else {
            if (data->use_24hour) {
                strcpy(data->time_format, "%H:%M");
            } else {
                strcpy(data->time_format, "%I:%M %p");
            }
        }
        config_updated = true;
    } else if (strcmp(key, "text_color") == 0) {
        int color = atoi(value);
        if (color >= 0 && color <= 6) { // Valid color range for 7-color display
            data->text_color = color;
            ctx->widget_region.color = color;
            config_updated = true;
        } else {
            ctx->api.log_warn(TAG, "Invalid text color: %s", value);
            return ESP_ERR_INVALID_ARG;
        }
    } else {
        ctx->api.log_warn(TAG, "Unknown configuration key: %s", key);
        return ESP_ERR_NOT_FOUND;
    }
    
    if (config_updated) {
        // Force time update on next cycle
        data->time_changed = true;
        data->last_time_str[0] = '\0';
        ctx->widget_region.dirty = true;
        
        ctx->api.log_info(TAG, "Configuration updated successfully");
    }
    
    return ESP_OK;
}

static esp_err_t clock_stop(pin_plugin_context_t* ctx) {
    ctx->api.log_info(TAG, "Stopping clock plugin");
    
    // Clear display content
    if (ctx->widget_region.content) {
        free(ctx->widget_region.content);
        ctx->widget_region.content = NULL;
    }
    
    ctx->widget_region.visible = false;
    ctx->widget_region.dirty = true;
    
    ctx->api.log_info(TAG, "Clock plugin stopped");
    return ESP_OK;
}

static esp_err_t clock_cleanup(pin_plugin_context_t* ctx) {
    ctx->api.log_info(TAG, "Cleaning up clock plugin");
    
    if (ctx->plugin->private_data) {
        clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
        pin_plugin_free(ctx, data, sizeof(clock_private_data_t));
        ctx->plugin->private_data = NULL;
    }
    
    if (ctx->widget_region.content) {
        free(ctx->widget_region.content);
        ctx->widget_region.content = NULL;
    }
    
    ctx->api.log_info(TAG, "Clock plugin cleaned up");
    return ESP_OK;
}

static void clock_load_config(pin_plugin_context_t* ctx, clock_private_data_t* data) {
    char config_value[64];
    
    // Load time format
    if (ctx->api.config_get("time_format", config_value, sizeof(config_value)) == ESP_OK) {
        if (clock_is_time_format_valid(config_value)) {
            strncpy(data->time_format, config_value, sizeof(data->time_format) - 1);
        }
    }
    
    // Load font size
    if (ctx->api.config_get("font_size", config_value, sizeof(config_value)) == ESP_OK) {
        int font_size = atoi(config_value);
        if (font_size >= 12 && font_size <= 48) {
            data->font_size = font_size;
        }
    }
    
    // Load show seconds setting
    if (ctx->api.config_get("show_seconds", config_value, sizeof(config_value)) == ESP_OK) {
        data->show_seconds = (strcmp(config_value, "true") == 0 || strcmp(config_value, "1") == 0);
    }
    
    // Load 24-hour format setting
    if (ctx->api.config_get("use_24hour", config_value, sizeof(config_value)) == ESP_OK) {
        data->use_24hour = (strcmp(config_value, "true") == 0 || strcmp(config_value, "1") == 0);
    }
    
    // Load text color
    if (ctx->api.config_get("text_color", config_value, sizeof(config_value)) == ESP_OK) {
        int color = atoi(config_value);
        if (color >= 0 && color <= 6) {
            data->text_color = color;
        }
    }
    
    ctx->api.log_info(TAG, "Configuration loaded");
}

static bool clock_is_time_format_valid(const char* format) {
    if (!format || strlen(format) == 0) {
        return false;
    }
    
    // Basic validation - check for common time format specifiers
    if (strstr(format, "%H") || strstr(format, "%I") || 
        strstr(format, "%M") || strstr(format, "%S") || 
        strstr(format, "%p")) {
        return true;
    }
    
    return false;
}

// Plugin definition
pin_plugin_t clock_plugin = {
    .metadata = {
        .name = "clock",
        .version = "1.0.0",
        .author = "Pin Team",
        .description = "Digital clock widget with customizable format",
        .homepage = "https://github.com/pin-project/plugins/clock",
        .min_firmware_version = 0x010000  // v1.0.0
    },
    .config = {
        .memory_limit = 4096,     // 4KB
        .update_interval = 10,    // 10 seconds (for seconds display)
        .api_rate_limit = 20,     // 20 calls per minute
        .auto_start = true,
        .persistent = true
    },
    .init = clock_init,
    .start = clock_start,
    .update = clock_update,
    .render = clock_render,
    .config_changed = clock_config_changed,
    .stop = clock_stop,
    .cleanup = clock_cleanup,
    .state = PLUGIN_STATE_UNLOADED,
    .enabled = false,
    .initialized = false,
    .running = false,
    .last_update_time = 0,
    .error_count = 0,
    .private_data = NULL,
    .plugin_id = 0,
    .plugin_task = NULL
};
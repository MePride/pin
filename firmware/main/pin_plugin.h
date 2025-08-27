/**
 * @file pin_plugin.h
 * @brief Pin Plugin System API (simplified)
 */

#pragma once

#include "esp_err.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_MAX_PLUGINS 8
#define PIN_PLUGIN_NAME_MAX_LEN 32
#define PIN_PLUGIN_VERSION_MAX_LEN 16
#define PIN_PLUGIN_AUTHOR_MAX_LEN 64
#define PIN_PLUGIN_DESC_MAX_LEN 128
#define PIN_PLUGIN_HOMEPAGE_MAX_LEN 256

// Forward declarations
typedef struct pin_plugin pin_plugin_t;
typedef struct pin_plugin_context pin_plugin_context_t;

// Plugin metadata
typedef struct {
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    const char* homepage;
    uint32_t min_firmware_version;
} pin_plugin_metadata_t;

// Plugin configuration
typedef struct {
    uint32_t memory_limit;
    uint32_t update_interval;
    uint32_t api_rate_limit;
    bool auto_start;
    bool persistent;
} pin_plugin_config_t;

// Plugin states
typedef enum {
    PLUGIN_STATE_UNLOADED,
    PLUGIN_STATE_LOADED,
    PLUGIN_STATE_INITIALIZED,
    PLUGIN_STATE_RUNNING,
    PLUGIN_STATE_SUSPENDED,
    PLUGIN_STATE_ERROR
} pin_plugin_state_t;

// Widget region for display
typedef struct {
    uint16_t x, y, width, height;
    uint8_t color;
    char* content;
    uint8_t font_size;
    bool visible;
    bool dirty;
} pin_widget_region_t;

// Plugin context
struct pin_plugin_context {
    pin_plugin_t* plugin;
    pin_widget_region_t widget_region;
    
    // System API interface
    struct {
        // Display system
        esp_err_t (*display_update_content)(const char* content);
        esp_err_t (*display_set_color)(uint8_t color);
        esp_err_t (*display_set_font_size)(uint8_t font_size);
    } api;
    
    // Resource monitoring
    struct {
        uint32_t memory_used;
        uint32_t memory_peak;
        uint32_t api_calls_count;
        uint32_t update_count;
        uint32_t error_count;
    } stats;
    
    bool is_suspended;
    bool is_blocked;
    uint32_t suspension_reason;
};

// Plugin structure
struct pin_plugin {
    pin_plugin_metadata_t metadata;
    pin_plugin_config_t config;
    
    // Lifecycle callback functions
    esp_err_t (*init)(pin_plugin_context_t* ctx);
    esp_err_t (*start)(pin_plugin_context_t* ctx);
    esp_err_t (*update)(pin_plugin_context_t* ctx);
    esp_err_t (*render)(pin_plugin_context_t* ctx, pin_widget_region_t* region);
    esp_err_t (*config_changed)(pin_plugin_context_t* ctx, const char* key, const char* value);
    esp_err_t (*stop)(pin_plugin_context_t* ctx);
    esp_err_t (*cleanup)(pin_plugin_context_t* ctx);
    
    // Plugin state
    pin_plugin_state_t state;
    bool enabled;
    bool initialized;
    bool running;
    uint32_t last_update_time;
    uint32_t error_count;
    
    void* private_data;
    uint8_t plugin_id;
    TaskHandle_t plugin_task;
};

// Function declarations
esp_err_t pin_plugin_manager_init(void);
esp_err_t pin_plugin_register(pin_plugin_t* plugin);
esp_err_t pin_plugin_enable(const char* plugin_name, bool enable);
pin_plugin_t* pin_plugin_find_by_name(const char* plugin_name);
esp_err_t pin_plugin_get_list(pin_plugin_t** plugins, uint8_t max_plugins, uint8_t* plugin_count);
void* pin_plugin_malloc(pin_plugin_context_t* ctx, size_t size);
void pin_plugin_free(pin_plugin_context_t* ctx, void* ptr, size_t size);

#ifdef __cplusplus
}
#endif
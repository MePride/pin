/**
 * @file pin_plugin.h
 * @brief Pin Plugin System API
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
    const char* name;           // Plugin name (unique identifier)
    const char* version;        // Version (semantic versioning)
    const char* author;         // Author information
    const char* description;    // Plugin description
    const char* homepage;       // Homepage URL
    uint32_t min_firmware_version; // Minimum firmware version requirement
} pin_plugin_metadata_t;

// Plugin configuration
typedef struct {
    uint32_t memory_limit;      // Memory limit (bytes)
    uint32_t update_interval;   // Update interval (seconds)
    uint32_t api_rate_limit;    // API call rate limit (calls/minute)
    bool auto_start;            // Auto start on boot
    bool persistent;            // Keep running persistently
} pin_plugin_config_t;

// Plugin states
typedef enum {
    PLUGIN_STATE_UNLOADED,    // Not loaded
    PLUGIN_STATE_LOADED,      // Loaded but not initialized
    PLUGIN_STATE_INITIALIZED, // Initialized
    PLUGIN_STATE_RUNNING,     // Running
    PLUGIN_STATE_SUSPENDED,   // Suspended
    PLUGIN_STATE_ERROR        // Error state
} pin_plugin_state_t;

// Widget region for display
typedef struct {
    uint16_t x, y, width, height;    // Position and size
    uint8_t color;                   // Color configuration
    char* content;                   // Display content
    uint8_t font_size;               // Font size
    bool visible;                    // Visibility flag
    bool dirty;                      // Needs update flag
} pin_widget_region_t;

// Plugin context - provides API access to plugins
struct pin_plugin_context {
    pin_plugin_t* plugin;               // Associated plugin
    pin_widget_region_t widget_region;  // Allocated display region
    
    // System API interface (plugins can only access system through these functions)
    struct {
        // Logging system
        esp_err_t (*log_info)(const char* tag, const char* format, ...);
        esp_err_t (*log_warn)(const char* tag, const char* format, ...);
        esp_err_t (*log_error)(const char* tag, const char* format, ...);
        
        // Network access
        esp_err_t (*http_get)(const char* url, char* response, size_t max_len);
        esp_err_t (*http_post)(const char* url, const char* data, char* response, size_t max_len);
        
        // Configuration storage
        esp_err_t (*config_get)(const char* key, char* value, size_t max_len);
        esp_err_t (*config_set)(const char* key, const char* value);
        esp_err_t (*config_delete)(const char* key);
        
        // Time system
        uint64_t (*get_timestamp)(void);
        esp_err_t (*get_time_string)(char* time_str, size_t max_len, const char* format);
        
        // Display system
        esp_err_t (*display_update_content)(const char* content);
        esp_err_t (*display_set_color)(uint8_t color);
        esp_err_t (*display_set_font_size)(uint8_t font_size);
        
        // Timer system
        esp_err_t (*schedule_update)(uint32_t delay_seconds);
        esp_err_t (*cancel_scheduled_update)(void);
        
        // Event system
        esp_err_t (*emit_event)(const char* event_name, const char* data);
        esp_err_t (*subscribe_event)(const char* event_name, void (*callback)(const char* data));
    } api;
    
    // Resource monitoring
    struct {
        uint32_t memory_used;           // Current memory usage
        uint32_t memory_peak;           // Peak memory usage
        uint32_t api_calls_count;       // API calls count
        uint32_t api_calls_last_reset;  // Last API count reset time
        uint32_t update_count;          // Update count
        uint32_t error_count;           // Error count
    } stats;
    
    // Sandbox state
    bool is_suspended;                  // Suspended flag
    bool is_blocked;                    // Blocked flag
    uint32_t suspension_reason;         // Suspension reason
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
    
    // Private data
    void* private_data;
    
    // Internal management fields
    uint8_t plugin_id;
    TaskHandle_t plugin_task;
};

// Plugin statistics
typedef struct {
    char plugin_name[PIN_PLUGIN_NAME_MAX_LEN];
    pin_plugin_state_t state;
    uint32_t memory_used;
    uint32_t memory_peak;
    uint32_t update_count;
    uint32_t error_count;
    uint32_t api_calls;
    uint32_t uptime_seconds;
    bool is_suspended;
} pin_plugin_stats_t;

/**
 * @brief Initialize plugin manager
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_manager_init(void);

/**
 * @brief Register a plugin
 * @param plugin Plugin structure
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_register(pin_plugin_t* plugin);

/**
 * @brief Enable/disable a plugin
 * @param plugin_name Plugin name
 * @param enable Enable flag
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_enable(const char* plugin_name, bool enable);

/**
 * @brief Set plugin configuration
 * @param plugin_name Plugin name
 * @param key Configuration key
 * @param value Configuration value
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_set_config(const char* plugin_name, const char* key, const char* value);

/**
 * @brief Get plugin configuration
 * @param plugin_name Plugin name
 * @param key Configuration key
 * @param value Buffer to store value
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_get_config(const char* plugin_name, const char* key, char* value, size_t max_len);

/**
 * @brief Get plugin statistics
 * @param plugin_name Plugin name
 * @param stats Statistics structure
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_get_stats(const char* plugin_name, pin_plugin_stats_t* stats);

/**
 * @brief Get all plugins list
 * @param plugins Array to store plugin pointers
 * @param max_plugins Maximum number of plugins
 * @param plugin_count Actual number of plugins
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_get_list(pin_plugin_t** plugins, uint8_t max_plugins, uint8_t* plugin_count);

/**
 * @brief Run plugin system diagnostics
 * @return ESP_OK on success
 */
esp_err_t pin_plugin_system_diagnostic(void);

/**
 * @brief Validate plugin structure
 * @param plugin Plugin to validate
 * @return ESP_OK if valid
 */
esp_err_t pin_plugin_validate(pin_plugin_t* plugin);

/**
 * @brief Find plugin by name
 * @param plugin_name Plugin name
 * @return Plugin pointer or NULL if not found
 */
pin_plugin_t* pin_plugin_find_by_name(const char* plugin_name);

/**
 * @brief Plugin malloc with tracking
 * @param ctx Plugin context
 * @param size Size to allocate
 * @return Allocated memory pointer
 */
void* pin_plugin_malloc(pin_plugin_context_t* ctx, size_t size);

/**
 * @brief Plugin free with tracking
 * @param ctx Plugin context
 * @param ptr Memory pointer
 * @param size Size of memory
 */
void pin_plugin_free(pin_plugin_context_t* ctx, void* ptr, size_t size);

#ifdef __cplusplus
}
#endif
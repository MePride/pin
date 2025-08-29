/**
 * @file pin_plugin.c
 * @brief Pin Plugin System Implementation
 */

#include <string.h>
#include <stdlib.h>
#include "pin_plugin.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_http_client.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"

static const char* TAG = "PIN_PLUGIN";

#define PIN_PLUGIN_DEFAULT_MEMORY_LIMIT (64 * 1024)   // 64KB
#define PIN_PLUGIN_API_RATE_LIMIT 100                 // 100 calls per minute
#define PIN_PLUGIN_MAX_ERRORS 5                       // Maximum error count

// Plugin manager structure
typedef struct {
    pin_plugin_t* plugins[PIN_MAX_PLUGINS];           // Plugin array
    pin_plugin_context_t contexts[PIN_MAX_PLUGINS];   // Context array
    uint8_t plugin_count;                             // Current plugin count
    
    // Task management
    TaskHandle_t manager_task_handle;                 // Manager task handle
    QueueHandle_t message_queue;                      // Message queue
    SemaphoreHandle_t plugins_mutex;                  // Plugin list mutex
    
    // System state
    bool plugins_enabled;                             // Plugin system enabled
    bool auto_load_enabled;                           // Auto load enabled
    uint32_t last_gc_time;                           // Last garbage collection time
} pin_plugin_manager_t;

// Global plugin manager instance
static pin_plugin_manager_t g_plugin_manager = {0};

// Plugin message types
typedef enum {
    PLUGIN_MSG_ENABLE,
    PLUGIN_MSG_DISABLE,
    PLUGIN_MSG_CONFIG_CHANGED,
    PLUGIN_MSG_SHUTDOWN
} pin_plugin_message_type_t;

typedef struct {
    pin_plugin_message_type_t type;
    char plugin_name[PIN_PLUGIN_NAME_MAX_LEN];
    char key[32];
    char value[128];
} pin_plugin_message_t;

// Forward declarations
static void pin_plugin_manager_task(void* pvParameters);
static void pin_plugin_task_wrapper(void* pvParameters);
static esp_err_t pin_plugin_init_context(pin_plugin_context_t* ctx, pin_plugin_t* plugin);
static esp_err_t pin_plugin_check_resources(pin_plugin_context_t* ctx);

// API implementation functions
static esp_err_t plugin_api_log_info(const char* tag, const char* format, ...);
static esp_err_t plugin_api_log_warn(const char* tag, const char* format, ...);
static esp_err_t plugin_api_log_error(const char* tag, const char* format, ...);
static esp_err_t plugin_api_http_get(const char* url, char* response, size_t max_len);
static esp_err_t plugin_api_http_post(const char* url, const char* data, char* response, size_t max_len);
static esp_err_t plugin_api_config_get(const char* key, char* value, size_t max_len);
static esp_err_t plugin_api_config_set(const char* key, const char* value);
static esp_err_t plugin_api_config_delete(const char* key);
static uint64_t plugin_api_get_timestamp(void);
static esp_err_t plugin_api_get_time_string(char* time_str, size_t max_len, const char* format);
static esp_err_t plugin_api_display_update_content(const char* content);
static esp_err_t plugin_api_display_set_color(uint8_t color);
static esp_err_t plugin_api_display_set_font_size(uint8_t font_size);
static esp_err_t plugin_api_schedule_update(uint32_t delay_seconds);
static esp_err_t plugin_api_cancel_scheduled_update(void);
static esp_err_t plugin_api_emit_event(const char* event_name, const char* data);
static esp_err_t plugin_api_subscribe_event(const char* event_name, void (*callback)(const char* data));

esp_err_t pin_plugin_manager_init(void) {
    ESP_LOGI(TAG, "Initializing plugin manager");
    
    // Initialize mutex and queue
    g_plugin_manager.plugins_mutex = xSemaphoreCreateMutex();
    if (!g_plugin_manager.plugins_mutex) {
        ESP_LOGE(TAG, "Failed to create plugins mutex");
        return ESP_ERR_NO_MEM;
    }
    
    g_plugin_manager.message_queue = xQueueCreate(10, sizeof(pin_plugin_message_t));
    if (!g_plugin_manager.message_queue) {
        ESP_LOGE(TAG, "Failed to create message queue");
        vSemaphoreDelete(g_plugin_manager.plugins_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    // Create manager task
    BaseType_t ret = xTaskCreate(
        pin_plugin_manager_task,
        "plugin_mgr",
        4096,
        NULL,
        5,
        &g_plugin_manager.manager_task_handle
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create manager task");
        vQueueDelete(g_plugin_manager.message_queue);
        vSemaphoreDelete(g_plugin_manager.plugins_mutex);
        return ESP_ERR_NO_MEM;
    }
    
    g_plugin_manager.plugins_enabled = true;
    
    ESP_LOGI(TAG, "Plugin manager initialized successfully");
    return ESP_OK;
}

esp_err_t pin_plugin_register(pin_plugin_t* plugin) {
    if (!plugin) {
        ESP_LOGE(TAG, "Plugin pointer is NULL");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Validate plugin
    esp_err_t ret = ESP_OK;
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Plugin validation failed");
        return ret;
    }
    
    if (xSemaphoreTake(g_plugin_manager.plugins_mutex, pdMS_TO_TICKS(1000))) {
        if (g_plugin_manager.plugin_count >= PIN_MAX_PLUGINS) {
            xSemaphoreGive(g_plugin_manager.plugins_mutex);
            ESP_LOGE(TAG, "Maximum number of plugins reached");
            return ESP_ERR_NO_MEM;
        }
        
        // Check for duplicate names
        for (int i = 0; i < g_plugin_manager.plugin_count; i++) {
            if (strcmp(g_plugin_manager.plugins[i]->metadata.name, plugin->metadata.name) == 0) {
                xSemaphoreGive(g_plugin_manager.plugins_mutex);
                ESP_LOGE(TAG, "Plugin with name '%s' already exists", plugin->metadata.name);
                return ESP_ERR_INVALID_STATE;
            }
        }
        
        uint8_t plugin_id = g_plugin_manager.plugin_count++;
        plugin->plugin_id = plugin_id;
        plugin->state = PLUGIN_STATE_LOADED;
        
        g_plugin_manager.plugins[plugin_id] = plugin;
        
        // Initialize plugin context
        ret = pin_plugin_init_context(&g_plugin_manager.contexts[plugin_id], plugin);
        
        xSemaphoreGive(g_plugin_manager.plugins_mutex);
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "Plugin '%s' registered with ID %d", plugin->metadata.name, plugin_id);
        } else {
            ESP_LOGE(TAG, "Failed to initialize context for plugin '%s'", plugin->metadata.name);
        }
        
        return ret;
    }
    
    return ESP_ERR_TIMEOUT;
}

esp_err_t pin_plugin_enable(const char* plugin_name, bool enable) {
    if (!plugin_name) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pin_plugin_t* plugin = pin_plugin_find_by_name(plugin_name);
    if (!plugin) {
        ESP_LOGE(TAG, "Plugin '%s' not found", plugin_name);
        return ESP_ERR_NOT_FOUND;
    }
    
    pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
    
    if (enable && !plugin->enabled) {
        ESP_LOGI(TAG, "Enabling plugin '%s'", plugin_name);
        
        // Initialize plugin if not already done
        if (!plugin->initialized && plugin->init) {
            esp_err_t ret = plugin->init(ctx);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to initialize plugin '%s': %s", plugin_name, esp_err_to_name(ret));
                plugin->state = PLUGIN_STATE_ERROR;
                return ret;
            }
            plugin->initialized = true;
            plugin->state = PLUGIN_STATE_INITIALIZED;
        }
        
        // Start plugin
        if (plugin->start) {
            esp_err_t ret = plugin->start(ctx);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to start plugin '%s': %s", plugin_name, esp_err_to_name(ret));
                plugin->state = PLUGIN_STATE_ERROR;
                return ret;
            }
        }
        
        plugin->enabled = true;
        plugin->running = true;
        plugin->state = PLUGIN_STATE_RUNNING;
        
        // Create plugin task
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "plugin_%s", plugin_name);
        BaseType_t task_ret = xTaskCreate(
            pin_plugin_task_wrapper,
            task_name,
            4096,
            plugin,
            4,
            &plugin->plugin_task
        );
        
        if (task_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create task for plugin '%s'", plugin_name);
            plugin->enabled = false;
            plugin->running = false;
            plugin->state = PLUGIN_STATE_ERROR;
            return ESP_FAIL;
        }
        
        ESP_LOGI(TAG, "Plugin '%s' enabled successfully", plugin_name);
        
    } else if (!enable && plugin->enabled) {
        ESP_LOGI(TAG, "Disabling plugin '%s'", plugin_name);
        
        plugin->enabled = false;
        plugin->running = false;
        plugin->state = PLUGIN_STATE_LOADED;
        
        // Delete plugin task
        if (plugin->plugin_task) {
            vTaskDelete(plugin->plugin_task);
            plugin->plugin_task = NULL;
        }
        
        // Stop plugin
        if (plugin->stop) {
            plugin->stop(ctx);
        }
        
        ESP_LOGI(TAG, "Plugin '%s' disabled successfully", plugin_name);
    }
    
    return ESP_OK;
}

pin_plugin_t* pin_plugin_find_by_name(const char* plugin_name) {
    if (!plugin_name) {
        return NULL;
    }
    
    for (int i = 0; i < g_plugin_manager.plugin_count; i++) {
        if (strcmp(g_plugin_manager.plugins[i]->metadata.name, plugin_name) == 0) {
            return g_plugin_manager.plugins[i];
        }
    }
    
    return NULL;
}

esp_err_t pin_plugin_get_list(pin_plugin_t** plugins, uint8_t max_plugins, uint8_t* plugin_count) {
    if (!plugins || !plugin_count) {
        return ESP_ERR_INVALID_ARG;
    }
    
    if (xSemaphoreTake(g_plugin_manager.plugins_mutex, pdMS_TO_TICKS(1000))) {
        uint8_t count = 0;
        for (int i = 0; i < g_plugin_manager.plugin_count && count < max_plugins; i++) {
            plugins[count++] = g_plugin_manager.plugins[i];
        }
        *plugin_count = count;
        
        xSemaphoreGive(g_plugin_manager.plugins_mutex);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

static esp_err_t pin_plugin_init_context(pin_plugin_context_t* ctx, pin_plugin_t* plugin) {
    memset(ctx, 0, sizeof(pin_plugin_context_t));
    
    ctx->plugin = plugin;
    
    // Initialize API functions
    ////ctx->api.log_info = plugin_api_log_info;
    ////ctx->api.log_warn = plugin_api_log_warn;
    ////ctx->api.log_error = plugin_api_log_error;
    ////ctx->api.http_get = plugin_api_http_get;
    ////ctx->api.http_post = plugin_api_http_post;
    //ctx->api.config_get = plugin_api_config_get;
    //ctx->api.config_set = plugin_api_config_set;
    //ctx->api.config_delete = plugin_api_config_delete;
    //ctx->api.get_timestamp = plugin_api_get_timestamp;
    //ctx->api.get_time_string = plugin_api_get_time_string;
    //ctx->api.display_update_content = plugin_api_display_update_content;
    //ctx->api.display_set_color = plugin_api_display_set_color;
    //ctx->api.display_set_font_size = plugin_api_display_set_font_size;
    //ctx->api.schedule_update = plugin_api_schedule_update;
    //ctx->api.cancel_scheduled_update = plugin_api_cancel_scheduled_update;
    //ctx->api.emit_event = plugin_api_emit_event;
    //ctx->api.subscribe_event = plugin_api_subscribe_event;
    
    return ESP_OK;
}

static void pin_plugin_task_wrapper(void* pvParameters) {
    pin_plugin_t* plugin = (pin_plugin_t*)pvParameters;
    pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
    
    ESP_LOGI(TAG, "Plugin '%s' task started", plugin->metadata.name);
    
    while (plugin->running) {
        // Check resource limits
        if (pin_plugin_check_resources(ctx) != ESP_OK) {
            ESP_LOGW(TAG, "Plugin '%s' suspended due to resource limits", plugin->metadata.name);
            ctx->is_suspended = true;
            plugin->state = PLUGIN_STATE_SUSPENDED;
            vTaskDelay(pdMS_TO_TICKS(60000));  // Sleep for 1 minute
            continue;
        }
        
        // Call plugin update function
        if (plugin->update) {
            esp_err_t ret = plugin->update(ctx);
            if (ret != ESP_OK) {
                plugin->error_count++;
                ctx->stats.error_count++;
                
                if (plugin->error_count >= PIN_PLUGIN_MAX_ERRORS) {
                    ESP_LOGE(TAG, "Plugin '%s' disabled due to too many errors", plugin->metadata.name);
                    plugin->enabled = false;
                    plugin->state = PLUGIN_STATE_ERROR;
                    break;
                }
            } else {
                plugin->error_count = 0;  // Reset error count on success
                ctx->stats.update_count++;
            }
        }
        
        // Sleep according to configured update interval
        uint32_t interval = plugin->config.update_interval > 0 ? plugin->config.update_interval : 60;
        vTaskDelay(pdMS_TO_TICKS(interval * 1000));
    }
    
    ESP_LOGI(TAG, "Plugin '%s' task stopped", plugin->metadata.name);
    plugin->plugin_task = NULL;
    vTaskDelete(NULL);
}

static void pin_plugin_manager_task(void* pvParameters) {
    pin_plugin_message_t message;
    
    ESP_LOGI(TAG, "Plugin manager task started");
    
    while (1) {
        // Process messages
        if (xQueueReceive(g_plugin_manager.message_queue, &message, pdMS_TO_TICKS(1000)) == pdTRUE) {
            switch (message.type) {
                case PLUGIN_MSG_ENABLE:
                    pin_plugin_enable(message.plugin_name, true);
                    break;
                case PLUGIN_MSG_DISABLE:
                    pin_plugin_enable(message.plugin_name, false);
                    break;
                case PLUGIN_MSG_CONFIG_CHANGED:
                    //pin_plugin_set_config(message.plugin_name, message.key, message.value);
                    break;
                case PLUGIN_MSG_SHUTDOWN:
                    ESP_LOGI(TAG, "Plugin manager shutting down");
                    goto exit;
                default:
                    break;
            }
        }
        
        // Periodic maintenance tasks
        // TODO: Add garbage collection, health checks, etc.
    }
    
exit:
    ESP_LOGI(TAG, "Plugin manager task ended");
    vTaskDelete(NULL);
}

static esp_err_t pin_plugin_check_resources(pin_plugin_context_t* ctx) {
    // Check memory usage
    if (ctx->stats.memory_used > ctx->plugin->config.memory_limit) {
        ESP_LOGW(TAG, "Plugin '%s' memory limit exceeded: %d/%d", 
                 ctx->plugin->metadata.name, ctx->stats.memory_used, ctx->plugin->config.memory_limit);
        return ESP_ERR_NO_MEM;
    }
    
    // Check API call rate
    uint64_t now = esp_timer_get_time() / 1000;  // Convert to milliseconds
    if (now - 0 > 60000) {  // Reset every minute
        ctx->stats.api_calls_count = 0;
        
    }
    
    if (ctx->stats.api_calls_count > ctx->plugin->config.api_rate_limit) {
        ESP_LOGW(TAG, "Plugin '%s' API rate limit exceeded: %d/%d", 
                 ctx->plugin->metadata.name, ctx->stats.api_calls_count, ctx->plugin->config.api_rate_limit);
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

esp_err_t pin_plugin_validate(pin_plugin_t* plugin) {
    if (!plugin) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check required fields
    if (!plugin->metadata.name || strlen(plugin->metadata.name) == 0) {
        ESP_LOGE(TAG, "Plugin name is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!plugin->metadata.version || strlen(plugin->metadata.version) == 0) {
        ESP_LOGE(TAG, "Plugin version is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Check memory limit
    if (plugin->config.memory_limit == 0) {
        plugin->config.memory_limit = PIN_PLUGIN_DEFAULT_MEMORY_LIMIT;
    }
    
    if (plugin->config.memory_limit > PIN_PLUGIN_DEFAULT_MEMORY_LIMIT * 4) {
        ESP_LOGW(TAG, "Plugin memory limit exceeds recommended size: %d", plugin->config.memory_limit);
    }
    
    // Check update interval
    if (plugin->config.update_interval == 0) {
        plugin->config.update_interval = 60; // Default 60 seconds
    }
    
    if (plugin->config.update_interval < 10) {
        ESP_LOGW(TAG, "Plugin update interval too short: %d seconds", plugin->config.update_interval);
    }
    
    // Check required callback functions
    if (!plugin->init) {
        ESP_LOGE(TAG, "Plugin init function is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Plugin '%s' validation passed", plugin->metadata.name);
    return ESP_OK;
}

void* pin_plugin_malloc(pin_plugin_context_t* ctx, size_t size) {
    if (!ctx || size == 0) {
        return NULL;
    }
    
    if (ctx->stats.memory_used + size > ctx->plugin->config.memory_limit) {
        ESP_LOGW(TAG, "Plugin '%s' memory allocation failed: would exceed limit", 
                 ctx->plugin->metadata.name);
        return NULL;
    }
    
    void* ptr = malloc(size);
    if (ptr) {
        ctx->stats.memory_used += size;
        ctx->stats.memory_peak = ctx->stats.memory_peak > ctx->stats.memory_used ? 
                                 ctx->stats.memory_peak : ctx->stats.memory_used;
    }
    
    return ptr;
}

void pin_plugin_free(pin_plugin_context_t* ctx, void* ptr, size_t size) {
    if (ptr && ctx && size > 0) {
        free(ptr);
        if (ctx->stats.memory_used >= size) {
            ctx->stats.memory_used -= size;
        } else {
            ctx->stats.memory_used = 0;
        }
    }
}

// API Implementation functions
static esp_err_t plugin_api_log_info(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_INFO, tag, format, args);
    va_end(args);
    return ESP_OK;
}

static esp_err_t plugin_api_log_warn(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_WARN, tag, format, args);
    va_end(args);
    return ESP_OK;
}

static esp_err_t plugin_api_log_error(const char* tag, const char* format, ...) {
    va_list args;
    va_start(args, format);
    esp_log_writev(ESP_LOG_ERROR, tag, format, args);
    va_end(args);
    return ESP_OK;
}

static uint64_t plugin_api_get_timestamp(void) {
    return esp_timer_get_time() / 1000; // Convert to milliseconds
}

static esp_err_t plugin_api_get_time_string(char* time_str, size_t max_len, const char* format) {
    if (!time_str || !format) {
        return ESP_ERR_INVALID_ARG;
    }
    
    time_t now = time(NULL);
    struct tm* timeinfo = localtime(&now);
    
    if (strftime(time_str, max_len, format, timeinfo) == 0) {
        return ESP_ERR_INVALID_SIZE;
    }
    
    return ESP_OK;
}

// Stub implementations for other API functions
static esp_err_t plugin_api_http_get(const char* url, char* response, size_t max_len) {
    if (!url || !response || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Simple domain whitelist for security
    const char* allowed_domains[] = {
        "api.github.com",
        "httpbin.org",
        "jsonplaceholder.typicode.com",
        NULL
    };
    
    // Extract domain from URL
    const char* domain_start = strstr(url, "://");
    if (!domain_start) {
        ESP_LOGE(TAG, "Invalid URL format");
        return ESP_ERR_INVALID_ARG;
    }
    domain_start += 3; // Skip "://"
    
    // Check if domain is whitelisted
    bool domain_allowed = false;
    for (int i = 0; allowed_domains[i] != NULL; i++) {
        if (strncmp(domain_start, allowed_domains[i], strlen(allowed_domains[i])) == 0) {
            domain_allowed = true;
            break;
        }
    }
    
    if (!domain_allowed) {
        ESP_LOGW(TAG, "Domain not in whitelist: %s", url);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = 5000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int content_length = esp_http_client_read(client, response, max_len - 1);
        if (content_length >= 0) {
            response[content_length] = '\0';
        } else {
            err = ESP_FAIL;
        }
    }
    
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t plugin_api_http_post(const char* url, const char* data, char* response, size_t max_len) {
    if (!url || !data || !response || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Use same domain whitelist as GET
    const char* allowed_domains[] = {
        "api.github.com",
        "httpbin.org", 
        "jsonplaceholder.typicode.com",
        NULL
    };
    
    // Extract and validate domain
    const char* domain_start = strstr(url, "://");
    if (!domain_start) {
        return ESP_ERR_INVALID_ARG;
    }
    domain_start += 3;
    
    bool domain_allowed = false;
    for (int i = 0; allowed_domains[i] != NULL; i++) {
        if (strncmp(domain_start, allowed_domains[i], strlen(allowed_domains[i])) == 0) {
            domain_allowed = true;
            break;
        }
    }
    
    if (!domain_allowed) {
        ESP_LOGW(TAG, "Domain not in whitelist for POST: %s", url);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .timeout_ms = 10000,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        return ESP_ERR_NO_MEM;
    }
    
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, data, strlen(data));
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int content_length = esp_http_client_read(client, response, max_len - 1);
        if (content_length >= 0) {
            response[content_length] = '\0';
        } else {
            err = ESP_FAIL;
        }
    }
    
    esp_http_client_cleanup(client);
    return err;
}

static esp_err_t plugin_api_config_get(const char* key, char* value, size_t max_len) {
    if (!key || !value || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get current plugin context to create scoped key
    pin_plugin_context_t* ctx = (pin_plugin_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, 0);
    if (!ctx || !ctx->plugin) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char scoped_key[128];
    snprintf(scoped_key, sizeof(scoped_key), "plugin_%s_%s", ctx->plugin->metadata.name, key);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("plugins", NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    size_t required_size = max_len;
    err = nvs_get_str(nvs_handle, scoped_key, value, &required_size);
    nvs_close(nvs_handle);
    
    return err;
}

static esp_err_t plugin_api_config_set(const char* key, const char* value) {
    if (!key || !value) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get current plugin context to create scoped key
    pin_plugin_context_t* ctx = (pin_plugin_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, 0);
    if (!ctx || !ctx->plugin) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char scoped_key[128];
    snprintf(scoped_key, sizeof(scoped_key), "plugin_%s_%s", ctx->plugin->metadata.name, key);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("plugins", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_set_str(nvs_handle, scoped_key, value);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t plugin_api_config_delete(const char* key) {
    if (!key) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // Get current plugin context to create scoped key
    pin_plugin_context_t* ctx = (pin_plugin_context_t*)pvTaskGetThreadLocalStoragePointer(NULL, 0);
    if (!ctx || !ctx->plugin) {
        return ESP_ERR_INVALID_STATE;
    }
    
    char scoped_key[128];
    snprintf(scoped_key, sizeof(scoped_key), "plugin_%s_%s", ctx->plugin->metadata.name, key);
    
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open("plugins", NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        return err;
    }
    
    err = nvs_erase_key(nvs_handle, scoped_key);
    if (err == ESP_OK) {
        err = nvs_commit(nvs_handle);
    }
    
    nvs_close(nvs_handle);
    return err;
}

static esp_err_t plugin_api_display_update_content(const char* content) {
    // TODO: Implement display content update
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_display_set_color(uint8_t color) {
    // TODO: Implement display color setting
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_display_set_font_size(uint8_t font_size) {
    // TODO: Implement display font size setting
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_schedule_update(uint32_t delay_seconds) {
    // TODO: Implement plugin update scheduling
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_cancel_scheduled_update(void) {
    // TODO: Implement plugin update cancellation
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_emit_event(const char* event_name, const char* data) {
    // TODO: Implement event emission
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t plugin_api_subscribe_event(const char* event_name, void (*callback)(const char* data)) {
    // TODO: Implement event subscription
    return ESP_ERR_NOT_SUPPORTED;
}
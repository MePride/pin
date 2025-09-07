/**
 * @file pin_weather_plugin.c
 * @brief Weather plugin for Pin device using OpenWeatherMap API
 */

#include <string.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "pin_plugin.h"
#include "esp_log.h"
#include "cJSON.h"

static const char* TAG = "WEATHER_PLUGIN";

// Weather data structure
typedef struct {
    char location[64];
    char condition[32];
    char description[128];
    float temperature;
    float feels_like;
    int humidity;
    float pressure;
    float wind_speed;
    int wind_direction;
    char icon[8];
    uint32_t last_update;
    bool data_valid;
} weather_data_t;

static weather_data_t g_weather_data = {0};

// Forward declarations
static esp_err_t fetch_weather_data(pin_plugin_context_t* ctx);
static esp_err_t parse_weather_response(const char* json_response);
static const char* get_weather_emoji(const char* icon);
static void format_weather_display(char* output, size_t max_len);

// Plugin lifecycle functions
static esp_err_t weather_init(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Weather plugin initialized");
    
    // Set default configuration if not exists
    char config_value[64];
    if (ctx->api.config_get("api_key", config_value, sizeof(config_value)) != ESP_OK) {
        // Set placeholder - user needs to configure their own API key
        ctx->api.config_set("api_key", "YOUR_OPENWEATHERMAP_API_KEY");
    }
    
    if (ctx->api.config_get("city", config_value, sizeof(config_value)) != ESP_OK) {
        ctx->api.config_set("city", "London,UK");
    }
    
    if (ctx->api.config_get("units", config_value, sizeof(config_value)) != ESP_OK) {
        ctx->api.config_set("units", "metric");  // metric, imperial, or kelvin
    }
    
    return ESP_OK;
}

static esp_err_t weather_start(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Weather plugin started");
    
    // Fetch initial weather data
    esp_err_t ret = fetch_weather_data(ctx);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "Failed to fetch initial weather data: %s", esp_err_to_name(ret));
    }
    
    return ESP_OK;
}

static esp_err_t weather_update(pin_plugin_context_t* ctx) {
    // Check if we need to update (every 10 minutes)
    time_t now = time(NULL);
    if (g_weather_data.data_valid && (now - g_weather_data.last_update) < 600) {
        ESP_LOGD(TAG, "Weather data still fresh, skipping update");
        return ESP_OK;
    }
    
    ESP_LOGI(TAG, "Updating weather data");
    return fetch_weather_data(ctx);
}

static esp_err_t weather_render(pin_plugin_context_t* ctx, pin_widget_region_t* region) {
    if (!region) {
        return ESP_ERR_INVALID_ARG;
    }
    
    char weather_display[256];
    format_weather_display(weather_display, sizeof(weather_display));
    
    // Update region content
    if (region->content) {
        free(region->content);
    }
    region->content = strdup(weather_display);
    region->dirty = true;
    
    return ESP_OK;
}

static esp_err_t weather_config_changed(pin_plugin_context_t* ctx, const char* key, const char* value) {
    ESP_LOGI(TAG, "Configuration changed: %s = %s", key, value);
    
    // If location or API key changed, invalidate current data
    if (strcmp(key, "city") == 0 || strcmp(key, "api_key") == 0) {
        g_weather_data.data_valid = false;
        g_weather_data.last_update = 0;
        
        // Trigger immediate update
        fetch_weather_data(ctx);
    }
    
    return ESP_OK;
}

static esp_err_t weather_cleanup(pin_plugin_context_t* ctx) {
    ESP_LOGI(TAG, "Weather plugin cleaned up");
    memset(&g_weather_data, 0, sizeof(weather_data_t));
    return ESP_OK;
}

// Private functions
static esp_err_t fetch_weather_data(pin_plugin_context_t* ctx) {
    char api_key[64];
    char city[64];
    char units[16];
    
    // Get configuration
    if (ctx->api.config_get("api_key", api_key, sizeof(api_key)) != ESP_OK) {
        ESP_LOGE(TAG, "No API key configured");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (ctx->api.config_get("city", city, sizeof(city)) != ESP_OK) {
        strcpy(city, "London,UK");
    }
    
    if (ctx->api.config_get("units", units, sizeof(units)) != ESP_OK) {
        strcpy(units, "metric");
    }
    
    // Check if API key is still placeholder
    if (strcmp(api_key, "YOUR_OPENWEATHERMAP_API_KEY") == 0) {
        ESP_LOGW(TAG, "Please configure your OpenWeatherMap API key");
        return ESP_ERR_INVALID_ARG;
    }
    
    // Build API URL
    char url[512];
    snprintf(url, sizeof(url), 
             "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=%s",
             city, api_key, units);
    
    char response[2048];
    esp_err_t ret = ctx->api.http_get(url, response, sizeof(response));
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "HTTP GET failed: %s", esp_err_to_name(ret));
        return ret;
    }
    
    ret = parse_weather_response(response);
    if (ret == ESP_OK) {
        g_weather_data.last_update = time(NULL);
        g_weather_data.data_valid = true;
        ESP_LOGI(TAG, "Weather data updated: %.1f°C in %s", 
                g_weather_data.temperature, g_weather_data.location);
    }
    
    return ret;
}

static esp_err_t parse_weather_response(const char* json_response) {
    cJSON *json = cJSON_Parse(json_response);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse weather JSON response");
        return ESP_FAIL;
    }
    
    // Parse location name
    cJSON *name = cJSON_GetObjectItem(json, "name");
    cJSON *sys = cJSON_GetObjectItem(json, "sys");
    if (cJSON_IsString(name)) {
        strncpy(g_weather_data.location, name->valuestring, sizeof(g_weather_data.location) - 1);
        
        // Add country code if available
        if (sys) {
            cJSON *country = cJSON_GetObjectItem(sys, "country");
            if (cJSON_IsString(country)) {
                strncat(g_weather_data.location, ", ", sizeof(g_weather_data.location) - strlen(g_weather_data.location) - 1);
                strncat(g_weather_data.location, country->valuestring, sizeof(g_weather_data.location) - strlen(g_weather_data.location) - 1);
            }
        }
    }
    
    // Parse main weather data
    cJSON *main = cJSON_GetObjectItem(json, "main");
    if (main) {
        cJSON *temp = cJSON_GetObjectItem(main, "temp");
        cJSON *feels_like = cJSON_GetObjectItem(main, "feels_like");
        cJSON *humidity = cJSON_GetObjectItem(main, "humidity");
        cJSON *pressure = cJSON_GetObjectItem(main, "pressure");
        
        if (cJSON_IsNumber(temp)) g_weather_data.temperature = (float)temp->valuedouble;
        if (cJSON_IsNumber(feels_like)) g_weather_data.feels_like = (float)feels_like->valuedouble;
        if (cJSON_IsNumber(humidity)) g_weather_data.humidity = humidity->valueint;
        if (cJSON_IsNumber(pressure)) g_weather_data.pressure = (float)pressure->valuedouble;
    }
    
    // Parse weather condition
    cJSON *weather = cJSON_GetObjectItem(json, "weather");
    if (cJSON_IsArray(weather) && cJSON_GetArraySize(weather) > 0) {
        cJSON *weather_item = cJSON_GetArrayItem(weather, 0);
        cJSON *condition = cJSON_GetObjectItem(weather_item, "main");
        cJSON *description = cJSON_GetObjectItem(weather_item, "description");
        cJSON *icon = cJSON_GetObjectItem(weather_item, "icon");
        
        if (cJSON_IsString(condition)) {
            strncpy(g_weather_data.condition, condition->valuestring, sizeof(g_weather_data.condition) - 1);
        }
        if (cJSON_IsString(description)) {
            strncpy(g_weather_data.description, description->valuestring, sizeof(g_weather_data.description) - 1);
        }
        if (cJSON_IsString(icon)) {
            strncpy(g_weather_data.icon, icon->valuestring, sizeof(g_weather_data.icon) - 1);
        }
    }
    
    // Parse wind data
    cJSON *wind = cJSON_GetObjectItem(json, "wind");
    if (wind) {
        cJSON *speed = cJSON_GetObjectItem(wind, "speed");
        cJSON *deg = cJSON_GetObjectItem(wind, "deg");
        
        if (cJSON_IsNumber(speed)) g_weather_data.wind_speed = (float)speed->valuedouble;
        if (cJSON_IsNumber(deg)) g_weather_data.wind_direction = deg->valueint;
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

static const char* get_weather_emoji(const char* icon) {
    if (!icon || strlen(icon) < 2) return "🌍";
    
    switch (icon[0]) {
        case '01': return icon[2] == 'd' ? "☀️" : "🌙";  // clear sky
        case '02': return icon[2] == 'd' ? "⛅" : "🌙";  // few clouds
        case '03': return "☁️";   // scattered clouds
        case '04': return "☁️";   // broken clouds
        case '09': return "🌧️";   // shower rain
        case '10': return "🌦️";   // rain
        case '11': return "⛈️";   // thunderstorm
        case '13': return "❄️";   // snow
        case '50': return "🌫️";   // mist
        default: return "🌍";
    }
}

static void format_weather_display(char* output, size_t max_len) {
    if (!g_weather_data.data_valid) {
        snprintf(output, max_len, "Weather: No data");
        return;
    }
    
    const char* emoji = get_weather_emoji(g_weather_data.icon);
    
    // Format temperature based on value
    char temp_str[16];
    if (g_weather_data.temperature == (int)g_weather_data.temperature) {
        snprintf(temp_str, sizeof(temp_str), "%.0f°", g_weather_data.temperature);
    } else {
        snprintf(temp_str, sizeof(temp_str), "%.1f°", g_weather_data.temperature);
    }
    
    // Create compact display
    snprintf(output, max_len, "%s %s\n%s\n%s %d%%", 
             emoji, temp_str,
             g_weather_data.location,
             g_weather_data.description,
             g_weather_data.humidity);
}

// Weather plugin definition
pin_plugin_t weather_plugin = {
    .metadata = {
        .name = "weather",
        .version = "1.0.0",
        .author = "Pin Team",
        .description = "OpenWeatherMap weather display",
        .homepage = "https://openweathermap.org",
        .min_firmware_version = 100
    },
    .config = {
        .memory_limit = 8192,
        .update_interval = 600,  // Update every 10 minutes
        .api_rate_limit = 60,    // 60 API calls per hour max
        .auto_start = true,
        .persistent = true
    },
    .init = weather_init,
    .start = weather_start,
    .update = weather_update,
    .render = weather_render,
    .config_changed = weather_config_changed,
    .cleanup = weather_cleanup,
    .state = PLUGIN_STATE_UNLOADED,
    .enabled = false,
    .initialized = false,
    .running = false,
    .private_data = NULL
};
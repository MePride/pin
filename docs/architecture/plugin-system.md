# Pin 插件系统架构文档

## 概述

Pin 插件系统是整个生态的核心扩展机制，基于轻量级沙盒设计，允许开发者创建自定义功能模块，同时保证系统的稳定性和安全性。

## 设计原则

1. **安全隔离**: 每个插件运行在独立的沙盒环境中
2. **资源限制**: 严格控制内存使用和API调用频率
3. **生命周期管理**: 标准化的插件生命周期钩子
4. **API一致性**: 统一的插件开发接口
5. **热插拔支持**: 支持插件的动态加载和卸载
6. **社区友好**: 为未来的插件市场预留扩展能力

## 核心架构

### 插件基础结构

```c
// components/pin_plugin_api/pin_plugin.h
typedef struct pin_plugin pin_plugin_t;
typedef struct pin_plugin_context pin_plugin_context_t;

// 插件元信息
typedef struct {
    const char* name;           // 插件名称 (唯一标识)
    const char* version;        // 版本号 (语义化版本)
    const char* author;         // 作者信息
    const char* description;    // 插件描述
    const char* homepage;       // 主页链接
    uint32_t min_firmware_version; // 最小固件版本要求
} pin_plugin_metadata_t;

// 插件配置
typedef struct {
    uint32_t memory_limit;      // 内存限制 (bytes)
    uint32_t update_interval;   // 更新间隔 (seconds)
    uint32_t api_rate_limit;    // API调用频率限制 (calls/minute)
    bool auto_start;            // 是否自动启动
    bool persistent;            // 是否持久化运行
} pin_plugin_config_t;

// 插件主结构
typedef struct pin_plugin {
    pin_plugin_metadata_t metadata;
    pin_plugin_config_t config;
    
    // 生命周期回调函数
    esp_err_t (*init)(pin_plugin_context_t* ctx);
    esp_err_t (*start)(pin_plugin_context_t* ctx);
    esp_err_t (*update)(pin_plugin_context_t* ctx);
    esp_err_t (*render)(pin_plugin_context_t* ctx, pin_widget_region_t* region);
    esp_err_t (*config_changed)(pin_plugin_context_t* ctx, const char* key, const char* value);
    esp_err_t (*stop)(pin_plugin_context_t* ctx);
    esp_err_t (*cleanup)(pin_plugin_context_t* ctx);
    
    // 插件状态
    bool enabled;
    bool initialized;
    bool running;
    uint32_t last_update_time;
    uint32_t error_count;
    
    // 私有数据
    void* private_data;
    
    // 内部管理字段
    uint8_t plugin_id;
    TaskHandle_t plugin_task;
} pin_plugin_t;
```

### 插件运行上下文

```c
// 插件沙盒上下文
typedef struct pin_plugin_context {
    pin_plugin_t* plugin;               // 关联的插件
    pin_widget_region_t widget_region;  // 分配的显示区域
    
    // 系统API接口 (只能通过这些函数访问系统功能)
    struct {
        // 日志系统
        esp_err_t (*log_info)(const char* tag, const char* format, ...);
        esp_err_t (*log_warn)(const char* tag, const char* format, ...);
        esp_err_t (*log_error)(const char* tag, const char* format, ...);
        
        // 网络访问
        esp_err_t (*http_get)(const char* url, char* response, size_t max_len);
        esp_err_t (*http_post)(const char* url, const char* data, char* response, size_t max_len);
        
        // 配置存储
        esp_err_t (*config_get)(const char* key, char* value, size_t max_len);
        esp_err_t (*config_set)(const char* key, const char* value);
        esp_err_t (*config_delete)(const char* key);
        
        // 时间系统
        uint64_t (*get_timestamp)(void);
        esp_err_t (*get_time_string)(char* time_str, size_t max_len, const char* format);
        
        // 显示系统
        esp_err_t (*display_update_content)(const char* content);
        esp_err_t (*display_set_color)(pin_colors_t color);
        esp_err_t (*display_set_font_size)(uint8_t font_size);
        
        // 定时器
        esp_err_t (*schedule_update)(uint32_t delay_seconds);
        esp_err_t (*cancel_scheduled_update)(void);
        
        // 事件系统
        esp_err_t (*emit_event)(const char* event_name, const char* data);
        esp_err_t (*subscribe_event)(const char* event_name, void (*callback)(const char* data));
    } api;
    
    // 资源监控
    struct {
        uint32_t memory_used;           // 当前内存使用量
        uint32_t memory_peak;           // 内存使用峰值
        uint32_t api_calls_count;       // API调用次数
        uint32_t api_calls_last_reset;  // 上次重置API计数的时间
        uint32_t update_count;          // 更新次数
        uint32_t error_count;           // 错误次数
    } stats;
    
    // 沙盒状态
    bool is_suspended;                  // 是否被挂起
    bool is_blocked;                    // 是否被阻塞
    uint32_t suspension_reason;         // 挂起原因
} pin_plugin_context_t;
```

## 插件管理器

### 管理器核心结构

```c
// pin_plugin.c - 插件管理系统
#define PIN_MAX_PLUGINS 8
#define PIN_PLUGIN_DEFAULT_MEMORY_LIMIT (64 * 1024)   // 64KB
#define PIN_PLUGIN_API_RATE_LIMIT 100                 // 100 calls per minute
#define PIN_PLUGIN_MAX_ERRORS 5                       // 最大错误次数

typedef struct {
    pin_plugin_t* plugins[PIN_MAX_PLUGINS];           // 插件数组
    pin_plugin_context_t contexts[PIN_MAX_PLUGINS];   // 上下文数组
    uint8_t plugin_count;                             // 当前插件数量
    
    // 任务管理
    TaskHandle_t manager_task_handle;                 // 管理器任务句柄
    QueueHandle_t message_queue;                      // 消息队列
    SemaphoreHandle_t plugins_mutex;                  // 插件列表互斥锁
    
    // 系统状态
    bool plugins_enabled;                             // 插件系统开关
    bool auto_load_enabled;                           // 自动加载开关
    uint32_t last_gc_time;                           // 上次垃圾回收时间
} pin_plugin_manager_t;

// 全局插件管理器实例
static pin_plugin_manager_t g_plugin_manager = {0};
```

### 插件管理API

```c
// 插件系统初始化
esp_err_t pin_plugin_manager_init(void) {
    // 初始化互斥锁和队列
    g_plugin_manager.plugins_mutex = xSemaphoreCreateMutex();
    g_plugin_manager.message_queue = xQueueCreate(10, sizeof(pin_plugin_message_t));
    
    // 创建管理器任务
    xTaskCreate(pin_plugin_manager_task, "plugin_mgr", 4096, NULL, 5, 
                &g_plugin_manager.manager_task_handle);
    
    g_plugin_manager.plugins_enabled = true;
    
    ESP_LOGI(TAG, "Plugin manager initialized");
    return ESP_OK;
}

// 注册插件
esp_err_t pin_plugin_register(pin_plugin_t* plugin) {
    if (g_plugin_manager.plugin_count >= PIN_MAX_PLUGINS) {
        return ESP_ERR_NO_MEM;
    }
    
    if (xSemaphoreTake(g_plugin_manager.plugins_mutex, pdMS_TO_TICKS(1000))) {
        uint8_t plugin_id = g_plugin_manager.plugin_count++;
        plugin->plugin_id = plugin_id;
        
        g_plugin_manager.plugins[plugin_id] = plugin;
        
        // 初始化插件上下文
        pin_plugin_init_context(&g_plugin_manager.contexts[plugin_id], plugin);
        
        xSemaphoreGive(g_plugin_manager.plugins_mutex);
        
        ESP_LOGI(TAG, "Plugin '%s' registered with ID %d", plugin->metadata.name, plugin_id);
        return ESP_OK;
    }
    
    return ESP_ERR_TIMEOUT;
}

// 启动插件
esp_err_t pin_plugin_enable(const char* plugin_name, bool enable) {
    pin_plugin_t* plugin = pin_plugin_find_by_name(plugin_name);
    if (plugin == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    
    if (enable && !plugin->enabled) {
        // 启动插件
        pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
        
        if (plugin->init && plugin->init(ctx) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to initialize plugin '%s'", plugin_name);
            return ESP_FAIL;
        }
        
        if (plugin->start && plugin->start(ctx) != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start plugin '%s'", plugin_name);
            return ESP_FAIL;
        }
        
        plugin->enabled = true;
        plugin->running = true;
        
        // 创建插件任务
        char task_name[32];
        snprintf(task_name, sizeof(task_name), "plugin_%s", plugin_name);
        xTaskCreate(pin_plugin_task_wrapper, task_name, 4096, plugin, 4, &plugin->plugin_task);
        
    } else if (!enable && plugin->enabled) {
        // 停止插件
        plugin->enabled = false;
        plugin->running = false;
        
        if (plugin->plugin_task) {
            vTaskDelete(plugin->plugin_task);
            plugin->plugin_task = NULL;
        }
        
        pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
        if (plugin->stop) {
            plugin->stop(ctx);
        }
    }
    
    return ESP_OK;
}

// 更新插件配置
esp_err_t pin_plugin_set_config(const char* plugin_name, const char* key, const char* value) {
    pin_plugin_t* plugin = pin_plugin_find_by_name(plugin_name);
    if (plugin == NULL) {
        return ESP_ERR_NOT_FOUND;
    }
    
    pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
    
    // 保存配置到NVS
    char config_key[64];
    snprintf(config_key, sizeof(config_key), "plugin.%s.%s", plugin_name, key);
    ctx->api.config_set(config_key, value);
    
    // 通知插件配置变更
    if (plugin->config_changed) {
        plugin->config_changed(ctx, key, value);
    }
    
    return ESP_OK;
}
```

## 插件开发接口

### 插件生命周期

```c
// 插件生命周期状态机
typedef enum {
    PLUGIN_STATE_UNLOADED,    // 未加载
    PLUGIN_STATE_LOADED,      // 已加载但未初始化  
    PLUGIN_STATE_INITIALIZED, // 已初始化
    PLUGIN_STATE_RUNNING,     // 正在运行
    PLUGIN_STATE_SUSPENDED,   // 被挂起
    PLUGIN_STATE_ERROR        // 错误状态
} pin_plugin_state_t;

// 插件任务包装器
static void pin_plugin_task_wrapper(void* pvParameters) {
    pin_plugin_t* plugin = (pin_plugin_t*)pvParameters;
    pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
    
    ESP_LOGI(TAG, "Plugin '%s' task started", plugin->metadata.name);
    
    while (plugin->running) {
        // 检查资源限制
        if (pin_plugin_check_resources(ctx) != ESP_OK) {
            ESP_LOGW(TAG, "Plugin '%s' suspended due to resource limits", plugin->metadata.name);
            ctx->is_suspended = true;
            vTaskDelay(pdMS_TO_TICKS(60000));  // 暂停1分钟
            continue;
        }
        
        // 调用插件更新函数
        if (plugin->update) {
            esp_err_t ret = plugin->update(ctx);
            if (ret != ESP_OK) {
                plugin->error_count++;
                ctx->stats.error_count++;
                
                if (plugin->error_count >= PIN_PLUGIN_MAX_ERRORS) {
                    ESP_LOGE(TAG, "Plugin '%s' disabled due to too many errors", plugin->metadata.name);
                    plugin->enabled = false;
                    break;
                }
            } else {
                plugin->error_count = 0;  // 重置错误计数
            }
            
            ctx->stats.update_count++;
        }
        
        // 根据配置的更新间隔休眠
        vTaskDelay(pdMS_TO_TICKS(plugin->config.update_interval * 1000));
    }
    
    ESP_LOGI(TAG, "Plugin '%s' task stopped", plugin->metadata.name);
    plugin->plugin_task = NULL;
    vTaskDelete(NULL);
}
```

### 资源监控和限制

```c
// 资源检查函数
esp_err_t pin_plugin_check_resources(pin_plugin_context_t* ctx) {
    // 检查内存使用
    if (ctx->stats.memory_used > ctx->plugin->config.memory_limit) {
        ESP_LOGW(TAG, "Plugin '%s' memory limit exceeded: %d/%d", 
                 ctx->plugin->metadata.name, ctx->stats.memory_used, ctx->plugin->config.memory_limit);
        return ESP_ERR_NO_MEM;
    }
    
    // 检查API调用频率
    uint64_t now = esp_timer_get_time() / 1000;  // 转换为毫秒
    if (now - ctx->stats.api_calls_last_reset > 60000) {  // 1分钟重置一次
        ctx->stats.api_calls_count = 0;
        ctx->stats.api_calls_last_reset = now;
    }
    
    if (ctx->stats.api_calls_count > ctx->plugin->config.api_rate_limit) {
        ESP_LOGW(TAG, "Plugin '%s' API rate limit exceeded: %d/%d", 
                 ctx->plugin->metadata.name, ctx->stats.api_calls_count, ctx->plugin->config.api_rate_limit);
        return ESP_ERR_INVALID_STATE;
    }
    
    return ESP_OK;
}

// 内存使用跟踪
void* pin_plugin_malloc(pin_plugin_context_t* ctx, size_t size) {
    if (ctx->stats.memory_used + size > ctx->plugin->config.memory_limit) {
        return NULL;  // 超出内存限制
    }
    
    void* ptr = malloc(size);
    if (ptr) {
        ctx->stats.memory_used += size;
        ctx->stats.memory_peak = MAX(ctx->stats.memory_peak, ctx->stats.memory_used);
    }
    
    return ptr;
}

void pin_plugin_free(pin_plugin_context_t* ctx, void* ptr, size_t size) {
    if (ptr) {
        free(ptr);
        ctx->stats.memory_used -= size;
    }
}
```

## 内置插件示例

### 时钟插件

```c
// plugins/clock/clock_plugin.c
#include "pin_plugin.h"

// 时钟插件私有数据
typedef struct {
    char time_format[32];       // 时间格式字符串
    bool show_seconds;          // 是否显示秒
    bool use_24hour;           // 是否使用24小时制
    uint8_t font_size;         // 字体大小
} clock_private_data_t;

// 时钟插件初始化
static esp_err_t clock_init(pin_plugin_context_t* ctx) {
    // 分配私有数据
    clock_private_data_t* data = pin_plugin_malloc(ctx, sizeof(clock_private_data_t));
    if (!data) {
        return ESP_ERR_NO_MEM;
    }
    
    // 初始化默认配置
    strcpy(data->time_format, "%H:%M");
    data->show_seconds = false;
    data->use_24hour = true;
    data->font_size = 24;
    
    ctx->plugin->private_data = data;
    
    // 从配置中读取设置
    char config_value[64];
    if (ctx->api.config_get("time_format", config_value, sizeof(config_value)) == ESP_OK) {
        strncpy(data->time_format, config_value, sizeof(data->time_format) - 1);
    }
    
    ctx->api.log_info("CLOCK", "Clock plugin initialized");
    return ESP_OK;
}

// 时钟插件更新
static esp_err_t clock_update(pin_plugin_context_t* ctx) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    // 获取当前时间字符串
    char time_str[32];
    esp_err_t ret = ctx->api.get_time_string(time_str, sizeof(time_str), data->time_format);
    if (ret != ESP_OK) {
        ctx->api.log_error("CLOCK", "Failed to get time string");
        return ret;
    }
    
    // 更新显示内容
    ctx->api.display_update_content(time_str);
    
    return ESP_OK;
}

// 时钟插件渲染
static esp_err_t clock_render(pin_plugin_context_t* ctx, pin_widget_region_t* region) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    // 设置显示属性
    ctx->api.display_set_font_size(data->font_size);
    
    pin_colors_t color = {.black = 255};  // 黑色文字
    ctx->api.display_set_color(color);
    
    return ESP_OK;
}

// 时钟插件配置变更
static esp_err_t clock_config_changed(pin_plugin_context_t* ctx, const char* key, const char* value) {
    clock_private_data_t* data = (clock_private_data_t*)ctx->plugin->private_data;
    
    if (strcmp(key, "time_format") == 0) {
        strncpy(data->time_format, value, sizeof(data->time_format) - 1);
    } else if (strcmp(key, "font_size") == 0) {
        data->font_size = atoi(value);
    }
    
    ctx->api.log_info("CLOCK", "Configuration updated: %s = %s", key, value);
    return ESP_OK;
}

// 时钟插件清理
static esp_err_t clock_cleanup(pin_plugin_context_t* ctx) {
    if (ctx->plugin->private_data) {
        pin_plugin_free(ctx, ctx->plugin->private_data, sizeof(clock_private_data_t));
        ctx->plugin->private_data = NULL;
    }
    
    ctx->api.log_info("CLOCK", "Clock plugin cleaned up");
    return ESP_OK;
}

// 时钟插件定义
pin_plugin_t clock_plugin = {
    .metadata = {
        .name = "clock",
        .version = "1.0.0",
        .author = "Pin Team",
        .description = "Simple digital clock widget",
        .homepage = "https://github.com/pin-project/plugins/clock",
        .min_firmware_version = 0x010000  // v1.0.0
    },
    .config = {
        .memory_limit = 4096,     // 4KB
        .update_interval = 60,    // 60秒更新一次
        .api_rate_limit = 10,     // 每分钟最多10次API调用
        .auto_start = true,
        .persistent = true
    },
    .init = clock_init,
    .start = NULL,
    .update = clock_update,
    .render = clock_render,
    .config_changed = clock_config_changed,
    .stop = NULL,
    .cleanup = clock_cleanup,
    .enabled = false,
    .initialized = false,
    .running = false,
    .private_data = NULL
};
```

### 天气插件

```c
// plugins/weather/weather_plugin.c
#include "pin_plugin.h"
#include "cJSON.h"

typedef struct {
    char api_key[64];           // API密钥
    char city[32];              // 城市名称
    char weather_data[256];     // 天气数据缓存
    uint32_t last_fetch_time;   // 上次获取时间
    uint32_t fetch_interval;    // 获取间隔(秒)
} weather_private_data_t;

static esp_err_t weather_init(pin_plugin_context_t* ctx) {
    weather_private_data_t* data = pin_plugin_malloc(ctx, sizeof(weather_private_data_t));
    if (!data) return ESP_ERR_NO_MEM;
    
    // 初始化默认值
    strcpy(data->city, "Beijing");
    data->fetch_interval = 1800;  // 30分钟
    data->last_fetch_time = 0;
    
    // 从配置读取API密钥和城市
    ctx->api.config_get("api_key", data->api_key, sizeof(data->api_key));
    ctx->api.config_get("city", data->city, sizeof(data->city));
    
    ctx->plugin->private_data = data;
    
    ctx->api.log_info("WEATHER", "Weather plugin initialized for city: %s", data->city);
    return ESP_OK;
}

static esp_err_t weather_update(pin_plugin_context_t* ctx) {
    weather_private_data_t* data = (weather_private_data_t*)ctx->plugin->private_data;
    
    uint64_t now = ctx->api.get_timestamp();
    
    // 检查是否需要更新天气数据
    if (now - data->last_fetch_time < data->fetch_interval) {
        return ESP_OK;  // 还不需要更新
    }
    
    // 构建天气API URL
    char url[256];
    snprintf(url, sizeof(url), 
             "http://api.openweathermap.org/data/2.5/weather?q=%s&appid=%s&units=metric",
             data->city, data->api_key);
    
    // 获取天气数据
    char response[512];
    esp_err_t ret = ctx->api.http_get(url, response, sizeof(response));
    if (ret != ESP_OK) {
        ctx->api.log_error("WEATHER", "Failed to fetch weather data");
        return ret;
    }
    
    // 解析JSON响应
    cJSON* json = cJSON_Parse(response);
    if (!json) {
        ctx->api.log_error("WEATHER", "Failed to parse weather JSON");
        return ESP_FAIL;
    }
    
    cJSON* main = cJSON_GetObjectItem(json, "main");
    cJSON* weather = cJSON_GetArrayItem(cJSON_GetObjectItem(json, "weather"), 0);
    
    if (main && weather) {
        double temp = cJSON_GetObjectItem(main, "temp")->valuedouble;
        const char* desc = cJSON_GetObjectItem(weather, "description")->valuestring;
        
        // 格式化显示文本
        snprintf(data->weather_data, sizeof(data->weather_data), 
                 "%.1f°C\\n%s", temp, desc);
        
        data->last_fetch_time = now;
        
        // 更新显示
        ctx->api.display_update_content(data->weather_data);
        
        ctx->api.log_info("WEATHER", "Weather updated: %.1f°C, %s", temp, desc);
    }
    
    cJSON_Delete(json);
    return ESP_OK;
}

// 天气插件定义
pin_plugin_t weather_plugin = {
    .metadata = {
        .name = "weather",
        .version = "1.0.0",
        .author = "Pin Team", 
        .description = "Weather information widget",
        .homepage = "https://github.com/pin-project/plugins/weather"
    },
    .config = {
        .memory_limit = 8192,     // 8KB
        .update_interval = 1800,  // 30分钟
        .api_rate_limit = 5,      // 每分钟最多5次API调用
        .auto_start = false,
        .persistent = true
    },
    .init = weather_init,
    .update = weather_update,
    .render = NULL,
    .config_changed = NULL,
    .cleanup = NULL,
    .enabled = false
};
```

## 插件开发工具

### 插件脚手架生成器

```c
// tools/plugin_builder/plugin_template.h
#define PLUGIN_TEMPLATE_H \
"#include \"pin_plugin.h\"\\n" \
"\\n" \
"typedef struct {\\n" \
"    // Plugin private data\\n" \
"} %s_private_data_t;\\n" \
"\\n" \
"static esp_err_t %s_init(pin_plugin_context_t* ctx) {\\n" \
"    ctx->api.log_info(\"%s\", \"Plugin initialized\");\\n" \
"    return ESP_OK;\\n" \
"}\\n" \
"\\n" \
"static esp_err_t %s_update(pin_plugin_context_t* ctx) {\\n" \
"    // Plugin update logic here\\n" \
"    return ESP_OK;\\n" \
"}\\n" \
"\\n" \
"pin_plugin_t %s_plugin = {\\n" \
"    .metadata = {\\n" \
"        .name = \"%s\",\\n" \
"        .version = \"1.0.0\",\\n" \
"        .author = \"Your Name\",\\n" \
"        .description = \"Plugin description\"\\n" \
"    },\\n" \
"    .config = {\\n" \
"        .memory_limit = 4096,\\n" \
"        .update_interval = 60,\\n" \
"        .api_rate_limit = 10\\n" \
"    },\\n" \
"    .init = %s_init,\\n" \
"    .update = %s_update\\n" \
"};\\n"
```

### 插件验证工具

```c
// 插件验证函数
esp_err_t pin_plugin_validate(pin_plugin_t* plugin) {
    if (!plugin) {
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查必需字段
    if (!plugin->metadata.name || strlen(plugin->metadata.name) == 0) {
        ESP_LOGE(TAG, "Plugin name is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    if (!plugin->metadata.version || strlen(plugin->metadata.version) == 0) {
        ESP_LOGE(TAG, "Plugin version is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    // 检查内存限制
    if (plugin->config.memory_limit > PIN_PLUGIN_DEFAULT_MEMORY_LIMIT) {
        ESP_LOGW(TAG, "Plugin memory limit exceeds recommended size: %d", plugin->config.memory_limit);
    }
    
    // 检查更新间隔
    if (plugin->config.update_interval < 10) {
        ESP_LOGW(TAG, "Plugin update interval too short: %d seconds", plugin->config.update_interval);
    }
    
    // 检查必需的回调函数
    if (!plugin->init) {
        ESP_LOGE(TAG, "Plugin init function is required");
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Plugin '%s' validation passed", plugin->metadata.name);
    return ESP_OK;
}
```

## 插件调试和诊断

### 插件状态监控

```c
// 插件统计信息
typedef struct {
    char plugin_name[32];
    pin_plugin_state_t state;
    uint32_t memory_used;
    uint32_t memory_peak;
    uint32_t update_count;
    uint32_t error_count;
    uint32_t api_calls;
    uint32_t uptime_seconds;
    bool is_suspended;
} pin_plugin_stats_t;

// 获取插件统计信息
esp_err_t pin_plugin_get_stats(const char* plugin_name, pin_plugin_stats_t* stats) {
    pin_plugin_t* plugin = pin_plugin_find_by_name(plugin_name);
    if (!plugin || !stats) {
        return ESP_ERR_INVALID_ARG;
    }
    
    pin_plugin_context_t* ctx = &g_plugin_manager.contexts[plugin->plugin_id];
    
    strncpy(stats->plugin_name, plugin->metadata.name, sizeof(stats->plugin_name) - 1);
    stats->memory_used = ctx->stats.memory_used;
    stats->memory_peak = ctx->stats.memory_peak;
    stats->update_count = ctx->stats.update_count;
    stats->error_count = ctx->stats.error_count;
    stats->api_calls = ctx->stats.api_calls_count;
    stats->is_suspended = ctx->is_suspended;
    
    return ESP_OK;
}

// 插件系统诊断
esp_err_t pin_plugin_system_diagnostic(void) {
    ESP_LOGI(TAG, "=== Plugin System Diagnostic ===");
    ESP_LOGI(TAG, "Total plugins: %d/%d", g_plugin_manager.plugin_count, PIN_MAX_PLUGINS);
    ESP_LOGI(TAG, "Plugins enabled: %s", g_plugin_manager.plugins_enabled ? "YES" : "NO");
    
    for (int i = 0; i < g_plugin_manager.plugin_count; i++) {
        pin_plugin_t* plugin = g_plugin_manager.plugins[i];
        pin_plugin_context_t* ctx = &g_plugin_manager.contexts[i];
        
        ESP_LOGI(TAG, "Plugin[%d]: %s v%s", i, plugin->metadata.name, plugin->metadata.version);
        ESP_LOGI(TAG, "  Status: %s, Running: %s", 
                 plugin->enabled ? "ENABLED" : "DISABLED",
                 plugin->running ? "YES" : "NO");
        ESP_LOGI(TAG, "  Memory: %d/%d bytes", ctx->stats.memory_used, plugin->config.memory_limit);
        ESP_LOGI(TAG, "  Updates: %d, Errors: %d", ctx->stats.update_count, ctx->stats.error_count);
    }
    
    return ESP_OK;
}
```

## 安全机制

### API访问控制

```c
// API权限检查
static bool pin_plugin_check_api_permission(pin_plugin_context_t* ctx, const char* api_name) {
    // 增加API调用计数
    ctx->stats.api_calls_count++;
    
    // 检查调用频率限制
    if (ctx->stats.api_calls_count > ctx->plugin->config.api_rate_limit) {
        ESP_LOGW(TAG, "Plugin '%s' API rate limit exceeded for '%s'", 
                 ctx->plugin->metadata.name, api_name);
        return false;
    }
    
    // 检查插件状态
    if (ctx->is_suspended || ctx->is_blocked) {
        ESP_LOGW(TAG, "Plugin '%s' access denied: suspended or blocked", ctx->plugin->metadata.name);
        return false;
    }
    
    return true;
}

// 安全的HTTP GET实现
static esp_err_t safe_http_get(const char* url, char* response, size_t max_len) {
    // URL白名单检查
    static const char* allowed_domains[] = {
        "api.openweathermap.org",
        "api.github.com",
        "httpbin.org",
        NULL
    };
    
    bool domain_allowed = false;
    for (int i = 0; allowed_domains[i]; i++) {
        if (strstr(url, allowed_domains[i])) {
            domain_allowed = true;
            break;
        }
    }
    
    if (!domain_allowed) {
        ESP_LOGW(TAG, "HTTP access denied to domain: %s", url);
        return ESP_ERR_NOT_ALLOWED;
    }
    
    // 执行HTTP请求
    return pin_http_client_get(url, response, max_len);
}
```

## 未来扩展

### 插件市场支持

```c
// 插件商店元数据
typedef struct {
    char store_url[256];        // 商店服务器URL
    char plugin_id[64];         // 插件商店ID
    char download_url[256];     // 下载URL
    char checksum[64];          // 文件校验和
    uint32_t file_size;         // 文件大小
    bool verified;              // 是否已验证
} pin_plugin_store_info_t;

// 插件热更新支持
esp_err_t pin_plugin_hot_reload(const char* plugin_name) {
    // 停止当前插件
    pin_plugin_enable(plugin_name, false);
    
    // 重新加载插件代码
    // (需要实现动态加载机制)
    
    // 重新启动插件
    return pin_plugin_enable(plugin_name, true);
}
```

## 使用示例

### 插件注册和使用

```c
void app_main(void) {
    // 初始化插件系统
    pin_plugin_manager_init();
    
    // 注册内置插件
    pin_plugin_register(&clock_plugin);
    pin_plugin_register(&weather_plugin);
    
    // 启动时钟插件
    pin_plugin_enable("clock", true);
    
    // 配置天气插件
    pin_plugin_set_config("weather", "api_key", "your_api_key_here");
    pin_plugin_set_config("weather", "city", "Shanghai");
    pin_plugin_enable("weather", true);
    
    // 主循环
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(10000));
        
        // 定期打印插件状态
        pin_plugin_system_diagnostic();
    }
}
```

插件系统为Pin设备提供了强大的扩展能力，既保证了系统的稳定性，又为未来的社区生态建设奠定了坚实的技术基础。
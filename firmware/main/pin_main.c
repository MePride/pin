/**
 * @file pin_main.c
 * @brief Pin Device Main Application Entry Point
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "driver/gpio.h"

#include "pin_display.h"
#include "pin_wifi.h"
#include "pin_plugin.h"
#include "pin_ota.h"
#include "pin_config.h"
#include "pin_webserver.h"
#include "pin_canvas.h"

static const char* TAG = "PIN_MAIN";

// 全局事件组和句柄
EventGroupHandle_t g_pin_event_group;
static pin_canvas_handle_t g_canvas_handle = NULL;
static fpc_a005_handle_t g_display_handle = NULL;

// Pin事件位定义
#define PIN_DISPLAY_READY_BIT   BIT0
#define PIN_WIFI_CONNECTED_BIT  BIT1
#define PIN_CANVAS_READY_BIT    BIT2
#define PIN_PLUGINS_READY_BIT   BIT3
#define PIN_WEB_SERVER_READY_BIT BIT4

/**
 * 系统初始化
 */
static esp_err_t pin_system_init(void) {
    esp_err_t ret;
    
    ESP_LOGI(TAG, "Pin Device starting up...");
    ESP_LOGI(TAG, "Firmware Version: %s", CONFIG_PIN_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // 初始化NVS
    ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition was truncated and needs to be erased");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_LOGI(TAG, "NVS initialized");
    
    // 初始化网络接口
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    
    // 创建全局事件组
    g_pin_event_group = xEventGroupCreate();
    if (g_pin_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create event group");
        return ESP_FAIL;
    }
    
    ESP_LOGI(TAG, "System initialization completed");
    return ESP_OK;
}

/**
 * 显示启动界面
 */
static void pin_show_startup_screen(void) {
    // 清屏并显示启动信息
    pin_display_clear(PIN_COLOR_WHITE);
    
    // 显示Logo区域
    pin_display_draw_text(200, 80, "Pin", PIN_FONT_XLARGE, PIN_COLOR_BLACK);
    pin_display_draw_text(120, 140, "Digital Minimalism", PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    // 显示版本信息
    char version_str[64];
    snprintf(version_str, sizeof(version_str), "Version: %s", CONFIG_PIN_FIRMWARE_VERSION);
    pin_display_draw_text(180, 180, version_str, PIN_FONT_SMALL, PIN_COLOR_BLACK);
    
    // 显示启动状态
    pin_display_draw_text(180, 220, "Initializing...", PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    pin_display_refresh(PIN_REFRESH_FULL);
}

/**
 * 更新启动状态
 */
static void pin_update_startup_status(const char* status) {
    // 清除之前的状态文本
    pin_display_draw_rect(120, 220, 360, 30, PIN_COLOR_WHITE, true);
    
    // 显示新状态
    pin_display_draw_text(180, 220, status, PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    pin_display_refresh(PIN_REFRESH_PARTIAL);
}

/**
 * 显示系统就绪界面
 */
static void pin_show_ready_screen(void) {
    pin_display_clear(PIN_COLOR_WHITE);
    
    // 显示就绪状态
    pin_display_draw_text(180, 100, "System Ready", PIN_FONT_LARGE, PIN_COLOR_GREEN);
    
    // 显示WiFi状态
    if (pin_wifi_is_connected()) {
        char ssid[32];
        if (pin_wifi_get_current_ssid(ssid, sizeof(ssid)) == ESP_OK) {
            char wifi_status[64];
            snprintf(wifi_status, sizeof(wifi_status), "WiFi: %s", ssid);
            pin_display_draw_text(120, 150, wifi_status, PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
            
            // 显示WiFi信号强度
            int8_t rssi = pin_wifi_get_rssi();
            pin_display_draw_wifi_icon(450, 150, rssi, PIN_COLOR_GREEN);
        }
    } else {
        pin_display_draw_text(120, 150, "WiFi: Not Connected", PIN_FONT_MEDIUM, PIN_COLOR_ORANGE);
    }
    
    // 显示电池状态
    float battery_voltage = pin_battery_get_voltage();
    uint8_t battery_percentage = pin_battery_get_percentage(battery_voltage);
    char battery_status[32];
    snprintf(battery_status, sizeof(battery_status), "Battery: %d%%", battery_percentage);
    pin_display_draw_text(120, 180, battery_status, PIN_FONT_MEDIUM, PIN_COLOR_BLACK);
    
    // 绘制电池图标
    pin_display_draw_battery_icon(450, 180, battery_percentage, 
                                 battery_percentage > 20 ? PIN_COLOR_GREEN : PIN_COLOR_RED);
    
    // 显示启动完成提示
    pin_display_draw_text(120, 220, "Loading plugins...", PIN_FONT_MEDIUM, PIN_COLOR_BLUE);
    
    pin_display_refresh(PIN_REFRESH_FULL);
}

/**
 * 检查唤醒原因并处理
 */
static void pin_handle_wakeup_reason(void) {
    esp_sleep_source_t wakeup_reasons = esp_sleep_get_wakeup_causes();
    esp_sleep_wakeup_cause_t wakeup_reason = ESP_SLEEP_WAKEUP_UNDEFINED;
    
    // Convert new API result to old API format for compatibility
    if (wakeup_reasons & ESP_SLEEP_WAKEUP_TIMER) {
        wakeup_reason = ESP_SLEEP_WAKEUP_TIMER;
    } else if (wakeup_reasons & ESP_SLEEP_WAKEUP_EXT0) {
        wakeup_reason = ESP_SLEEP_WAKEUP_EXT0;
    } else if (wakeup_reasons & ESP_SLEEP_WAKEUP_EXT1) {
        wakeup_reason = ESP_SLEEP_WAKEUP_EXT1;
    } else if (wakeup_reasons & ESP_SLEEP_WAKEUP_GPIO) {
        wakeup_reason = ESP_SLEEP_WAKEUP_GPIO;
    }
    
    switch (wakeup_reason) {
        case ESP_SLEEP_WAKEUP_TIMER:
            ESP_LOGI(TAG, "Wakeup from timer");
            break;
        case ESP_SLEEP_WAKEUP_GPIO:
            ESP_LOGI(TAG, "Wakeup from GPIO");
            break;
        case ESP_SLEEP_WAKEUP_UNDEFINED:
        default:
            ESP_LOGI(TAG, "Cold boot or reset");
            break;
    }
}

/**
 * 主任务
 */
static void pin_main_task(void *pvParameters) {
    EventBits_t event_bits;
    
    ESP_LOGI(TAG, "Main task started");
    
    // 等待所有子系统初始化完成
    event_bits = xEventGroupWaitBits(g_pin_event_group,
                                    PIN_DISPLAY_READY_BIT | PIN_WIFI_CONNECTED_BIT | PIN_PLUGINS_READY_BIT,
                                    pdFALSE,
                                    pdFALSE,  // 不需要所有位都设置，WiFi可能需要配置
                                    portMAX_DELAY);
    
    ESP_LOGI(TAG, "Subsystems initialization status: 0x%08x", (unsigned int)event_bits);
    
    // 显示系统就绪界面
    if (event_bits & PIN_DISPLAY_READY_BIT) {
        pin_show_ready_screen();
        vTaskDelay(pdMS_TO_TICKS(3000));  // 显示3秒
    }
    
    // 主循环
    while (1) {
        // 检查系统状态
        if (!pin_wifi_is_connected()) {
            ESP_LOGW(TAG, "WiFi connection lost, checking configuration...");
            // WiFi断开处理将在wifi模块中自动进行
        }
        
        // 检查电池电压
        float voltage = pin_battery_get_voltage();
        if (voltage < 3.2f) {  // 低电量警告
            ESP_LOGW(TAG, "Low battery warning: %.2fV", voltage);
            // TODO: 显示低电量警告
        }
        
        // 检查是否需要进入深度睡眠
        if (pin_config_get_sleep_enabled() && pin_should_enter_sleep()) {
            ESP_LOGI(TAG, "Entering deep sleep mode");
            pin_enter_deep_sleep();
        }
        
        // 主循环每10秒运行一次
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/**
 * 应用程序入口点
 */
void app_main(void) {
    esp_err_t ret;
    
    // 处理唤醒原因
    pin_handle_wakeup_reason();
    
    // 系统初始化
    ret = pin_system_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "System initialization failed: %s", esp_err_to_name(ret));
        esp_restart();
        return;
    }
    
    // 初始化显示系统
    pin_update_startup_status("Initializing Display...");
    ret = pin_display_init();
    if (ret == ESP_OK) {
        xEventGroupSetBits(g_pin_event_group, PIN_DISPLAY_READY_BIT);
        ESP_LOGI(TAG, "Display system initialized");
        
        // Get display handle for canvas system
        g_display_handle = pin_display_get_handle();
        
        // 显示启动界面
        pin_show_startup_screen();
    } else {
        ESP_LOGE(TAG, "Display initialization failed: %s", esp_err_to_name(ret));
    }
    
    // 初始化配置系统
    pin_update_startup_status("Loading Configuration...");
    pin_config_init();
    ESP_LOGI(TAG, "Configuration system initialized");
    
    // 初始化Canvas系统
    if (g_display_handle) {
        pin_update_startup_status("Initializing Canvas...");
        ret = pin_canvas_init(g_display_handle, &g_canvas_handle);
        if (ret == ESP_OK) {
            xEventGroupSetBits(g_pin_event_group, PIN_CANVAS_READY_BIT);
            ESP_LOGI(TAG, "Canvas system initialized");
        } else {
            ESP_LOGE(TAG, "Canvas initialization failed: %s", esp_err_to_name(ret));
        }
    }
    
    // 初始化WiFi系统
    pin_update_startup_status("Initializing WiFi...");
    ret = pin_wifi_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "WiFi system initialized");
        
        // 启动WiFi配置任务（它会处理连接或配网）
        pin_wifi_start_config_task();
    } else {
        ESP_LOGE(TAG, "WiFi initialization failed: %s", esp_err_to_name(ret));
    }
    
    // 初始化插件系统
    pin_update_startup_status("Loading Plugins...");
    ret = pin_plugin_manager_init();
    if (ret == ESP_OK) {
        xEventGroupSetBits(g_pin_event_group, PIN_PLUGINS_READY_BIT);
        ESP_LOGI(TAG, "Plugin system initialized");
        
        // 注册内置插件
        extern pin_plugin_t clock_plugin;
        extern pin_plugin_t weather_plugin;
        
        pin_plugin_register(&clock_plugin);
        pin_plugin_register(&weather_plugin);
        
        // 启用默认插件
        pin_plugin_enable("clock", true);
        pin_plugin_enable("weather", true);
    } else {
        ESP_LOGE(TAG, "Plugin system initialization failed: %s", esp_err_to_name(ret));
    }
    
    // 初始化OTA系统
    pin_update_startup_status("Initializing OTA System...");
    ret = pin_ota_init();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "OTA system initialized");
        // 启用自动更新检查 (每24小时检查一次)
        pin_ota_set_auto_check_interval(24);
    } else {
        ESP_LOGE(TAG, "OTA system initialization failed: %s", esp_err_to_name(ret));
    }
    
    // 初始化Web服务器
    pin_update_startup_status("Starting Web Server...");
    ret = pin_webserver_init(g_canvas_handle);
    if (ret == ESP_OK) {
        ret = pin_webserver_start();
        if (ret == ESP_OK) {
            xEventGroupSetBits(g_pin_event_group, PIN_WEB_SERVER_READY_BIT);
            ESP_LOGI(TAG, "Web server started");
        } else {
            ESP_LOGE(TAG, "Web server start failed: %s", esp_err_to_name(ret));
        }
    } else {
        ESP_LOGE(TAG, "Web server initialization failed: %s", esp_err_to_name(ret));
    }
    
    // 创建主任务
    xTaskCreate(pin_main_task, "pin_main", 4096, NULL, 5, NULL);
    
    ESP_LOGI(TAG, "Pin Device initialization completed");
}
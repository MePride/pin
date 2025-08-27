/**
 * @file pin_wifi.c
 * @brief Pin Device WiFi Implementation
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "pin_wifi.h"
#include "pin_config.h"

static const char *TAG = "PIN_WIFI";

esp_err_t pin_wifi_init(void) {
    ESP_LOGI(TAG, "WiFi initialization");
    
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_LOGI(TAG, "WiFi initialized successfully");
    return ESP_OK;
}

esp_err_t pin_wifi_start_ap(void) {
    esp_netif_create_default_wifi_ap();

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = CONFIG_PIN_WIFI_AP_SSID_PREFIX "0000",
            .ssid_len = strlen(CONFIG_PIN_WIFI_AP_SSID_PREFIX "0000"),
            .channel = 1,
            .password = CONFIG_PIN_WIFI_AP_PASSWORD,
            .max_connection = CONFIG_PIN_WIFI_AP_MAX_CONNECTIONS,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    
    if (strlen(CONFIG_PIN_WIFI_AP_PASSWORD) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "WiFi AP started. SSID:%s password:%s channel:%d",
             CONFIG_PIN_WIFI_AP_SSID_PREFIX "0000", CONFIG_PIN_WIFI_AP_PASSWORD, 1);
    
    return ESP_OK;
}

bool pin_wifi_is_connected(void) {
    wifi_mode_t mode;
    esp_err_t ret = esp_wifi_get_mode(&mode);
    if (ret != ESP_OK) {
        return false;
    }
    
    if (mode == WIFI_MODE_STA || mode == WIFI_MODE_APSTA) {
        wifi_ap_record_t ap_info;
        ret = esp_wifi_sta_get_ap_info(&ap_info);
        return (ret == ESP_OK);
    }
    
    return false;
}

esp_err_t pin_wifi_get_current_ssid(char* ssid, size_t max_len) {
    if (!ssid || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        strncpy(ssid, (char*)ap_info.ssid, max_len - 1);
        ssid[max_len - 1] = '\0';
    } else {
        ssid[0] = '\0';
    }
    
    return ret;
}

int8_t pin_wifi_get_rssi(void) {
    wifi_ap_record_t ap_info;
    esp_err_t ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (ret == ESP_OK) {
        return ap_info.rssi;
    }
    return -100; // Very weak signal as default
}

esp_err_t pin_wifi_start_config_task(void) {
    ESP_LOGI(TAG, "Starting WiFi configuration task");
    return ESP_OK;
}
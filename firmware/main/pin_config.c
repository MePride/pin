/**
 * @file pin_config.c
 * @brief Pin Device Configuration Implementation
 */

#include "pin_config.h"
#include "esp_log.h"

static const char* TAG = "PIN_CONFIG";

void pin_config_init(void) {
    ESP_LOGI(TAG, "Configuration initialized");
    ESP_LOGI(TAG, "Firmware Version: %s", CONFIG_PIN_FIRMWARE_VERSION);
    ESP_LOGI(TAG, "Device Name: %s", CONFIG_PIN_DEVICE_NAME);
}

bool pin_config_get_sleep_enabled(void) {
    // For now, return true - this could be made configurable later
    return true;
}
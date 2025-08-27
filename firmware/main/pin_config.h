/**
 * @file pin_config.h
 * @brief Pin Device Configuration
 */

#ifndef PIN_CONFIG_H
#define PIN_CONFIG_H

#ifdef __cplusplus
extern "C" {
#endif

// Device configuration
#define CONFIG_PIN_FIRMWARE_VERSION "1.0.0"
#define CONFIG_PIN_DEVICE_NAME "Pin E-Paper Display"

// WiFi configuration
#define CONFIG_PIN_WIFI_AP_SSID_PREFIX "Pin-Device-"
#define CONFIG_PIN_WIFI_AP_PASSWORD "pin12345"
#define CONFIG_PIN_WIFI_AP_MAX_CONNECTIONS 4

// Display configuration
#define CONFIG_PIN_DISPLAY_ROTATION 0
#define CONFIG_PIN_DISPLAY_REFRESH_RATE 60

// Plugin configuration
#define CONFIG_PIN_PLUGIN_MAX_COUNT 10
#define CONFIG_PIN_PLUGIN_HEAP_SIZE 32768

#ifdef __cplusplus
}
#endif

#endif // PIN_CONFIG_H
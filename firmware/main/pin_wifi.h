/**
 * @file pin_wifi.h
 * @brief Pin WiFi Configuration System Header
 */

#pragma once

#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// WiFi event bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// WiFi configuration states
typedef enum {
    WIFI_CONFIG_STATE_IDLE,
    WIFI_CONFIG_STATE_CHECK_SAVED,
    WIFI_CONFIG_STATE_AP_MODE,
    WIFI_CONFIG_STATE_PORTAL_ACTIVE,
    WIFI_CONFIG_STATE_CONNECTING,
    WIFI_CONFIG_STATE_CONNECTED,
    WIFI_CONFIG_STATE_FAILED,
    WIFI_CONFIG_STATE_TIMEOUT
} pin_wifi_config_state_t;

// WiFi network information
typedef struct {
    char ssid[32];
    int8_t rssi;
    wifi_auth_mode_t auth;
    uint8_t channel;
} pin_wifi_network_t;

// WiFi configuration
typedef struct {
    pin_wifi_config_state_t state;
    char ap_ssid[32];
    char ap_password[64];
    char target_ssid[32];
    char target_password[64];
    uint32_t config_timeout_ms;
    uint32_t connect_timeout_ms;
    uint32_t portal_start_time;
    uint8_t retry_count;
    uint8_t max_retry;
    bool config_received;
    bool force_ap_mode;
} pin_wifi_config_t;

/**
 * @brief Initialize WiFi system
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_init(void);

/**
 * @brief Start WiFi configuration task
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_start_config_task(void);

/**
 * @brief Check if WiFi is connected
 * @return true if connected
 */
bool pin_wifi_is_connected(void);

/**
 * @brief Get current WiFi SSID
 * @param ssid Buffer to store SSID
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_get_current_ssid(char* ssid, size_t max_len);

/**
 * @brief Get WiFi RSSI
 * @return RSSI value in dBm
 */
int8_t pin_wifi_get_rssi(void);

/**
 * @brief Check if saved WiFi configuration exists
 * @return true if configuration exists
 */
bool pin_wifi_has_saved_config(void);

/**
 * @brief Load saved WiFi configuration
 * @param ssid Buffer for SSID
 * @param password Buffer for password
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_load_saved_config(char* ssid, char* password);

/**
 * @brief Save WiFi configuration
 * @param ssid WiFi SSID
 * @param password WiFi password
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_save_config(const char* ssid, const char* password);

/**
 * @brief Clear saved WiFi configuration
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_clear_config(void);

/**
 * @brief Start AP mode for configuration
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_start_ap_mode(void);

/**
 * @brief Start configuration portal
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_start_config_portal(void);

/**
 * @brief Stop configuration portal
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_stop_config_portal(void);

/**
 * @brief Scan for WiFi networks
 * @param networks Array to store networks
 * @param max_networks Maximum number of networks
 * @param found_networks Pointer to store actual number found
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_scan_networks(pin_wifi_network_t* networks, uint16_t max_networks, uint16_t* found_networks);

/**
 * @brief Connect to WiFi network
 * @param ssid Network SSID
 * @param password Network password
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_connect(const char* ssid, const char* password);

/**
 * @brief Force AP configuration mode
 * @param force True to force AP mode
 */
void pin_wifi_force_ap_mode(bool force);

/**
 * @brief Get WiFi configuration state
 * @return Current state
 */
pin_wifi_config_state_t pin_wifi_get_state(void);

/**
 * @brief Get AP SSID for configuration
 * @param ssid Buffer to store AP SSID
 * @param max_len Maximum buffer length
 * @return ESP_OK on success
 */
esp_err_t pin_wifi_get_ap_ssid(char* ssid, size_t max_len);

#ifdef __cplusplus
}
#endif
/**
 * @file pin_ota.h
 * @brief Pin Device OTA (Over-The-Air) Update System
 */

#pragma once

#include "esp_err.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"

#ifdef __cplusplus
extern "C" {
#endif

#define PIN_OTA_URL_MAX_LEN 256
#define PIN_OTA_VERSION_MAX_LEN 32
#define PIN_OTA_HASH_LEN 32

typedef enum {
    PIN_OTA_STATE_IDLE,
    PIN_OTA_STATE_CHECKING,
    PIN_OTA_STATE_DOWNLOADING,
    PIN_OTA_STATE_INSTALLING,
    PIN_OTA_STATE_COMPLETE,
    PIN_OTA_STATE_ERROR
} pin_ota_state_t;

typedef struct {
    char version[PIN_OTA_VERSION_MAX_LEN];
    char url[PIN_OTA_URL_MAX_LEN];
    char description[128];
    uint8_t sha256_hash[PIN_OTA_HASH_LEN];
    size_t size;
    bool force_update;
    uint32_t release_timestamp;
} pin_ota_info_t;

typedef struct {
    pin_ota_state_t state;
    int progress_percent;
    char current_version[PIN_OTA_VERSION_MAX_LEN];
    char error_message[128];
    pin_ota_info_t available_update;
    bool update_available;
    uint32_t last_check_time;
} pin_ota_status_t;

typedef void (*pin_ota_progress_callback_t)(int progress_percent, const char* message);
typedef void (*pin_ota_complete_callback_t)(bool success, const char* message);

/**
 * @brief Initialize OTA system
 */
esp_err_t pin_ota_init(void);

/**
 * @brief Check for available updates
 * @param update_url URL to check for updates (JSON endpoint)
 * @return ESP_OK on success
 */
esp_err_t pin_ota_check_update(const char* update_url);

/**
 * @brief Start OTA update process
 * @param progress_cb Progress callback function
 * @param complete_cb Completion callback function
 * @return ESP_OK on success
 */
esp_err_t pin_ota_start_update(pin_ota_progress_callback_t progress_cb, 
                               pin_ota_complete_callback_t complete_cb);

/**
 * @brief Get current OTA status
 * @param status Output status structure
 * @return ESP_OK on success
 */
esp_err_t pin_ota_get_status(pin_ota_status_t* status);

/**
 * @brief Cancel ongoing update
 * @return ESP_OK on success
 */
esp_err_t pin_ota_cancel_update(void);

/**
 * @brief Rollback to previous firmware version
 * @return ESP_OK on success
 */
esp_err_t pin_ota_rollback(void);

/**
 * @brief Validate current firmware and mark as valid
 * Should be called after successful boot with new firmware
 * @return ESP_OK on success
 */
esp_err_t pin_ota_mark_valid(void);

/**
 * @brief Set automatic update checking interval
 * @param interval_hours Hours between update checks (0 to disable)
 * @return ESP_OK on success
 */
esp_err_t pin_ota_set_auto_check_interval(uint32_t interval_hours);

/**
 * @brief Get current firmware version
 * @return Version string
 */
const char* pin_ota_get_current_version(void);

#ifdef __cplusplus
}
#endif
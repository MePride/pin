/**
 * @file pin_ota.c
 * @brief Pin Device OTA Update System Implementation
 */

#include <string.h>
#include <stdio.h>
#include "pin_ota.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "esp_crt_bundle.h"
#include "cJSON.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "PIN_OTA";

// OTA configuration constants
#define PIN_OTA_TASK_STACK_SIZE 8192
#define PIN_OTA_BUFFER_SIZE 1024
#define PIN_OTA_TIMEOUT_MS 30000
#define PIN_OTA_NVS_NAMESPACE "ota_config"

// Event group for OTA coordination
static EventGroupHandle_t ota_event_group;
#define OTA_UPDATE_BIT BIT0
#define OTA_CANCEL_BIT BIT1

// Global OTA state
static pin_ota_status_t g_ota_status = {
    .state = PIN_OTA_STATE_IDLE,
    .progress_percent = 0,
    .current_version = "1.0.0",
    .error_message = "",
    .update_available = false,
    .last_check_time = 0
};

// Callback functions
static pin_ota_progress_callback_t g_progress_callback = NULL;
static pin_ota_complete_callback_t g_complete_callback = NULL;

// Auto-check configuration
static esp_timer_handle_t g_auto_check_timer = NULL;
static uint32_t g_auto_check_interval_hours = 0;

// Forward declarations
static void ota_update_task(void *pvParameter);
static void auto_check_timer_callback(void *arg);
static esp_err_t parse_update_info(const char *json_data, pin_ota_info_t *info);
static esp_err_t download_and_install_update(const pin_ota_info_t *update_info);

esp_err_t pin_ota_init(void) {
    ESP_LOGI(TAG, "Initializing OTA system");
    
    // Create event group for OTA coordination
    ota_event_group = xEventGroupCreate();
    if (!ota_event_group) {
        ESP_LOGE(TAG, "Failed to create OTA event group");
        return ESP_ERR_NO_MEM;
    }
    
    // Get current running partition info
    const esp_partition_t *running = esp_ota_get_running_partition();
    if (running) {
        ESP_LOGI(TAG, "Running partition: %s, type %d, subtype %d, offset 0x%x, size 0x%x",
                 running->label, running->type, running->subtype, 
                 running->address, running->size);
    }
    
    // Check if we need to mark current firmware as valid
    esp_ota_img_states_t ota_state;
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            ESP_LOGW(TAG, "Current firmware pending verification - auto-validating");
            pin_ota_mark_valid();
        }
    }
    
    // Initialize auto-check timer
    esp_timer_create_args_t timer_args = {
        .callback = auto_check_timer_callback,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "ota_auto_check"
    };
    
    esp_err_t ret = esp_timer_create(&timer_args, &g_auto_check_timer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create auto-check timer");
        return ret;
    }
    
    ESP_LOGI(TAG, "OTA system initialized successfully");
    return ESP_OK;
}

esp_err_t pin_ota_check_update(const char* update_url) {
    if (!update_url) {
        return ESP_ERR_INVALID_ARG;
    }
    
    ESP_LOGI(TAG, "Checking for updates from: %s", update_url);
    g_ota_status.state = PIN_OTA_STATE_CHECKING;
    
    // Configure HTTP client
    esp_http_client_config_t config = {
        .url = update_url,
        .method = HTTP_METHOD_GET,
        .timeout_ms = PIN_OTA_TIMEOUT_MS,
        .crt_bundle_attach = esp_crt_bundle_attach,
    };
    
    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        g_ota_status.state = PIN_OTA_STATE_ERROR;
        strncpy(g_ota_status.error_message, "Failed to initialize HTTP client", 
                sizeof(g_ota_status.error_message) - 1);
        return ESP_ERR_NO_MEM;
    }
    
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        int status_code = esp_http_client_get_status_code(client);
        int content_length = esp_http_client_get_content_length(client);
        
        if (status_code == 200 && content_length > 0) {
            char *buffer = malloc(content_length + 1);
            if (buffer) {
                int data_read = esp_http_client_read_response(client, buffer, content_length);
                if (data_read > 0) {
                    buffer[data_read] = '\0';
                    
                    // Parse update information
                    err = parse_update_info(buffer, &g_ota_status.available_update);
                    if (err == ESP_OK) {
                        // Compare versions to see if update is needed
                        if (strcmp(g_ota_status.available_update.version, g_ota_status.current_version) != 0) {
                            g_ota_status.update_available = true;
                            ESP_LOGI(TAG, "Update available: %s -> %s", 
                                    g_ota_status.current_version, 
                                    g_ota_status.available_update.version);
                        } else {
                            g_ota_status.update_available = false;
                            ESP_LOGI(TAG, "Already running latest version: %s", 
                                    g_ota_status.current_version);
                        }
                    }
                }
                free(buffer);
            } else {
                err = ESP_ERR_NO_MEM;
            }
        } else {
            ESP_LOGE(TAG, "HTTP request failed: status %d", status_code);
            err = ESP_FAIL;
        }
    }
    
    esp_http_client_cleanup(client);
    
    if (err != ESP_OK) {
        g_ota_status.state = PIN_OTA_STATE_ERROR;
        snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                "Update check failed: %s", esp_err_to_name(err));
    } else {
        g_ota_status.state = PIN_OTA_STATE_IDLE;
    }
    
    g_ota_status.last_check_time = esp_timer_get_time() / 1000000;
    return err;
}

esp_err_t pin_ota_start_update(pin_ota_progress_callback_t progress_cb, 
                               pin_ota_complete_callback_t complete_cb) {
    
    if (!g_ota_status.update_available) {
        ESP_LOGW(TAG, "No update available");
        return ESP_ERR_INVALID_STATE;
    }
    
    if (g_ota_status.state != PIN_OTA_STATE_IDLE) {
        ESP_LOGW(TAG, "OTA already in progress");
        return ESP_ERR_INVALID_STATE;
    }
    
    g_progress_callback = progress_cb;
    g_complete_callback = complete_cb;
    
    // Create OTA update task
    BaseType_t ret = xTaskCreate(ota_update_task, "ota_update", PIN_OTA_TASK_STACK_SIZE, 
                                 NULL, 5, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create OTA update task");
        return ESP_ERR_NO_MEM;
    }
    
    ESP_LOGI(TAG, "OTA update started");
    return ESP_OK;
}

esp_err_t pin_ota_get_status(pin_ota_status_t* status) {
    if (!status) {
        return ESP_ERR_INVALID_ARG;
    }
    
    memcpy(status, &g_ota_status, sizeof(pin_ota_status_t));
    return ESP_OK;
}

esp_err_t pin_ota_cancel_update(void) {
    if (g_ota_status.state != PIN_OTA_STATE_DOWNLOADING && 
        g_ota_status.state != PIN_OTA_STATE_INSTALLING) {
        return ESP_ERR_INVALID_STATE;
    }
    
    xEventGroupSetBits(ota_event_group, OTA_CANCEL_BIT);
    ESP_LOGI(TAG, "OTA update cancellation requested");
    return ESP_OK;
}

esp_err_t pin_ota_rollback(void) {
    ESP_LOGI(TAG, "Rolling back to previous firmware");
    
    const esp_partition_t *running = esp_ota_get_running_partition();
    esp_ota_img_states_t ota_state;
    
    if (esp_ota_get_state_partition(running, &ota_state) == ESP_OK) {
        if (ota_state == ESP_OTA_IMG_VALID || ota_state == ESP_OTA_IMG_PENDING_VERIFY) {
            // Mark current partition as invalid
            esp_err_t err = esp_ota_mark_app_invalid_rollback_and_reboot();
            if (err == ESP_OK) {
                // This point should never be reached as the function reboots
                return ESP_OK;
            } else {
                ESP_LOGE(TAG, "Rollback failed: %s", esp_err_to_name(err));
                return err;
            }
        }
    }
    
    ESP_LOGE(TAG, "Cannot rollback - no valid previous partition");
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t pin_ota_mark_valid(void) {
    ESP_LOGI(TAG, "Marking current firmware as valid");
    
    esp_err_t err = esp_ota_mark_app_valid_cancel_rollback();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mark app as valid: %s", esp_err_to_name(err));
    }
    
    return err;
}

esp_err_t pin_ota_set_auto_check_interval(uint32_t interval_hours) {
    g_auto_check_interval_hours = interval_hours;
    
    // Stop existing timer
    esp_timer_stop(g_auto_check_timer);
    
    if (interval_hours > 0) {
        uint64_t interval_us = (uint64_t)interval_hours * 3600 * 1000000;
        esp_err_t err = esp_timer_start_periodic(g_auto_check_timer, interval_us);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to start auto-check timer: %s", esp_err_to_name(err));
            return err;
        }
        ESP_LOGI(TAG, "Auto-check enabled: every %d hours", interval_hours);
    } else {
        ESP_LOGI(TAG, "Auto-check disabled");
    }
    
    return ESP_OK;
}

const char* pin_ota_get_current_version(void) {
    return g_ota_status.current_version;
}

// Private functions

static void ota_update_task(void *pvParameter) {
    ESP_LOGI(TAG, "Starting OTA update task");
    
    esp_err_t err = download_and_install_update(&g_ota_status.available_update);
    
    if (err == ESP_OK) {
        g_ota_status.state = PIN_OTA_STATE_COMPLETE;
        g_ota_status.progress_percent = 100;
        
        if (g_complete_callback) {
            g_complete_callback(true, "Update completed successfully - reboot required");
        }
        
        ESP_LOGI(TAG, "OTA update completed successfully");
        
        // Wait a bit for callback processing, then reboot
        vTaskDelay(pdMS_TO_TICKS(3000));
        esp_restart();
    } else {
        g_ota_status.state = PIN_OTA_STATE_ERROR;
        snprintf(g_ota_status.error_message, sizeof(g_ota_status.error_message), 
                "Update failed: %s", esp_err_to_name(err));
        
        if (g_complete_callback) {
            g_complete_callback(false, g_ota_status.error_message);
        }
        
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
    }
    
    vTaskDelete(NULL);
}

static void auto_check_timer_callback(void *arg) {
    ESP_LOGI(TAG, "Automatic update check triggered");
    
    // Use a default update server URL - this should be configurable
    const char* update_url = "https://api.github.com/repos/MePride/pin/releases/latest";
    pin_ota_check_update(update_url);
}

static esp_err_t parse_update_info(const char *json_data, pin_ota_info_t *info) {
    cJSON *json = cJSON_Parse(json_data);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse update JSON");
        return ESP_FAIL;
    }
    
    esp_err_t err = ESP_OK;
    
    // Parse GitHub release format
    cJSON *tag_name = cJSON_GetObjectItem(json, "tag_name");
    cJSON *body = cJSON_GetObjectItem(json, "body");
    cJSON *assets = cJSON_GetObjectItem(json, "assets");
    cJSON *published_at = cJSON_GetObjectItem(json, "published_at");
    
    if (cJSON_IsString(tag_name)) {
        strncpy(info->version, tag_name->valuestring, PIN_OTA_VERSION_MAX_LEN - 1);
        info->version[PIN_OTA_VERSION_MAX_LEN - 1] = '\0';
    } else {
        err = ESP_ERR_INVALID_ARG;
    }
    
    if (cJSON_IsString(body)) {
        strncpy(info->description, body->valuestring, sizeof(info->description) - 1);
        info->description[sizeof(info->description) - 1] = '\0';
    }
    
    // Find firmware binary in assets
    if (cJSON_IsArray(assets)) {
        cJSON *asset = NULL;
        cJSON_ArrayForEach(asset, assets) {
            cJSON *name = cJSON_GetObjectItem(asset, "name");
            cJSON *browser_download_url = cJSON_GetObjectItem(asset, "browser_download_url");
            cJSON *size = cJSON_GetObjectItem(asset, "size");
            
            if (cJSON_IsString(name) && strstr(name->valuestring, "pin_firmware.bin")) {
                if (cJSON_IsString(browser_download_url)) {
                    strncpy(info->url, browser_download_url->valuestring, PIN_OTA_URL_MAX_LEN - 1);
                    info->url[PIN_OTA_URL_MAX_LEN - 1] = '\0';
                }
                if (cJSON_IsNumber(size)) {
                    info->size = (size_t)size->valueint;
                }
                break;
            }
        }
    }
    
    cJSON_Delete(json);
    
    if (strlen(info->url) == 0) {
        ESP_LOGE(TAG, "No firmware binary found in release");
        err = ESP_ERR_NOT_FOUND;
    }
    
    return err;
}

static esp_err_t download_and_install_update(const pin_ota_info_t *update_info) {
    g_ota_status.state = PIN_OTA_STATE_DOWNLOADING;
    g_ota_status.progress_percent = 0;
    
    ESP_LOGI(TAG, "Starting download from: %s", update_info->url);
    
    esp_https_ota_config_t ota_config = {
        .http_config = {
            .url = update_info->url,
            .crt_bundle_attach = esp_crt_bundle_attach,
            .timeout_ms = PIN_OTA_TIMEOUT_MS,
            .keep_alive_enable = true,
        },
    };
    
    esp_https_ota_handle_t https_ota_handle = NULL;
    esp_err_t err = esp_https_ota_begin(&ota_config, &https_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "ESP HTTPS OTA Begin failed");
        return err;
    }
    
    esp_app_desc_t app_desc;
    err = esp_https_ota_get_img_desc(https_ota_handle, &app_desc);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_https_ota_read_img_desc failed");
        goto ota_end;
    }
    
    ESP_LOGI(TAG, "New firmware version: %s", app_desc.version);
    
    g_ota_status.state = PIN_OTA_STATE_INSTALLING;
    
    while (1) {
        // Check for cancellation
        EventBits_t bits = xEventGroupWaitBits(ota_event_group, OTA_CANCEL_BIT, 
                                               pdTRUE, pdFALSE, 0);
        if (bits & OTA_CANCEL_BIT) {
            ESP_LOGI(TAG, "OTA update cancelled by user");
            err = ESP_ERR_INVALID_STATE;
            break;
        }
        
        err = esp_https_ota_perform(https_ota_handle);
        if (err != ESP_ERR_HTTPS_OTA_IN_PROGRESS) {
            break;
        }
        
        // Update progress
        int total_size = esp_https_ota_get_image_size(https_ota_handle);
        int downloaded = esp_https_ota_get_image_len_read(https_ota_handle);
        
        if (total_size > 0) {
            g_ota_status.progress_percent = (downloaded * 100) / total_size;
            
            if (g_progress_callback) {
                char progress_msg[64];
                snprintf(progress_msg, sizeof(progress_msg), "Downloaded %d/%d bytes", 
                        downloaded, total_size);
                g_progress_callback(g_ota_status.progress_percent, progress_msg);
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (esp_https_ota_is_complete_data_received(https_ota_handle) != true) {
        ESP_LOGE(TAG, "Complete data was not received.");
        err = ESP_ERR_INVALID_SIZE;
    }

ota_end:
    esp_err_t ota_finish_err = esp_https_ota_finish(https_ota_handle);
    if ((err == ESP_OK) && (ota_finish_err == ESP_OK)) {
        ESP_LOGI(TAG, "ESP_HTTPS_OTA upgrade successful. Rebooting ...");
        return ESP_OK;
    } else {
        if (ota_finish_err == ESP_ERR_OTA_VALIDATE_FAILED) {
            ESP_LOGE(TAG, "Image validation failed, image is corrupted");
        }
        ESP_LOGE(TAG, "ESP_HTTPS_OTA upgrade failed 0x%x", ota_finish_err);
        return ota_finish_err;
    }
}
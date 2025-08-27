/**
 * @file pin_webserver.h
 * @brief Pin Device Web Server
 * 
 * HTTP server for device configuration and canvas API
 */

#ifndef PIN_WEBSERVER_H
#define PIN_WEBSERVER_H

#include "esp_err.h"
#include "esp_http_server.h"
#include "pin_canvas.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize web server
 * 
 * @param canvas_handle Canvas system handle
 * @return ESP_OK on success
 */
esp_err_t pin_webserver_init(pin_canvas_handle_t canvas_handle);

/**
 * @brief Start web server
 * 
 * @return ESP_OK on success
 */
esp_err_t pin_webserver_start(void);

/**
 * @brief Stop web server
 * 
 * @return ESP_OK on success
 */
esp_err_t pin_webserver_stop(void);

/**
 * @brief Get web server handle
 * 
 * @return Web server handle or NULL if not started
 */
httpd_handle_t pin_webserver_get_handle(void);

#ifdef __cplusplus
}
#endif

#endif // PIN_WEBSERVER_H
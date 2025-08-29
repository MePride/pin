/**
 * @file pin_webserver.c
 * @brief Pin Device Web Server Implementation
 */

#include <stdio.h>
#include <string.h>
#include <sys/param.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "esp_http_server.h"
#include "cJSON.h"

#include "pin_webserver.h"
#include "pin_display.h"
#include "pin_wifi.h"
#include "esp_timer.h"
#include "pin_canvas.h"

static const char *TAG = "PIN_WEBSERVER";

static httpd_handle_t server = NULL;
static pin_canvas_handle_t g_canvas_handle = NULL;

// Embedded web files
extern const uint8_t index_html_start[] asm("_binary_index_html_start");
extern const uint8_t index_html_end[]   asm("_binary_index_html_end");
extern const uint8_t app_js_start[] asm("_binary_app_js_start");
extern const uint8_t app_js_end[]   asm("_binary_app_js_end");
extern const uint8_t manifest_json_start[] asm("_binary_manifest_json_start");
extern const uint8_t manifest_json_end[]   asm("_binary_manifest_json_end");
extern const uint8_t sw_js_start[] asm("_binary_sw_js_start");
extern const uint8_t sw_js_end[]   asm("_binary_sw_js_end");

// Helper functions
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json, int status_code);
static esp_err_t send_error_response(httpd_req_t *req, int status_code, const char *message);
static char* get_request_body(httpd_req_t *req);

// Static file handlers
static esp_err_t index_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    return httpd_resp_send(req, (const char*)index_html_start, index_html_end - index_html_start);
}

static esp_err_t app_js_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    return httpd_resp_send(req, (const char*)app_js_start, app_js_end - app_js_start);
}

static esp_err_t manifest_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    return httpd_resp_send(req, (const char*)manifest_json_start, manifest_json_end - manifest_json_start);
}

static esp_err_t sw_js_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "application/javascript");
    httpd_resp_set_hdr(req, "Cache-Control", "public, max-age=31536000");
    return httpd_resp_send(req, (const char*)sw_js_start, sw_js_end - sw_js_start);
}

// Device API handlers
static esp_err_t api_status_handler(httpd_req_t *req) {
    cJSON *response = cJSON_CreateObject();
    
    // Add device information
    cJSON_AddStringToObject(response, "firmware_version", "1.0.0");
    cJSON_AddStringToObject(response, "device_name", "Pin E-ink Display");
    
    // Add battery information
    float battery_voltage = pin_battery_get_voltage();
    uint8_t battery_percentage = pin_battery_get_percentage(battery_voltage);
    cJSON_AddNumberToObject(response, "battery_voltage", battery_voltage);
    cJSON_AddNumberToObject(response, "battery_percentage", battery_percentage);
    
    // Add WiFi information
    cJSON *wifi_info = cJSON_CreateObject();
    cJSON_AddBoolToObject(wifi_info, "connected", pin_wifi_is_connected());
    
    if (pin_wifi_is_connected()) {
        char ssid[32] = {0};
        if (pin_wifi_get_current_ssid(ssid, sizeof(ssid)) == ESP_OK) {
            cJSON_AddStringToObject(wifi_info, "ssid", ssid);
        }
        cJSON_AddNumberToObject(wifi_info, "rssi", pin_wifi_get_rssi());
    }
    cJSON_AddItemToObject(response, "wifi", wifi_info);
    
    // Add system information
    cJSON *system_info = cJSON_CreateObject();
    cJSON_AddNumberToObject(system_info, "free_heap", esp_get_free_heap_size());
    cJSON_AddNumberToObject(system_info, "uptime", esp_timer_get_time() / 1000000);
    cJSON_AddItemToObject(response, "system", system_info);
    
    return send_json_response(req, response, 200);
}

static esp_err_t api_display_refresh_handler(httpd_req_t *req) {
    fpc_a005_handle_t display_handle = pin_display_get_handle();
    if (!display_handle) {
        return send_error_response(req, 500, "Display not initialized");
    }
    
    esp_err_t ret = fpc_a005_refresh(display_handle, FPC_A005_REFRESH_FULL);
    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to refresh display");
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Display refreshed successfully");
    return send_json_response(req, response, 200);
}

static esp_err_t api_display_clear_handler(httpd_req_t *req) {
    esp_err_t ret = pin_display_clear(PIN_CANVAS_COLOR_WHITE);
    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to clear display");
    }
    
    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Display cleared successfully");
    return send_json_response(req, response, 200);
}

// Canvas API handlers
static esp_err_t canvas_list_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char canvas_ids[50][32];
    size_t count;
    esp_err_t ret = pin_canvas_list(g_canvas_handle, canvas_ids, 50, &count);
    
    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to list canvases");
    }

    cJSON *json = cJSON_CreateObject();
    cJSON *canvases = cJSON_CreateArray();

    for (size_t i = 0; i < count; i++) {
        pin_canvas_t canvas;
        ret = pin_canvas_get(g_canvas_handle, canvas_ids[i], &canvas);
        if (ret == ESP_OK) {
            cJSON *canvas_info = cJSON_CreateObject();
            cJSON_AddStringToObject(canvas_info, "id", canvas.id);
            cJSON_AddStringToObject(canvas_info, "name", canvas.name);
            cJSON_AddNumberToObject(canvas_info, "created_time", canvas.created_time);
            cJSON_AddNumberToObject(canvas_info, "modified_time", canvas.modified_time);
            cJSON_AddNumberToObject(canvas_info, "element_count", canvas.element_count);
            cJSON_AddItemToArray(canvases, canvas_info);
        }
    }

    cJSON_AddItemToObject(json, "canvases", canvases);
    cJSON_AddNumberToObject(json, "total", count);

    return send_json_response(req, json, 200);
}

static esp_err_t canvas_create_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char *body = get_request_body(req);
    if (!body) {
        return send_error_response(req, 400, "Invalid request body");
    }

    cJSON *json = cJSON_Parse(body);
    free(body);

    if (!json) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    cJSON *id_item = cJSON_GetObjectItem(json, "id");
    cJSON *name_item = cJSON_GetObjectItem(json, "name");

    if (!cJSON_IsString(id_item) || !cJSON_IsString(name_item)) {
        cJSON_Delete(json);
        return send_error_response(req, 400, "Missing required fields: id, name");
    }

    esp_err_t ret = pin_canvas_create(g_canvas_handle, id_item->valuestring, name_item->valuestring);
    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to create canvas");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Canvas created successfully");
    cJSON_AddStringToObject(response, "id", id_item->valuestring);

    return send_json_response(req, response, 201);
}

static esp_err_t canvas_get_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char canvas_id[32];
    if (httpd_req_get_url_query_str(req, canvas_id, sizeof(canvas_id)) != ESP_OK) {
        return send_error_response(req, 400, "Missing canvas_id parameter");
    }

    // Extract canvas_id from query string
    char *id_param = strstr(canvas_id, "id=");
    if (!id_param) {
        return send_error_response(req, 400, "Missing canvas_id parameter");
    }
    id_param += 3; // Skip "id="

    char *json_str;
    esp_err_t ret = pin_canvas_export_json(g_canvas_handle, id_param, &json_str);
    if (ret != ESP_OK) {
        return send_error_response(req, 404, "Canvas not found");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json_str, strlen(json_str));
    free(json_str);

    return ESP_OK;
}

static esp_err_t canvas_update_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char *body = get_request_body(req);
    if (!body) {
        return send_error_response(req, 400, "Invalid request body");
    }

    esp_err_t ret = pin_canvas_import_json(g_canvas_handle, body);
    free(body);

    if (ret != ESP_OK) {
        return send_error_response(req, 400, "Failed to update canvas");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Canvas updated successfully");

    return send_json_response(req, response, 200);
}

static esp_err_t canvas_delete_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char canvas_id[32];
    if (httpd_req_get_url_query_str(req, canvas_id, sizeof(canvas_id)) != ESP_OK) {
        return send_error_response(req, 400, "Missing canvas_id parameter");
    }

    // Extract canvas_id from query string
    char *id_param = strstr(canvas_id, "id=");
    if (!id_param) {
        return send_error_response(req, 400, "Missing canvas_id parameter");
    }
    id_param += 3; // Skip "id="

    esp_err_t ret = pin_canvas_delete(g_canvas_handle, id_param);
    if (ret != ESP_OK) {
        return send_error_response(req, 404, "Canvas not found or failed to delete");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Canvas deleted successfully");

    return send_json_response(req, response, 200);
}

static esp_err_t canvas_display_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char *body = get_request_body(req);
    if (!body) {
        return send_error_response(req, 400, "Invalid request body");
    }

    cJSON *json = cJSON_Parse(body);
    free(body);

    if (!json) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    cJSON *canvas_id_item = cJSON_GetObjectItem(json, "canvas_id");
    if (!cJSON_IsString(canvas_id_item)) {
        cJSON_Delete(json);
        return send_error_response(req, 400, "Missing canvas_id field");
    }

    esp_err_t ret = pin_canvas_display(g_canvas_handle, canvas_id_item->valuestring);
    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to display canvas");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Canvas displayed successfully");

    return send_json_response(req, response, 200);
}

static esp_err_t canvas_element_add_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    char *body = get_request_body(req);
    if (!body) {
        return send_error_response(req, 400, "Invalid request body");
    }

    cJSON *json = cJSON_Parse(body);
    free(body);

    if (!json) {
        return send_error_response(req, 400, "Invalid JSON");
    }

    cJSON *canvas_id_item = cJSON_GetObjectItem(json, "canvas_id");
    cJSON *element_item = cJSON_GetObjectItem(json, "element");

    if (!cJSON_IsString(canvas_id_item) || !cJSON_IsObject(element_item)) {
        cJSON_Delete(json);
        return send_error_response(req, 400, "Missing canvas_id or element fields");
    }

    // Parse element from JSON (simplified parsing)
    pin_canvas_element_t element = {0};
    
    cJSON *id = cJSON_GetObjectItem(element_item, "id");
    cJSON *type = cJSON_GetObjectItem(element_item, "type");
    cJSON *x = cJSON_GetObjectItem(element_item, "x");
    cJSON *y = cJSON_GetObjectItem(element_item, "y");
    cJSON *width = cJSON_GetObjectItem(element_item, "width");
    cJSON *height = cJSON_GetObjectItem(element_item, "height");

    if (cJSON_IsString(id)) strncpy(element.id, id->valuestring, sizeof(element.id) - 1);
    if (cJSON_IsNumber(type)) element.type = (pin_canvas_element_type_t)type->valueint;
    if (cJSON_IsNumber(x)) element.bounds.position.x = (int16_t)x->valueint;
    if (cJSON_IsNumber(y)) element.bounds.position.y = (int16_t)y->valueint;
    if (cJSON_IsNumber(width)) element.bounds.size.width = (uint16_t)width->valueint;
    if (cJSON_IsNumber(height)) element.bounds.size.height = (uint16_t)height->valueint;

    element.visible = true;
    element.z_index = 0;

    esp_err_t ret = pin_canvas_add_element(g_canvas_handle, canvas_id_item->valuestring, &element);
    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to add element");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Element added successfully");

    return send_json_response(req, response, 201);
}

static esp_err_t image_upload_handler(httpd_req_t *req) {
    if (!g_canvas_handle) {
        return send_error_response(req, 500, "Canvas system not initialized");
    }

    // Get image_id from query parameter
    char query_str[128];
    char image_id[32] = {0};
    
    if (httpd_req_get_url_query_str(req, query_str, sizeof(query_str)) == ESP_OK) {
        char *id_param = strstr(query_str, "id=");
        if (id_param) {
            id_param += 3; // Skip "id="
            char *end = strchr(id_param, '&');
            int len = end ? (end - id_param) : strlen(id_param);
            len = (len < sizeof(image_id) - 1) ? len : sizeof(image_id) - 1;
            strncpy(image_id, id_param, len);
        }
    }

    if (strlen(image_id) == 0) {
        return send_error_response(req, 400, "Missing image_id parameter");
    }

    // Allocate buffer for image data
    size_t content_len = req->content_len;
    if (content_len > PIN_CANVAS_MAX_IMAGE_SIZE) {
        return send_error_response(req, 413, "Image too large");
    }

    uint8_t *buffer = malloc(content_len);
    if (!buffer) {
        return send_error_response(req, 500, "Failed to allocate memory");
    }

    // Read image data
    size_t received = 0;
    while (received < content_len) {
        size_t ret = httpd_req_recv(req, (char*)buffer + received, content_len - received);
        if (ret <= 0) {
            free(buffer);
            return send_error_response(req, 400, "Failed to receive image data");
        }
        received += ret;
    }

    // Detect image format (simple detection)
    pin_canvas_image_format_t format = PIN_CANVAS_IMAGE_BMP;
    if (content_len >= 8) {
        if (buffer[0] == 0x89 && buffer[1] == 0x50 && buffer[2] == 0x4E && buffer[3] == 0x47) {
            format = PIN_CANVAS_IMAGE_PNG;
        } else if (buffer[0] == 0xFF && buffer[1] == 0xD8) {
            format = PIN_CANVAS_IMAGE_JPG;
        }
    }

    esp_err_t ret = pin_canvas_store_image(g_canvas_handle, image_id, buffer, content_len, format);
    free(buffer);

    if (ret != ESP_OK) {
        return send_error_response(req, 500, "Failed to store image");
    }

    cJSON *response = cJSON_CreateObject();
    cJSON_AddStringToObject(response, "message", "Image uploaded successfully");
    cJSON_AddStringToObject(response, "image_id", image_id);
    cJSON_AddNumberToObject(response, "format", format);
    cJSON_AddNumberToObject(response, "size", content_len);

    return send_json_response(req, response, 201);
}

// Device status handler (basic implementation)
// Helper functions implementation
static esp_err_t send_json_response(httpd_req_t *req, cJSON *json, int status_code) {
    char *json_str = cJSON_Print(json);
    if (!json_str) {
        cJSON_Delete(json);
        return send_error_response(req, 500, "Failed to serialize JSON");
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_set_status(req, status_code == 200 ? "200 OK" : 
                              status_code == 201 ? "201 Created" :
                              status_code == 400 ? "400 Bad Request" :
                              status_code == 404 ? "404 Not Found" :
                              status_code == 500 ? "500 Internal Server Error" : "200 OK");
    
    esp_err_t ret = httpd_resp_send(req, json_str, strlen(json_str));
    
    free(json_str);
    cJSON_Delete(json);
    
    return ret;
}

static esp_err_t send_error_response(httpd_req_t *req, int status_code, const char *message) {
    cJSON *json = cJSON_CreateObject();
    cJSON_AddStringToObject(json, "error", message);
    cJSON_AddNumberToObject(json, "status", status_code);
    return send_json_response(req, json, status_code);
}

static char* get_request_body(httpd_req_t *req) {
    size_t content_len = req->content_len;
    if (content_len == 0) {
        return NULL;
    }

    char *buffer = malloc(content_len + 1);
    if (!buffer) {
        return NULL;
    }

    size_t received = 0;
    while (received < content_len) {
        size_t ret = httpd_req_recv(req, buffer + received, content_len - received);
        if (ret <= 0) {
            free(buffer);
            return NULL;
        }
        received += ret;
    }
    
    buffer[content_len] = '\0';
    return buffer;
}

esp_err_t pin_webserver_init(pin_canvas_handle_t canvas_handle) {
    g_canvas_handle = canvas_handle;
    ESP_LOGI(TAG, "Web server initialized with canvas handle");
    return ESP_OK;
}

esp_err_t pin_webserver_start(void) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;
    config.server_port = 80;

    ESP_LOGI(TAG, "Starting web server on port %d", config.server_port);

    if (httpd_start(&server, &config) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start web server");
        return ESP_FAIL;
    }

    // Static file handlers
    httpd_uri_t index_uri = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &index_uri);

    httpd_uri_t app_js_uri = {
        .uri = "/app.js",
        .method = HTTP_GET,
        .handler = app_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &app_js_uri);

    httpd_uri_t manifest_uri = {
        .uri = "/manifest.json",
        .method = HTTP_GET,
        .handler = manifest_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &manifest_uri);

    httpd_uri_t sw_uri = {
        .uri = "/sw.js",
        .method = HTTP_GET,
        .handler = sw_js_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &sw_uri);

    // API endpoints
    httpd_uri_t status_uri = {
        .uri = "/api/status",
        .method = HTTP_GET,
        .handler = api_status_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &status_uri);
    
    // Display control endpoints
    httpd_uri_t display_refresh_uri = {
        .uri = "/api/display/refresh",
        .method = HTTP_POST,
        .handler = api_display_refresh_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &display_refresh_uri);
    
    httpd_uri_t display_clear_uri = {
        .uri = "/api/display/clear", 
        .method = HTTP_POST,
        .handler = api_display_clear_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &display_clear_uri);

    // Canvas API endpoints
    httpd_uri_t canvas_list_uri = {
        .uri = "/api/canvas",
        .method = HTTP_GET,
        .handler = canvas_list_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_list_uri);

    httpd_uri_t canvas_create_uri = {
        .uri = "/api/canvas",
        .method = HTTP_POST,
        .handler = canvas_create_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_create_uri);

    httpd_uri_t canvas_get_uri = {
        .uri = "/api/canvas/get",
        .method = HTTP_GET,
        .handler = canvas_get_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_get_uri);

    httpd_uri_t canvas_update_uri = {
        .uri = "/api/canvas/update",
        .method = HTTP_PUT,
        .handler = canvas_update_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_update_uri);

    httpd_uri_t canvas_delete_uri = {
        .uri = "/api/canvas/delete",
        .method = HTTP_DELETE,
        .handler = canvas_delete_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_delete_uri);

    httpd_uri_t canvas_display_uri = {
        .uri = "/api/canvas/display",
        .method = HTTP_POST,
        .handler = canvas_display_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_display_uri);

    httpd_uri_t canvas_element_add_uri = {
        .uri = "/api/canvas/element",
        .method = HTTP_POST,
        .handler = canvas_element_add_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &canvas_element_add_uri);

    httpd_uri_t image_upload_uri = {
        .uri = "/api/images",
        .method = HTTP_POST,
        .handler = image_upload_handler,
        .user_ctx = NULL
    };
    httpd_register_uri_handler(server, &image_upload_uri);

    ESP_LOGI(TAG, "Web server started with Canvas API endpoints");
    return ESP_OK;
}

esp_err_t pin_webserver_stop(void) {
    if (server) {
        httpd_stop(server);
        server = NULL;
        ESP_LOGI(TAG, "Web server stopped");
    }
    return ESP_OK;
}

httpd_handle_t pin_webserver_get_handle(void) {
    return server;
}
/**
 * @file pin_canvas.c
 * @brief Pin Canvas API Implementation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_err.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cjson/cjson.h"
#include "pin_canvas.h"

static const char* TAG = "PIN_CANVAS";

#define NVS_CANVAS_NAMESPACE "pin_canvas"
#define NVS_IMAGE_NAMESPACE "pin_images"

// Internal canvas manager structure
struct pin_canvas_manager {
    fpc_a005_handle_t display_handle;
    SemaphoreHandle_t mutex;
    nvs_handle_t canvas_nvs_handle;
    nvs_handle_t image_nvs_handle;
    uint8_t* render_buffer;
    bool initialized;
};

// Static functions
static esp_err_t canvas_to_json(const pin_canvas_t* canvas, cJSON** json);
static esp_err_t json_to_canvas(const cJSON* json, pin_canvas_t* canvas);
static esp_err_t render_text_element(uint8_t* buffer, const pin_canvas_element_t* element);
static esp_err_t render_image_element(pin_canvas_handle_t handle, uint8_t* buffer, const pin_canvas_element_t* element);
static esp_err_t render_shape_element(uint8_t* buffer, const pin_canvas_element_t* element);
static void draw_pixel(uint8_t* buffer, int x, int y, pin_canvas_color_t color);
static void draw_line(uint8_t* buffer, int x0, int y0, int x1, int y1, pin_canvas_color_t color);
static void draw_rect(uint8_t* buffer, const pin_canvas_rect_t* rect, pin_canvas_color_t color, bool filled);
static void draw_circle(uint8_t* buffer, int cx, int cy, int radius, pin_canvas_color_t color, bool filled);

esp_err_t pin_canvas_init(fpc_a005_handle_t display_handle, pin_canvas_handle_t* handle) {
    if (!display_handle || !handle) {
        return ESP_ERR_INVALID_ARG;
    }

    struct pin_canvas_manager* manager = calloc(1, sizeof(struct pin_canvas_manager));
    if (!manager) {
        ESP_LOGE(TAG, "Failed to allocate canvas manager");
        return ESP_ERR_NO_MEM;
    }

    // Allocate render buffer
    manager->render_buffer = heap_caps_malloc(PIN_CANVAS_WIDTH * PIN_CANVAS_HEIGHT, MALLOC_CAP_8BIT);
    if (!manager->render_buffer) {
        ESP_LOGE(TAG, "Failed to allocate render buffer");
        free(manager);
        return ESP_ERR_NO_MEM;
    }

    // Create mutex
    manager->mutex = xSemaphoreCreateMutex();
    if (!manager->mutex) {
        ESP_LOGE(TAG, "Failed to create mutex");
        free(manager->render_buffer);
        free(manager);
        return ESP_ERR_NO_MEM;
    }

    // Open NVS handles
    esp_err_t ret = nvs_open(NVS_CANVAS_NAMESPACE, NVS_READWRITE, &manager->canvas_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open canvas NVS: %s", esp_err_to_name(ret));
        vSemaphoreDelete(manager->mutex);
        free(manager->render_buffer);
        free(manager);
        return ret;
    }

    ret = nvs_open(NVS_IMAGE_NAMESPACE, NVS_READWRITE, &manager->image_nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open image NVS: %s", esp_err_to_name(ret));
        nvs_close(manager->canvas_nvs_handle);
        vSemaphoreDelete(manager->mutex);
        free(manager->render_buffer);
        free(manager);
        return ret;
    }

    manager->display_handle = display_handle;
    manager->initialized = true;
    *handle = manager;

    ESP_LOGI(TAG, "Canvas system initialized");
    return ESP_OK;
}

esp_err_t pin_canvas_deinit(pin_canvas_handle_t handle) {
    if (!handle || !handle->initialized) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_close(handle->canvas_nvs_handle);
    nvs_close(handle->image_nvs_handle);
    vSemaphoreDelete(handle->mutex);
    free(handle->render_buffer);
    free(handle);

    ESP_LOGI(TAG, "Canvas system deinitialized");
    return ESP_OK;
}

esp_err_t pin_canvas_create(pin_canvas_handle_t handle, const char* canvas_id, const char* name) {
    if (!handle || !handle->initialized || !canvas_id || !name) {
        return ESP_ERR_INVALID_ARG;
    }

    if (strlen(canvas_id) >= 32 || strlen(name) >= 64) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    pin_canvas_t canvas = {0};
    strncpy(canvas.id, canvas_id, sizeof(canvas.id) - 1);
    strncpy(canvas.name, name, sizeof(canvas.name) - 1);
    canvas.background_color = PIN_CANVAS_COLOR_WHITE;
    canvas.created_time = (uint32_t)time(NULL);
    canvas.modified_time = canvas.created_time;
    canvas.element_count = 0;

    esp_err_t ret = nvs_set_blob(handle->canvas_nvs_handle, canvas_id, &canvas, sizeof(canvas));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle->canvas_nvs_handle);
    }

    xSemaphoreGive(handle->mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Created canvas: %s (%s)", canvas_id, name);
    } else {
        ESP_LOGE(TAG, "Failed to create canvas: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_delete(pin_canvas_handle_t handle, const char* canvas_id) {
    if (!handle || !handle->initialized || !canvas_id) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    esp_err_t ret = nvs_erase_key(handle->canvas_nvs_handle, canvas_id);
    if (ret == ESP_OK) {
        ret = nvs_commit(handle->canvas_nvs_handle);
    }

    xSemaphoreGive(handle->mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Deleted canvas: %s", canvas_id);
    } else {
        ESP_LOGE(TAG, "Failed to delete canvas: %s", esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_get(pin_canvas_handle_t handle, const char* canvas_id, pin_canvas_t* canvas) {
    if (!handle || !handle->initialized || !canvas_id || !canvas) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    size_t required_size = sizeof(pin_canvas_t);
    esp_err_t ret = nvs_get_blob(handle->canvas_nvs_handle, canvas_id, canvas, &required_size);

    xSemaphoreGive(handle->mutex);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get canvas %s: %s", canvas_id, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_update(pin_canvas_handle_t handle, const pin_canvas_t* canvas) {
    if (!handle || !handle->initialized || !canvas) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    pin_canvas_t updated_canvas = *canvas;
    updated_canvas.modified_time = (uint32_t)time(NULL);

    esp_err_t ret = nvs_set_blob(handle->canvas_nvs_handle, canvas->id, &updated_canvas, sizeof(updated_canvas));
    if (ret == ESP_OK) {
        ret = nvs_commit(handle->canvas_nvs_handle);
    }

    xSemaphoreGive(handle->mutex);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update canvas %s: %s", canvas->id, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_add_element(pin_canvas_handle_t handle, const char* canvas_id, const pin_canvas_element_t* element) {
    if (!handle || !handle->initialized || !canvas_id || !element) {
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = pin_canvas_get(handle, canvas_id, &canvas);
    if (ret != ESP_OK) {
        return ret;
    }

    if (canvas.element_count >= PIN_CANVAS_MAX_ELEMENTS) {
        ESP_LOGE(TAG, "Canvas %s is full", canvas_id);
        return ESP_ERR_NO_MEM;
    }

    // Check for duplicate element ID
    for (int i = 0; i < canvas.element_count; i++) {
        if (strcmp(canvas.elements[i].id, element->id) == 0) {
            ESP_LOGE(TAG, "Element %s already exists in canvas %s", element->id, canvas_id);
            return ESP_ERR_INVALID_STATE;
        }
    }

    canvas.elements[canvas.element_count] = *element;
    canvas.element_count++;

    return pin_canvas_update(handle, &canvas);
}

esp_err_t pin_canvas_update_element(pin_canvas_handle_t handle, const char* canvas_id, const char* element_id, const pin_canvas_element_t* element) {
    if (!handle || !handle->initialized || !canvas_id || !element_id || !element) {
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = pin_canvas_get(handle, canvas_id, &canvas);
    if (ret != ESP_OK) {
        return ret;
    }

    // Find element
    int element_index = -1;
    for (int i = 0; i < canvas.element_count; i++) {
        if (strcmp(canvas.elements[i].id, element_id) == 0) {
            element_index = i;
            break;
        }
    }

    if (element_index == -1) {
        ESP_LOGE(TAG, "Element %s not found in canvas %s", element_id, canvas_id);
        return ESP_ERR_NOT_FOUND;
    }

    canvas.elements[element_index] = *element;
    return pin_canvas_update(handle, &canvas);
}

esp_err_t pin_canvas_remove_element(pin_canvas_handle_t handle, const char* canvas_id, const char* element_id) {
    if (!handle || !handle->initialized || !canvas_id || !element_id) {
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = pin_canvas_get(handle, canvas_id, &canvas);
    if (ret != ESP_OK) {
        return ret;
    }

    // Find element
    int element_index = -1;
    for (int i = 0; i < canvas.element_count; i++) {
        if (strcmp(canvas.elements[i].id, element_id) == 0) {
            element_index = i;
            break;
        }
    }

    if (element_index == -1) {
        ESP_LOGE(TAG, "Element %s not found in canvas %s", element_id, canvas_id);
        return ESP_ERR_NOT_FOUND;
    }

    // Remove element by shifting remaining elements
    for (int i = element_index; i < canvas.element_count - 1; i++) {
        canvas.elements[i] = canvas.elements[i + 1];
    }
    canvas.element_count--;

    return pin_canvas_update(handle, &canvas);
}

esp_err_t pin_canvas_store_image(pin_canvas_handle_t handle, const char* image_id, const uint8_t* data, size_t size, pin_canvas_image_format_t format) {
    if (!handle || !handle->initialized || !image_id || !data || size == 0 || size > PIN_CANVAS_MAX_IMAGE_SIZE) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // Store image metadata and data
    struct {
        pin_canvas_image_format_t format;
        size_t size;
        uint32_t stored_time;
    } image_meta = {
        .format = format,
        .size = size,
        .stored_time = (uint32_t)time(NULL)
    };

    char meta_key[64];
    snprintf(meta_key, sizeof(meta_key), "%s_meta", image_id);

    esp_err_t ret = nvs_set_blob(handle->image_nvs_handle, meta_key, &image_meta, sizeof(image_meta));
    if (ret == ESP_OK) {
        ret = nvs_set_blob(handle->image_nvs_handle, image_id, data, size);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle->image_nvs_handle);
    }

    xSemaphoreGive(handle->mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Stored image %s (%zu bytes)", image_id, size);
    } else {
        ESP_LOGE(TAG, "Failed to store image %s: %s", image_id, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_delete_image(pin_canvas_handle_t handle, const char* image_id) {
    if (!handle || !handle->initialized || !image_id) {
        return ESP_ERR_INVALID_ARG;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    char meta_key[64];
    snprintf(meta_key, sizeof(meta_key), "%s_meta", image_id);

    esp_err_t ret = nvs_erase_key(handle->image_nvs_handle, meta_key);
    if (ret == ESP_OK || ret == ESP_ERR_NVS_NOT_FOUND) {
        ret = nvs_erase_key(handle->image_nvs_handle, image_id);
    }
    if (ret == ESP_OK) {
        ret = nvs_commit(handle->image_nvs_handle);
    }

    xSemaphoreGive(handle->mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Deleted image: %s", image_id);
    } else if (ret != ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGE(TAG, "Failed to delete image %s: %s", image_id, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_render(pin_canvas_handle_t handle, const char* canvas_id, uint8_t* buffer) {
    if (!handle || !handle->initialized || !canvas_id || !buffer) {
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = pin_canvas_get(handle, canvas_id, &canvas);
    if (ret != ESP_OK) {
        return ret;
    }

    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    // Clear buffer with background color
    memset(buffer, canvas.background_color, PIN_CANVAS_WIDTH * PIN_CANVAS_HEIGHT);

    // Sort elements by z_index
    pin_canvas_element_t* sorted_elements[PIN_CANVAS_MAX_ELEMENTS];
    for (int i = 0; i < canvas.element_count; i++) {
        sorted_elements[i] = &canvas.elements[i];
    }

    // Simple bubble sort by z_index
    for (int i = 0; i < canvas.element_count - 1; i++) {
        for (int j = 0; j < canvas.element_count - i - 1; j++) {
            if (sorted_elements[j]->z_index > sorted_elements[j + 1]->z_index) {
                pin_canvas_element_t* temp = sorted_elements[j];
                sorted_elements[j] = sorted_elements[j + 1];
                sorted_elements[j + 1] = temp;
            }
        }
    }

    // Render elements in z_index order
    for (int i = 0; i < canvas.element_count; i++) {
        if (!sorted_elements[i]->visible) {
            continue;
        }

        switch (sorted_elements[i]->type) {
            case PIN_CANVAS_ELEMENT_TEXT:
                render_text_element(buffer, sorted_elements[i]);
                break;
            case PIN_CANVAS_ELEMENT_IMAGE:
                render_image_element(handle, buffer, sorted_elements[i]);
                break;
            case PIN_CANVAS_ELEMENT_RECT:
            case PIN_CANVAS_ELEMENT_LINE:
            case PIN_CANVAS_ELEMENT_CIRCLE:
                render_shape_element(buffer, sorted_elements[i]);
                break;
        }
    }

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Rendered canvas %s with %d elements", canvas_id, canvas.element_count);
    return ESP_OK;
}

esp_err_t pin_canvas_display(pin_canvas_handle_t handle, const char* canvas_id) {
    if (!handle || !handle->initialized || !canvas_id) {
        return ESP_ERR_INVALID_ARG;
    }

    esp_err_t ret = pin_canvas_render(handle, canvas_id, handle->render_buffer);
    if (ret != ESP_OK) {
        return ret;
    }

    // Convert buffer to display format and update display
    ret = fpc_a005_draw_bitmap(handle->display_handle, 0, 0, PIN_CANVAS_WIDTH, PIN_CANVAS_HEIGHT, handle->render_buffer);
    if (ret == ESP_OK) {
        ret = fpc_a005_display(handle->display_handle);
    }

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Displayed canvas: %s", canvas_id);
    } else {
        ESP_LOGE(TAG, "Failed to display canvas %s: %s", canvas_id, esp_err_to_name(ret));
    }

    return ret;
}

esp_err_t pin_canvas_list(pin_canvas_handle_t handle, char canvas_ids[][32], size_t max_count, size_t* count) {
    if (!handle || !handle->initialized || !canvas_ids || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    *count = 0;
    xSemaphoreTake(handle->mutex, portMAX_DELAY);

    nvs_iterator_t iter = nvs_entry_find(NVS_CANVAS_NAMESPACE, NULL, NVS_TYPE_BLOB);
    while (iter != NULL && *count < max_count) {
        nvs_entry_info_t info;
        nvs_entry_info(iter, &info);
        
        strncpy(canvas_ids[*count], info.key, 31);
        canvas_ids[*count][31] = '\0';
        (*count)++;
        
        iter = nvs_entry_next(iter);
    }

    xSemaphoreGive(handle->mutex);

    ESP_LOGI(TAG, "Listed %zu canvases", *count);
    return ESP_OK;
}

esp_err_t pin_canvas_export_json(pin_canvas_handle_t handle, const char* canvas_id, char** json_str) {
    if (!handle || !handle->initialized || !canvas_id || !json_str) {
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = pin_canvas_get(handle, canvas_id, &canvas);
    if (ret != ESP_OK) {
        return ret;
    }

    cJSON* json = NULL;
    ret = canvas_to_json(&canvas, &json);
    if (ret != ESP_OK) {
        return ret;
    }

    *json_str = cJSON_Print(json);
    cJSON_Delete(json);

    if (!*json_str) {
        ESP_LOGE(TAG, "Failed to serialize canvas to JSON");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Exported canvas %s to JSON", canvas_id);
    return ESP_OK;
}

esp_err_t pin_canvas_import_json(pin_canvas_handle_t handle, const char* json_str) {
    if (!handle || !handle->initialized || !json_str) {
        return ESP_ERR_INVALID_ARG;
    }

    cJSON* json = cJSON_Parse(json_str);
    if (!json) {
        ESP_LOGE(TAG, "Failed to parse JSON");
        return ESP_ERR_INVALID_ARG;
    }

    pin_canvas_t canvas;
    esp_err_t ret = json_to_canvas(json, &canvas);
    cJSON_Delete(json);

    if (ret != ESP_OK) {
        return ret;
    }

    ret = pin_canvas_update(handle, &canvas);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Imported canvas %s from JSON", canvas.id);
    }

    return ret;
}

// Static helper functions implementation
static esp_err_t canvas_to_json(const pin_canvas_t* canvas, cJSON** json) {
    *json = cJSON_CreateObject();
    if (!*json) return ESP_ERR_NO_MEM;

    cJSON_AddStringToObject(*json, "id", canvas->id);
    cJSON_AddStringToObject(*json, "name", canvas->name);
    cJSON_AddNumberToObject(*json, "background_color", canvas->background_color);
    cJSON_AddNumberToObject(*json, "created_time", canvas->created_time);
    cJSON_AddNumberToObject(*json, "modified_time", canvas->modified_time);

    cJSON* elements = cJSON_CreateArray();
    for (int i = 0; i < canvas->element_count; i++) {
        cJSON* element = cJSON_CreateObject();
        const pin_canvas_element_t* elem = &canvas->elements[i];
        
        cJSON_AddStringToObject(element, "id", elem->id);
        cJSON_AddNumberToObject(element, "type", elem->type);
        cJSON_AddNumberToObject(element, "x", elem->bounds.position.x);
        cJSON_AddNumberToObject(element, "y", elem->bounds.position.y);
        cJSON_AddNumberToObject(element, "width", elem->bounds.size.width);
        cJSON_AddNumberToObject(element, "height", elem->bounds.size.height);
        cJSON_AddNumberToObject(element, "z_index", elem->z_index);
        cJSON_AddBoolToObject(element, "visible", elem->visible);

        // Add element-specific properties based on type
        cJSON* props = cJSON_CreateObject();
        switch (elem->type) {
            case PIN_CANVAS_ELEMENT_TEXT:
                cJSON_AddStringToObject(props, "text", elem->props.text.text);
                cJSON_AddNumberToObject(props, "font_size", elem->props.text.font_size);
                cJSON_AddNumberToObject(props, "color", elem->props.text.color);
                cJSON_AddNumberToObject(props, "align", elem->props.text.align);
                cJSON_AddBoolToObject(props, "bold", elem->props.text.bold);
                cJSON_AddBoolToObject(props, "italic", elem->props.text.italic);
                break;
            case PIN_CANVAS_ELEMENT_IMAGE:
                cJSON_AddStringToObject(props, "image_id", elem->props.image.image_id);
                cJSON_AddNumberToObject(props, "format", elem->props.image.format);
                cJSON_AddBoolToObject(props, "maintain_aspect_ratio", elem->props.image.maintain_aspect_ratio);
                cJSON_AddNumberToObject(props, "opacity", elem->props.image.opacity);
                break;
            default:
                cJSON_AddNumberToObject(props, "fill_color", elem->props.shape.fill_color);
                cJSON_AddNumberToObject(props, "border_color", elem->props.shape.border_color);
                cJSON_AddNumberToObject(props, "border_width", elem->props.shape.border_width);
                cJSON_AddBoolToObject(props, "filled", elem->props.shape.filled);
                break;
        }
        cJSON_AddItemToObject(element, "props", props);
        cJSON_AddItemToArray(elements, element);
    }
    cJSON_AddItemToObject(*json, "elements", elements);

    return ESP_OK;
}

static esp_err_t json_to_canvas(const cJSON* json, pin_canvas_t* canvas) {
    memset(canvas, 0, sizeof(pin_canvas_t));

    cJSON* item = cJSON_GetObjectItem(json, "id");
    if (cJSON_IsString(item)) {
        strncpy(canvas->id, item->valuestring, sizeof(canvas->id) - 1);
    }

    item = cJSON_GetObjectItem(json, "name");
    if (cJSON_IsString(item)) {
        strncpy(canvas->name, item->valuestring, sizeof(canvas->name) - 1);
    }

    item = cJSON_GetObjectItem(json, "background_color");
    if (cJSON_IsNumber(item)) {
        canvas->background_color = (pin_canvas_color_t)item->valueint;
    }

    item = cJSON_GetObjectItem(json, "created_time");
    if (cJSON_IsNumber(item)) {
        canvas->created_time = (uint32_t)item->valueint;
    }

    item = cJSON_GetObjectItem(json, "modified_time");
    if (cJSON_IsNumber(item)) {
        canvas->modified_time = (uint32_t)item->valueint;
    }

    cJSON* elements = cJSON_GetObjectItem(json, "elements");
    if (cJSON_IsArray(elements)) {
        int count = cJSON_GetArraySize(elements);
        canvas->element_count = (count > PIN_CANVAS_MAX_ELEMENTS) ? PIN_CANVAS_MAX_ELEMENTS : count;

        for (int i = 0; i < canvas->element_count; i++) {
            cJSON* element = cJSON_GetArrayItem(elements, i);
            pin_canvas_element_t* elem = &canvas->elements[i];

            item = cJSON_GetObjectItem(element, "id");
            if (cJSON_IsString(item)) {
                strncpy(elem->id, item->valuestring, sizeof(elem->id) - 1);
            }

            item = cJSON_GetObjectItem(element, "type");
            if (cJSON_IsNumber(item)) {
                elem->type = (pin_canvas_element_type_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "x");
            if (cJSON_IsNumber(item)) {
                elem->bounds.position.x = (int16_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "y");
            if (cJSON_IsNumber(item)) {
                elem->bounds.position.y = (int16_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "width");
            if (cJSON_IsNumber(item)) {
                elem->bounds.size.width = (uint16_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "height");
            if (cJSON_IsNumber(item)) {
                elem->bounds.size.height = (uint16_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "z_index");
            if (cJSON_IsNumber(item)) {
                elem->z_index = (uint8_t)item->valueint;
            }

            item = cJSON_GetObjectItem(element, "visible");
            if (cJSON_IsBool(item)) {
                elem->visible = cJSON_IsTrue(item);
            }

            // Parse element-specific properties
            cJSON* props = cJSON_GetObjectItem(element, "props");
            if (cJSON_IsObject(props)) {
                switch (elem->type) {
                    case PIN_CANVAS_ELEMENT_TEXT:
                        item = cJSON_GetObjectItem(props, "text");
                        if (cJSON_IsString(item)) {
                            strncpy(elem->props.text.text, item->valuestring, sizeof(elem->props.text.text) - 1);
                        }
                        item = cJSON_GetObjectItem(props, "font_size");
                        if (cJSON_IsNumber(item)) {
                            elem->props.text.font_size = (pin_canvas_font_size_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "color");
                        if (cJSON_IsNumber(item)) {
                            elem->props.text.color = (pin_canvas_color_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "align");
                        if (cJSON_IsNumber(item)) {
                            elem->props.text.align = (pin_canvas_text_align_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "bold");
                        if (cJSON_IsBool(item)) {
                            elem->props.text.bold = cJSON_IsTrue(item);
                        }
                        item = cJSON_GetObjectItem(props, "italic");
                        if (cJSON_IsBool(item)) {
                            elem->props.text.italic = cJSON_IsTrue(item);
                        }
                        break;
                    case PIN_CANVAS_ELEMENT_IMAGE:
                        item = cJSON_GetObjectItem(props, "image_id");
                        if (cJSON_IsString(item)) {
                            strncpy(elem->props.image.image_id, item->valuestring, sizeof(elem->props.image.image_id) - 1);
                        }
                        item = cJSON_GetObjectItem(props, "format");
                        if (cJSON_IsNumber(item)) {
                            elem->props.image.format = (pin_canvas_image_format_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "maintain_aspect_ratio");
                        if (cJSON_IsBool(item)) {
                            elem->props.image.maintain_aspect_ratio = cJSON_IsTrue(item);
                        }
                        item = cJSON_GetObjectItem(props, "opacity");
                        if (cJSON_IsNumber(item)) {
                            elem->props.image.opacity = (uint8_t)item->valueint;
                        }
                        break;
                    default:
                        item = cJSON_GetObjectItem(props, "fill_color");
                        if (cJSON_IsNumber(item)) {
                            elem->props.shape.fill_color = (pin_canvas_color_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "border_color");
                        if (cJSON_IsNumber(item)) {
                            elem->props.shape.border_color = (pin_canvas_color_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "border_width");
                        if (cJSON_IsNumber(item)) {
                            elem->props.shape.border_width = (uint8_t)item->valueint;
                        }
                        item = cJSON_GetObjectItem(props, "filled");
                        if (cJSON_IsBool(item)) {
                            elem->props.shape.filled = cJSON_IsTrue(item);
                        }
                        break;
                }
            }
        }
    }

    return ESP_OK;
}

static esp_err_t render_text_element(uint8_t* buffer, const pin_canvas_element_t* element) {
    // Simple text rendering implementation
    // In a real implementation, this would use a font library like FreeType
    const pin_canvas_text_props_t* text = &element->props.text;
    
    // Calculate approximate character size based on font size
    int char_width = text->font_size / 2;
    int char_height = text->font_size;
    
    int x_start = element->bounds.position.x;
    int y_start = element->bounds.position.y;
    
    // Apply text alignment
    int text_width = strlen(text->text) * char_width;
    switch (text->align) {
        case PIN_CANVAS_ALIGN_CENTER:
            x_start = element->bounds.position.x + (element->bounds.size.width - text_width) / 2;
            break;
        case PIN_CANVAS_ALIGN_RIGHT:
            x_start = element->bounds.position.x + element->bounds.size.width - text_width;
            break;
        case PIN_CANVAS_ALIGN_LEFT:
        default:
            break;
    }
    
    // Render each character as a simple rectangle (placeholder implementation)
    for (int i = 0; i < strlen(text->text) && i < 50; i++) {
        pin_canvas_rect_t char_rect = {
            .position = {x_start + i * char_width, y_start},
            .size = {char_width - 1, char_height}
        };
        draw_rect(buffer, &char_rect, text->color, true);
    }
    
    return ESP_OK;
}

static esp_err_t render_image_element(pin_canvas_handle_t handle, uint8_t* buffer, const pin_canvas_element_t* element) {
    // Placeholder image rendering - would need proper image decoding
    const pin_canvas_image_props_t* image = &element->props.image;
    
    // For now, just draw a placeholder rectangle
    draw_rect(buffer, &element->bounds, PIN_CANVAS_COLOR_BLUE, false);
    
    // Draw diagonal lines to indicate it's an image placeholder
    draw_line(buffer, 
              element->bounds.position.x, 
              element->bounds.position.y,
              element->bounds.position.x + element->bounds.size.width,
              element->bounds.position.y + element->bounds.size.height,
              PIN_CANVAS_COLOR_BLUE);
    
    draw_line(buffer,
              element->bounds.position.x + element->bounds.size.width,
              element->bounds.position.y,
              element->bounds.position.x,
              element->bounds.position.y + element->bounds.size.height,
              PIN_CANVAS_COLOR_BLUE);
    
    return ESP_OK;
}

static esp_err_t render_shape_element(uint8_t* buffer, const pin_canvas_element_t* element) {
    const pin_canvas_shape_props_t* shape = &element->props.shape;
    
    switch (element->type) {
        case PIN_CANVAS_ELEMENT_RECT:
            draw_rect(buffer, &element->bounds, shape->fill_color, shape->filled);
            if (shape->border_width > 0) {
                draw_rect(buffer, &element->bounds, shape->border_color, false);
            }
            break;
            
        case PIN_CANVAS_ELEMENT_LINE:
            draw_line(buffer,
                     element->bounds.position.x,
                     element->bounds.position.y,
                     element->bounds.position.x + element->bounds.size.width,
                     element->bounds.position.y + element->bounds.size.height,
                     shape->fill_color);
            break;
            
        case PIN_CANVAS_ELEMENT_CIRCLE: {
            int cx = element->bounds.position.x + element->bounds.size.width / 2;
            int cy = element->bounds.position.y + element->bounds.size.height / 2;
            int radius = (element->bounds.size.width < element->bounds.size.height) 
                        ? element->bounds.size.width / 2 
                        : element->bounds.size.height / 2;
            draw_circle(buffer, cx, cy, radius, shape->fill_color, shape->filled);
            break;
        }
        
        default:
            break;
    }
    
    return ESP_OK;
}

static void draw_pixel(uint8_t* buffer, int x, int y, pin_canvas_color_t color) {
    if (x >= 0 && x < PIN_CANVAS_WIDTH && y >= 0 && y < PIN_CANVAS_HEIGHT) {
        buffer[y * PIN_CANVAS_WIDTH + x] = (uint8_t)color;
    }
}

static void draw_line(uint8_t* buffer, int x0, int y0, int x1, int y1, pin_canvas_color_t color) {
    int dx = abs(x1 - x0);
    int dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    
    while (true) {
        draw_pixel(buffer, x0, y0, color);
        
        if (x0 == x1 && y0 == y1) break;
        
        int e2 = 2 * err;
        if (e2 > -dy) {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx) {
            err += dx;
            y0 += sy;
        }
    }
}

static void draw_rect(uint8_t* buffer, const pin_canvas_rect_t* rect, pin_canvas_color_t color, bool filled) {
    if (filled) {
        for (int y = rect->position.y; y < rect->position.y + rect->size.height; y++) {
            for (int x = rect->position.x; x < rect->position.x + rect->size.width; x++) {
                draw_pixel(buffer, x, y, color);
            }
        }
    } else {
        // Draw rectangle outline
        for (int x = rect->position.x; x < rect->position.x + rect->size.width; x++) {
            draw_pixel(buffer, x, rect->position.y, color);
            draw_pixel(buffer, x, rect->position.y + rect->size.height - 1, color);
        }
        for (int y = rect->position.y; y < rect->position.y + rect->size.height; y++) {
            draw_pixel(buffer, rect->position.x, y, color);
            draw_pixel(buffer, rect->position.x + rect->size.width - 1, y, color);
        }
    }
}

static void draw_circle(uint8_t* buffer, int cx, int cy, int radius, pin_canvas_color_t color, bool filled) {
    int x = 0;
    int y = radius;
    int d = 3 - 2 * radius;
    
    while (y >= x) {
        if (filled) {
            draw_line(buffer, cx - x, cy - y, cx + x, cy - y, color);
            draw_line(buffer, cx - x, cy + y, cx + x, cy + y, color);
            draw_line(buffer, cx - y, cy - x, cx + y, cy - x, color);
            draw_line(buffer, cx - y, cy + x, cx + y, cy + x, color);
        } else {
            draw_pixel(buffer, cx + x, cy + y, color);
            draw_pixel(buffer, cx - x, cy + y, color);
            draw_pixel(buffer, cx + x, cy - y, color);
            draw_pixel(buffer, cx - x, cy - y, color);
            draw_pixel(buffer, cx + y, cy + x, color);
            draw_pixel(buffer, cx - y, cy + x, color);
            draw_pixel(buffer, cx + y, cy - x, color);
            draw_pixel(buffer, cx - y, cy - x, color);
        }
        x++;
        if (d > 0) {
            y--;
            d = d + 4 * (x - y) + 10;
        } else {
            d = d + 4 * x + 6;
        }
    }
}
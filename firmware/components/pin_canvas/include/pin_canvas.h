/**
 * @file pin_canvas.h
 * @brief Pin Canvas API - Flexible content display system
 * 
 * Provides a canvas-like interface for displaying text, images, and mixed content
 * on the e-ink display with precise positioning and styling control.
 */

#ifndef PIN_CANVAS_H
#define PIN_CANVAS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "fpc_a005.h"

#ifdef __cplusplus
extern "C" {
#endif

// Canvas dimensions (matches FPC-A005 display)
#define PIN_CANVAS_WIDTH  600
#define PIN_CANVAS_HEIGHT 448

// Maximum elements per canvas
#define PIN_CANVAS_MAX_ELEMENTS 50

// Maximum text length per element
#define PIN_CANVAS_MAX_TEXT_LEN 512

// Maximum image size (64KB)
#define PIN_CANVAS_MAX_IMAGE_SIZE (64 * 1024)

// Canvas element types
typedef enum {
    PIN_CANVAS_ELEMENT_TEXT = 0,
    PIN_CANVAS_ELEMENT_IMAGE,
    PIN_CANVAS_ELEMENT_RECT,
    PIN_CANVAS_ELEMENT_LINE,
    PIN_CANVAS_ELEMENT_CIRCLE
} pin_canvas_element_type_t;

// Text alignment options
typedef enum {
    PIN_CANVAS_ALIGN_LEFT = 0,
    PIN_CANVAS_ALIGN_CENTER,
    PIN_CANVAS_ALIGN_RIGHT
} pin_canvas_text_align_t;

// Font sizes
typedef enum {
    PIN_CANVAS_FONT_SMALL = 12,
    PIN_CANVAS_FONT_MEDIUM = 16,
    PIN_CANVAS_FONT_LARGE = 24,
    PIN_CANVAS_FONT_XLARGE = 32
} pin_canvas_font_size_t;

// Colors (matching FPC-A005 7-color capability)
typedef enum {
    PIN_CANVAS_COLOR_BLACK = 0,
    PIN_CANVAS_COLOR_WHITE,
    PIN_CANVAS_COLOR_RED,
    PIN_CANVAS_COLOR_YELLOW,
    PIN_CANVAS_COLOR_BLUE,
    PIN_CANVAS_COLOR_GREEN,
    PIN_CANVAS_COLOR_ORANGE
} pin_canvas_color_t;

// Image formats
typedef enum {
    PIN_CANVAS_IMAGE_BMP = 0,
    PIN_CANVAS_IMAGE_PNG,
    PIN_CANVAS_IMAGE_JPG
} pin_canvas_image_format_t;

// Position structure
typedef struct {
    int16_t x;
    int16_t y;
} pin_canvas_point_t;

// Size structure
typedef struct {
    uint16_t width;
    uint16_t height;
} pin_canvas_size_t;

// Rectangle structure
typedef struct {
    pin_canvas_point_t position;
    pin_canvas_size_t size;
} pin_canvas_rect_t;

// Text element properties
typedef struct {
    char text[PIN_CANVAS_MAX_TEXT_LEN];
    pin_canvas_font_size_t font_size;
    pin_canvas_color_t color;
    pin_canvas_text_align_t align;
    bool bold;
    bool italic;
} pin_canvas_text_props_t;

// Image element properties
typedef struct {
    char image_id[32];  // Reference to stored image
    pin_canvas_image_format_t format;
    bool maintain_aspect_ratio;
    uint8_t opacity;  // 0-255
} pin_canvas_image_props_t;

// Shape element properties
typedef struct {
    pin_canvas_color_t fill_color;
    pin_canvas_color_t border_color;
    uint8_t border_width;
    bool filled;
} pin_canvas_shape_props_t;

// Canvas element union
typedef struct {
    char id[32];  // Unique element identifier
    pin_canvas_element_type_t type;
    pin_canvas_rect_t bounds;  // Position and size
    uint8_t z_index;  // Layer order (0-255)
    bool visible;
    
    union {
        pin_canvas_text_props_t text;
        pin_canvas_image_props_t image;
        pin_canvas_shape_props_t shape;
    } props;
} pin_canvas_element_t;

// Canvas structure
typedef struct {
    char id[32];  // Canvas identifier
    char name[64];  // Human-readable name
    pin_canvas_color_t background_color;
    uint32_t created_time;
    uint32_t modified_time;
    uint16_t element_count;
    pin_canvas_element_t elements[PIN_CANVAS_MAX_ELEMENTS];
} pin_canvas_t;

// Canvas manager handle
typedef struct pin_canvas_manager* pin_canvas_handle_t;

// Render callback function
typedef esp_err_t (*pin_canvas_render_callback_t)(const uint8_t* buffer, size_t size);

/**
 * @brief Initialize the canvas system
 * 
 * @param display_handle FPC-A005 display handle
 * @param handle Output canvas manager handle
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_init(fpc_a005_handle_t display_handle, pin_canvas_handle_t* handle);

/**
 * @brief Deinitialize the canvas system
 * 
 * @param handle Canvas manager handle
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_deinit(pin_canvas_handle_t handle);

/**
 * @brief Create a new canvas
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Unique canvas identifier
 * @param name Canvas display name
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_create(pin_canvas_handle_t handle, const char* canvas_id, const char* name);

/**
 * @brief Delete a canvas
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_delete(pin_canvas_handle_t handle, const char* canvas_id);

/**
 * @brief Get canvas by ID
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param canvas Output canvas structure
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_get(pin_canvas_handle_t handle, const char* canvas_id, pin_canvas_t* canvas);

/**
 * @brief Update canvas properties
 * 
 * @param handle Canvas manager handle
 * @param canvas Canvas structure to save
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_update(pin_canvas_handle_t handle, const pin_canvas_t* canvas);

/**
 * @brief Add element to canvas
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param element Element to add
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_add_element(pin_canvas_handle_t handle, const char* canvas_id, const pin_canvas_element_t* element);

/**
 * @brief Update element in canvas
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param element_id Element identifier
 * @param element Updated element data
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_update_element(pin_canvas_handle_t handle, const char* canvas_id, const char* element_id, const pin_canvas_element_t* element);

/**
 * @brief Remove element from canvas
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param element_id Element identifier
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_remove_element(pin_canvas_handle_t handle, const char* canvas_id, const char* element_id);

/**
 * @brief Store image data for canvas elements
 * 
 * @param handle Canvas manager handle
 * @param image_id Unique image identifier
 * @param data Image data buffer
 * @param size Image data size
 * @param format Image format
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_store_image(pin_canvas_handle_t handle, const char* image_id, const uint8_t* data, size_t size, pin_canvas_image_format_t format);

/**
 * @brief Delete stored image
 * 
 * @param handle Canvas manager handle
 * @param image_id Image identifier
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_delete_image(pin_canvas_handle_t handle, const char* image_id);

/**
 * @brief Render canvas to display buffer
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param buffer Output buffer (must be PIN_CANVAS_WIDTH * PIN_CANVAS_HEIGHT bytes)
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_render(pin_canvas_handle_t handle, const char* canvas_id, uint8_t* buffer);

/**
 * @brief Display canvas on screen
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_display(pin_canvas_handle_t handle, const char* canvas_id);

/**
 * @brief List all canvases
 * 
 * @param handle Canvas manager handle
 * @param canvas_ids Array to store canvas IDs (must be allocated by caller)
 * @param max_count Maximum number of canvas IDs to return
 * @param count Actual number of canvas IDs returned
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_list(pin_canvas_handle_t handle, char canvas_ids[][32], size_t max_count, size_t* count);

/**
 * @brief Export canvas as JSON
 * 
 * @param handle Canvas manager handle
 * @param canvas_id Canvas identifier
 * @param json_str Output JSON string (allocated by function, must be freed by caller)
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_export_json(pin_canvas_handle_t handle, const char* canvas_id, char** json_str);

/**
 * @brief Import canvas from JSON
 * 
 * @param handle Canvas manager handle
 * @param json_str JSON string containing canvas data
 * @return ESP_OK on success
 */
esp_err_t pin_canvas_import_json(pin_canvas_handle_t handle, const char* json_str);

#ifdef __cplusplus
}
#endif

#endif // PIN_CANVAS_H
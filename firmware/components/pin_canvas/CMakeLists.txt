idf_component_register(SRCS "pin_canvas.c"
                    INCLUDE_DIRS "include"
                    REQUIRES esp_common nvs_flash fpc_a005 json esp_http_server)
idf_component_register(SRCS "pin_plugin_api.c"
                       INCLUDE_DIRS "include"
                       REQUIRES esp_timer
                                esp_wifi
                                esp_http_client
                                nvs_flash
                                json)
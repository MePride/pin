idf_component_register(SRCS "pin_main.c"
                           "pin_display.c"
                           "pin_wifi.c" 
                           "pin_plugin.c"
                           "pin_config.c"
                           "pin_webserver.c"
                       INCLUDE_DIRS "."
                       REQUIRES esp_wifi
                                esp_http_server
                                esp_https_server
                                nvs_flash
                                json
                                spiffs
                                esp_timer
                                driver
                                spi_flash
                                mbedtls
                                fpc_a005
                                pin_plugin_api)
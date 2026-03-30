idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES espressif__coap json esp_wifi esp_event esp_netif nvs_flash esp_driver_gpio esp_timer)

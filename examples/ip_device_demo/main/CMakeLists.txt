idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES
        erikmeinders__idf-knx-device
        esp_wifi
        esp_netif
        esp_event
        nvs_flash
        esp_timer
)

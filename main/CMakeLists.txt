file(GLOB SOURCES "*.c" "ui/*.c")

idf_component_register(
    SRCS       ${SOURCES}
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        config_storage
        victron_ble
        dns_server
        esp_netif 
        lvgl
        esp_lcd
        spi_flash
        nvs_flash
        bt
        spiffs
        esp_netif          # <— for TCP/IP interfaces
        esp_event          # <— for Wi-Fi & netif events
        esp_wifi           # <— to start/configure the Soft-AP
        esp_http_server    # <— the HTTP server library
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    LV_LVGL_H_INCLUDE_SIMPLE
)

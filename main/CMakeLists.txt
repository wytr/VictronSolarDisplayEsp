file(GLOB SOURCES "*.c")

idf_component_register(
    SRCS       ${SOURCES}
    INCLUDE_DIRS "."
    PRIV_REQUIRES
        dns_server
        esp_netif 
        lvgl
        esp_lcd
        spi_flash
        nvs_flash
        bt
        mbedtls
        spiffs
        esp_netif          # <— for TCP/IP interfaces
        esp_event          # <— for Wi-Fi & netif events
        esp_wifi           # <— to start/configure the Soft-AP
        esp_http_server    # <— the HTTP server library
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    LV_CONF_PATH=${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h
)

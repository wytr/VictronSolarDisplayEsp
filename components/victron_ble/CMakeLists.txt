idf_component_register(
    SRCS "victron_ble.c" "victron_products.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_netif nvs_flash bt esp_hw_support mbedtls config_storage
)

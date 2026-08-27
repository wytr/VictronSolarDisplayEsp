idf_component_register(
    SRCS "config_storage.c"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES nvs_flash esp_system esp_hw_support
)

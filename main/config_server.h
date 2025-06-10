// config_server.h
#pragma once

#include "esp_err.h"

// Initialize & start Wi-Fi Soft-AP for configuration portal
esp_err_t wifi_ap_init(void);

// Mount SPIFFS and start HTTP config server
esp_err_t config_server_start(void);

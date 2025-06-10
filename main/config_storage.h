// config_storage.h
#pragma once
#include "esp_err.h"
esp_err_t load_aes_key(uint8_t key_out[16]);
esp_err_t save_aes_key(const uint8_t key_in[16]);

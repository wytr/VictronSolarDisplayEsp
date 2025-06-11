// config_storage.h
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h> // <-- Add this line

// brightness settings
esp_err_t load_brightness(uint8_t *brightness_out);
esp_err_t save_brightness(uint8_t brightness);

// AES key handling
esp_err_t load_aes_key(uint8_t key_out[16]);
esp_err_t save_aes_key(const uint8_t key_in[16]);

// Screensaver settings
esp_err_t load_screensaver_settings(bool *enabled, uint8_t *brightness, uint16_t *timeout);
esp_err_t save_screensaver_settings(bool enabled, uint8_t brightness, uint16_t timeout);

// Wi‑Fi AP settings handling (NVS namespace: "wifi")
// ssid_out and pass_out must have space for ssid_len and pass_len, respectively.
// On return, *ssid_len and *pass_len are set to the actual string lengths (including null).
esp_err_t load_wifi_config(char *ssid_out, size_t *ssid_len,
                           char *pass_out, size_t *pass_len,
                           uint8_t *enabled_out);

// Save AP settings; ssid and pass should be null‑terminated strings.
esp_err_t save_wifi_config(const char *ssid,
                           const char *pass,
                           uint8_t enabled_out);

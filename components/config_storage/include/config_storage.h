// config_storage.h
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "nvs.h"

#define DEFAULT_AP_PASSWORD "12345678"

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

// Relay tab configuration persistence
esp_err_t load_relay_config(bool *enabled_out,
                            uint8_t *count_out,
                            uint8_t *pins_out,
                            char (*labels_out)[20],
                            size_t max_pins);

esp_err_t save_relay_config(bool enabled,
                            const uint8_t *pins,
                            const char (*labels)[20],
                            uint8_t count);

// Victron BLE debug flag persistence (NVS namespace: "debug")
esp_err_t load_victron_debug(bool *enabled_out);
esp_err_t save_victron_debug(bool enabled);

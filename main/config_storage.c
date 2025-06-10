// config_storage.c
#include "config_storage.h"
#include "nvs.h"
#include <string.h>

#define AES_NAMESPACE  "victron"
#define AES_KEY        "aes_key"
#define WIFI_NAMESPACE "wifi"

esp_err_t load_aes_key(uint8_t key_out[16]) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(AES_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    size_t required = 16;
    err = nvs_get_blob(h, AES_KEY, key_out, &required);
    nvs_close(h);
    return err;
}

esp_err_t save_aes_key(const uint8_t key_in[16]) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(AES_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_blob(h, AES_KEY, key_in, 16);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_wifi_config(char *ssid_out, size_t *ssid_len,
                           char *pass_out, size_t *pass_len,
                           uint8_t *enabled_out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    // Read SSID
    err = nvs_get_str(h, "ssid", ssid_out, ssid_len);
    if (err != ESP_OK) {
        // default SSID
        const char *d = "VictronConfig";
        size_t dlen = strlen(d) + 1;
        if (*ssid_len >= dlen) memcpy(ssid_out, d, dlen);
        *ssid_len = dlen;
        nvs_set_str(h, "ssid", ssid_out);
    }

    // Read Password
    err = nvs_get_str(h, "password", pass_out, pass_len);
    if (err != ESP_OK) {
        // default empty
        if (*pass_len > 0) pass_out[0] = '\0';
        *pass_len = 1;
        nvs_set_str(h, "password", pass_out);
    }

    // Read Enabled flag
    err = nvs_get_u8(h, "enabled", enabled_out);
    if (err != ESP_OK) {
        *enabled_out = 1; // default enabled
        nvs_set_u8(h, "enabled", *enabled_out);
    }

    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_wifi_config(const char *ssid,
                           const char *pass,
                           uint8_t enabled_out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, "ssid", ssid);
    if (err == ESP_OK) err = nvs_set_str(h, "password", pass);
    if (err == ESP_OK) err = nvs_set_u8(h, "enabled", enabled_out);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

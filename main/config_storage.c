// config_storage.c
#include "config_storage.h"
#include "nvs_flash.h"
#include "nvs.h"

#define STORAGE_NAMESPACE "victron"
#define STORAGE_KEY       "aes_key"

esp_err_t load_aes_key(uint8_t key_out[16]) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // no user key yet
        nvs_close(h);
        return ESP_ERR_NVS_NOT_FOUND;
    } else if (err != ESP_OK) {
        return err;
    }
    size_t required = 16;
    err = nvs_get_blob(h, STORAGE_KEY, key_out, &required);
    nvs_close(h);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        // no key in storage
        return ESP_ERR_NVS_NOT_FOUND;
    }
    return err;
}

esp_err_t save_aes_key(const uint8_t key_in[16]) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(STORAGE_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }
    err = nvs_set_blob(h, STORAGE_KEY, key_in, 16);
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    return err;
}

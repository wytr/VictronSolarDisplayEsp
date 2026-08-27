// config_storage.c
#include "config_storage.h"
#include <string.h>

#define AES_NAMESPACE  "victron"
#define AES_KEY        "aes_key"
#define WIFI_NAMESPACE "wifi"
#define BRIGHTNESS_NAMESPACE "display"
#define BRIGHTNESS_KEY       "brightness"
#define SCREENSAVER_NAMESPACE "screensaver"
#define SS_ENABLED_KEY        "enabled"
#define SS_BRIGHT_KEY         "brightness"
#define SS_TIMEOUT_KEY        "timeout"
#define RELAY_NAMESPACE       "relay"
#define RELAY_ENABLED_KEY     "enabled"
#define RELAY_COUNT_KEY       "count"
#define RELAY_PINS_KEY        "pins"
#define RELAY_LABELS_KEY      "labels"
#define RELAY_MAX_PINS        8
#define RELAY_UNUSED_PIN      0xFF

#define DEBUG_NAMESPACE       "debug"
#define VICTRON_DEBUG_KEY     "victron_debug"

esp_err_t load_brightness(uint8_t *brightness_out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(BRIGHTNESS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(h, BRIGHTNESS_KEY, brightness_out);
    if (err != ESP_OK) {
        *brightness_out = 5; // default value
        nvs_set_u8(h, BRIGHTNESS_KEY, *brightness_out);
        nvs_commit(h);
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_brightness(uint8_t brightness) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(BRIGHTNESS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, BRIGHTNESS_KEY, brightness);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

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

esp_err_t load_screensaver_settings(bool *enabled, uint8_t *brightness, uint16_t *timeout) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(SCREENSAVER_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint8_t en = 1, bright = 1;
    uint16_t tout = 10;
    nvs_get_u8(h, SS_ENABLED_KEY, &en);
    nvs_get_u8(h, SS_BRIGHT_KEY, &bright);
    nvs_get_u16(h, SS_TIMEOUT_KEY, &tout);

    if (enabled) *enabled = en;
    if (brightness) *brightness = bright;
    if (timeout) *timeout = tout;

    // Save defaults if not present
    nvs_set_u8(h, SS_ENABLED_KEY, en);
    nvs_set_u8(h, SS_BRIGHT_KEY, bright);
    nvs_set_u16(h, SS_TIMEOUT_KEY, tout);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_screensaver_settings(bool enabled, uint8_t brightness, uint16_t timeout) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(SCREENSAVER_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_u8(h, SS_ENABLED_KEY, enabled ? 1 : 0);
    nvs_set_u8(h, SS_BRIGHT_KEY, brightness);
    nvs_set_u16(h, SS_TIMEOUT_KEY, timeout);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_relay_config(bool *enabled_out,
                            uint8_t *count_out,
                            uint8_t *pins_out,
                            char (*labels_out)[20],
                            size_t max_pins)
{
    if (enabled_out == NULL || count_out == NULL || pins_out == NULL || max_pins == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(RELAY_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    bool changed = false;

    uint8_t en = 1;
    esp_err_t tmp = nvs_get_u8(h, RELAY_ENABLED_KEY, &en);
    if (tmp != ESP_OK) {
        en = 1;
        nvs_set_u8(h, RELAY_ENABLED_KEY, en);
        changed = true;
    }

    uint8_t count = 0;
    tmp = nvs_get_u8(h, RELAY_COUNT_KEY, &count);
    if (tmp != ESP_OK) {
        count = 0;
        nvs_set_u8(h, RELAY_COUNT_KEY, count);
        changed = true;
    }

    if (count > RELAY_MAX_PINS) {
        count = RELAY_MAX_PINS;
        nvs_set_u8(h, RELAY_COUNT_KEY, count);
        changed = true;
    }

    uint8_t stored_pins[RELAY_MAX_PINS];
    memset(stored_pins, RELAY_UNUSED_PIN, sizeof(stored_pins));
    size_t blob_size = sizeof(stored_pins);
    tmp = nvs_get_blob(h, RELAY_PINS_KEY, stored_pins, &blob_size);
    if (tmp != ESP_OK || blob_size != sizeof(stored_pins)) {
        memset(stored_pins, RELAY_UNUSED_PIN, sizeof(stored_pins));
        nvs_set_blob(h, RELAY_PINS_KEY, stored_pins, sizeof(stored_pins));
        changed = true;
    }

    char stored_labels[RELAY_MAX_PINS][20];
    size_t labels_blob_size = sizeof(stored_labels);
    tmp = nvs_get_blob(h, RELAY_LABELS_KEY, stored_labels, &labels_blob_size);
    if (tmp != ESP_OK || labels_blob_size != sizeof(stored_labels)) {
        /* Initialize empty labels */
        for (size_t i = 0; i < RELAY_MAX_PINS; ++i) stored_labels[i][0] = '\0';
        nvs_set_blob(h, RELAY_LABELS_KEY, stored_labels, sizeof(stored_labels));
        changed = true;
    }

    if (changed) {
        nvs_commit(h);
    }

    nvs_close(h);

    if (count > max_pins) {
        count = (uint8_t)max_pins;
    }

    for (size_t i = 0; i < max_pins; ++i) {
        if (i < count) {
            pins_out[i] = stored_pins[i];
        } else {
            pins_out[i] = RELAY_UNUSED_PIN;
        }
        if (labels_out != NULL) {
            if (i < count) {
                strncpy(labels_out[i], stored_labels[i], 20);
                labels_out[i][19] = '\0';
            } else {
                labels_out[i][0] = '\0';
            }
        }
    }

    *enabled_out = (en != 0);
    *count_out = count;
    return ESP_OK;
}

esp_err_t save_relay_config(bool enabled,
                            const uint8_t *pins,
                            const char (*labels)[20],
                            uint8_t count)
{
    if (count > RELAY_MAX_PINS) {
        count = RELAY_MAX_PINS;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(RELAY_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t stored_pins[RELAY_MAX_PINS];
    for (size_t i = 0; i < RELAY_MAX_PINS; ++i) {
        stored_pins[i] = RELAY_UNUSED_PIN;
    }

    char stored_labels[RELAY_MAX_PINS][20];
    for (size_t i = 0; i < RELAY_MAX_PINS; ++i) stored_labels[i][0] = '\0';

    if (pins != NULL) {
        size_t copy_count = count;
        if (copy_count > RELAY_MAX_PINS) {
            copy_count = RELAY_MAX_PINS;
        }
        memcpy(stored_pins, pins, copy_count);
    }

    if (labels != NULL) {
        size_t copy_count = count;
        if (copy_count > RELAY_MAX_PINS) copy_count = RELAY_MAX_PINS;
        for (size_t i = 0; i < copy_count; ++i) {
            strncpy(stored_labels[i], labels[i], 20);
            stored_labels[i][19] = '\0';
        }
    }

    err = nvs_set_u8(h, RELAY_ENABLED_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_u8(h, RELAY_COUNT_KEY, count);
    if (err == ESP_OK) err = nvs_set_blob(h, RELAY_PINS_KEY, stored_pins, sizeof(stored_pins));
    if (err == ESP_OK) err = nvs_set_blob(h, RELAY_LABELS_KEY, stored_labels, sizeof(stored_labels));
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err;
}

esp_err_t load_victron_debug(bool *enabled_out)
{
    if (enabled_out == NULL) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(DEBUG_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint8_t v = 0;
    esp_err_t tmp = nvs_get_u8(h, VICTRON_DEBUG_KEY, &v);
    if (tmp != ESP_OK) {
        v = 0; // default: debug disabled
        nvs_set_u8(h, VICTRON_DEBUG_KEY, v);
        nvs_commit(h);
    }

    *enabled_out = (v != 0);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_victron_debug(bool enabled)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(DEBUG_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, VICTRON_DEBUG_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

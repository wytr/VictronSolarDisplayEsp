// victron_ble.c
#include "victron_ble.h"
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "aes/esp_aes.h"

static const char *TAG = "victron_ble";

// AES key (must match device)
static const uint8_t aes_key[16] = {
    0x4B, 0x71, 0x78, 0xE6,
    0x4C, 0x82, 0x8A, 0x26,
    0x2C, 0xDD, 0x51, 0x61,
    0xE3, 0x40, 0x4B, 0x7A
};

// Internal manufacturer-data layout
typedef struct __attribute__((packed)) {
    uint16_t vendorID;
    uint8_t  beaconType;
    uint8_t  unknownData1[3];
    uint8_t  victronRecordType;
    uint16_t nonceDataCounter;
    uint8_t  encryptKeyMatch;
    uint8_t  victronEncryptedData[21];
    uint8_t  nullPad;
} victronManufacturerData;

// Registered callback
static victron_data_cb_t data_cb = NULL;

void victron_ble_register_callback(victron_data_cb_t cb) {
    data_cb = cb;
}

// Forward declarations
static void ble_host_task(void *param);
static int ble_gap_event_handler(struct ble_gap_event *event, void *arg);
static void ble_app_on_sync(void);

void victron_ble_init(void) {
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Initialize NimBLE stack
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_app_on_sync;

    // Launch the NimBLE host task under FreeRTOS
    nimble_port_freertos_init(ble_host_task);
}

static void ble_host_task(void *param) {
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

static void ble_app_on_sync(void) {
    struct ble_gap_disc_params disc_params = {
        .itvl = 0x0060,
        .window = 0x0030,
        .passive = 1,
        .filter_policy = 0,
        .limited = 0,
    };
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, BLE_HS_FOREVER,
                          &disc_params, ble_gap_event_handler, NULL);
    if (rc) {
        ESP_LOGE(TAG, "Error starting discovery; rc=%d", rc);
    } else {
        ESP_LOGI(TAG, "Started Victron BLE scan");
    }
}

static int ble_gap_event_handler(struct ble_gap_event *event, void *arg) {
    if (event->type != BLE_GAP_EVENT_DISC) {
        return 0;
    }

    struct ble_hs_adv_fields fields;
    int rc = ble_hs_adv_parse_fields(&fields,
                                     event->disc.data,
                                     event->disc.length_data);
    if (rc != 0 ||
        fields.mfg_data_len < offsetof(victronManufacturerData, victronEncryptedData) + 1)
    {
        return 0;
    }

    victronManufacturerData *mdata = (void*)fields.mfg_data;
    if (mdata->vendorID != 0x02e1 ||
        mdata->victronRecordType != 0x01 ||
        mdata->encryptKeyMatch != aes_key[0])
    {
        return 0;
    }

    int encr_size = fields.mfg_data_len
                  - offsetof(victronManufacturerData, victronEncryptedData);
    uint8_t input[32]  = {0};
    uint8_t output[32] = {0};
    memcpy(input, mdata->victronEncryptedData, encr_size);

    esp_aes_context ctx;
    esp_aes_init(&ctx);
    if (esp_aes_setkey(&ctx, aes_key, 128) != 0) {
        ESP_LOGE(TAG, "AES setkey failed");
        esp_aes_free(&ctx);
        return 0;
    }
    uint16_t nonce = mdata->nonceDataCounter;
    uint8_t ctr_blk[16] = { nonce & 0xFF, (nonce >> 8) & 0xFF };
    uint8_t stream_block[16] = {0};
    size_t offset = 0;
    rc = esp_aes_crypt_ctr(&ctx, encr_size, &offset,
                           ctr_blk, stream_block,
                           input, output);
    esp_aes_free(&ctx);
    if (rc != 0) {
        ESP_LOGE(TAG, "AES CTR decrypt failed");
        return 0;
    }

    victronPanelData_t panel;
    memcpy(&panel, output, sizeof(panel));
    if ((panel.outputCurrentHi & 0xFE) != 0xFE) {
        return 0;
    }

    if (data_cb) {
        data_cb(&panel);
    }

    return 0;
}

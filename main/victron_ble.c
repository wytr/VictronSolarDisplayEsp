#include "victron_ble.h"
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_err.h>
#include "mbedtls/aes.h"

// NimBLE includes
#include "esp_nimble_hci.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/util/util.h"

static const char *TAG = "VIC_BLE";
static uint8_t own_addr_type;

// Victron AES key (AES-128)
static const uint8_t key[16] = {
    0x2b, 0xaa, 0xb6, 0x36, 0x0a, 0x89, 0x0b, 0x2e,
    0x1f, 0x11, 0xef, 0x77, 0x43, 0xb5, 0x8c, 0xcb
};

// Shared variables
float batVol = 0.0f;
float powIn  = 0.0f;
int readings = 0;
long nread    = 0;
int yeTod     = 0;
int deState   = -1;
char savedDeviceName[32] = "(unknown)";

// Manufacturer and panel data formats (packed)
typedef struct __attribute__((packed)) {
    uint16_t vendorID;
    uint8_t beaconType;
    uint8_t unknownData1[3];
    uint8_t victronRecordType;
    uint16_t nonceDataCounter;
    uint8_t encryptKeyMatch;
    uint8_t victronEncryptedData[21];
    uint8_t nullPad;
} victronManufacturerData;

typedef struct __attribute__((packed)) {
    uint8_t deviceState;
    uint8_t errorCode;
    int16_t batteryVoltage;
    int16_t batteryCurrent;
    uint16_t todayYield;
    uint16_t inputPower;
    uint8_t outputCurrentLo;
    uint8_t outputCurrentHi;
    uint8_t unused[4];
} victronPanelData;

static void ble_app_scan(void);

static int scan_callback(struct ble_gap_event *event, void *arg)
{
    if (event->type == BLE_GAP_EVENT_DISC) {
        struct ble_hs_adv_fields fields;
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) != 0) {
            return 0;
        }

        if (fields.mfg_data_len >= sizeof(victronManufacturerData)) {
            victronManufacturerData *vicData = (victronManufacturerData *)fields.mfg_data;
            if (vicData->vendorID != 0x02E1 || vicData->victronRecordType != 0x01) return 0;
            if (vicData->encryptKeyMatch != key[0]) return 0;

            // Decrypt AES-CTR
            uint8_t inputData[16] = {0};
            uint8_t outputData[16] = {0};
            int encrSize = fields.mfg_data_len - 10;
            if (encrSize > 16) encrSize = 16;
            memcpy(inputData, vicData->victronEncryptedData, encrSize);

            mbedtls_aes_context ctx;
            mbedtls_aes_init(&ctx);
            mbedtls_aes_setkey_enc(&ctx, key, 128);

            uint8_t nonce_counter[16] = {0};
            nonce_counter[0] = (uint8_t)(vicData->nonceDataCounter & 0xFF);
            nonce_counter[1] = (uint8_t)((vicData->nonceDataCounter >> 8) & 0xFF);
            uint8_t stream_block[16] = {0};
            size_t nc_off = 0;

            mbedtls_aes_crypt_ctr(&ctx, encrSize, &nc_off,
                                  nonce_counter, stream_block,
                                  inputData, outputData);

            mbedtls_aes_free(&ctx);

            // Parse panel data
            victronPanelData *pdata = (victronPanelData *)outputData;
            batVol = pdata->batteryVoltage * 0.01f;
            powIn  = pdata->inputPower;
            yeTod  = pdata->todayYield * 0.01f * 1000;
            deState = pdata->deviceState;
            readings = (readings + 1) & 0x0F;
            nread++;

            ESP_LOGI(TAG, "Victron data: V=%.2fV P=%dW Y=%dWh S=%d",
                     batVol, (int)powIn, yeTod, deState);
        }
    }
    return 0;
}

static void ble_on_sync(void)
{
    ble_hs_id_infer_auto(0, &own_addr_type);
    ble_app_scan();
}

static void ble_app_scan(void)
{
    struct ble_gap_disc_params scan_params = {
        .passive = 0,
        .itvl = 0x10,     // replaces interval
        .window = 0x10,   // replaces window
        .filter_policy = 0,
        .limited = 0,
    };

    int rc = ble_gap_disc(own_addr_type, BLE_HS_FOREVER, &scan_params, scan_callback, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "Error initiating GAP discovery: %d", rc);
    } else {
        ESP_LOGI(TAG, "Scanning started");
    }
}

static void ble_host_task(void *param)
{
    nimble_port_run();
    nimble_port_freertos_deinit();
}

void victron_ble_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_nimble_hci_init()); // compatible with your ESP-IDF
    nimble_port_init();
    ble_hs_cfg.sync_cb = ble_on_sync;
    nimble_port_freertos_init(ble_host_task);
}

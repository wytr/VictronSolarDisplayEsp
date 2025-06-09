// victron_ble.c
#include "victron_ble.h"
#include <string.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_bt.h>
#include <esp_gap_ble_api.h>
#include <esp_bt_main.h>
#include <esp_bt_device.h>
#include <esp_err.h>
#include "mbedtls/aes.h"

static const char *TAG = "VIC_BLE";
static const int scanTime = 5; // seconds

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

static void gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan params set, starting scan for %d seconds", scanTime);
        esp_ble_gap_start_scanning(scanTime);
        break;

    case ESP_GAP_BLE_SCAN_RESULT_EVT: {
        esp_ble_gap_cb_param_t *scan_result = param;
        if (scan_result->scan_rst.search_evt == ESP_GAP_SEARCH_INQ_RES_EVT) {
            uint8_t *man_data = NULL;
            uint8_t man_len = 0;
            man_data = esp_ble_resolve_adv_data(
                scan_result->scan_rst.ble_adv,
                ESP_BLE_AD_MANUFACTURER_SPECIFIC_TYPE,
                &man_len);
            if (man_data && man_len >= sizeof(victronManufacturerData)) {
                victronManufacturerData *vicData = (victronManufacturerData *)man_data;
                if (vicData->vendorID != 0x02E1 || vicData->victronRecordType != 0x01) {
                    return;
                }
                if (vicData->encryptKeyMatch != key[0]) {
                    return;
                }

                // Decrypt AES-CTR using mbedtls
                uint8_t inputData[16] = {0};
                uint8_t outputData[16] = {0};
                int encrSize = man_len - 10;
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
        break;
    }
    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        ESP_LOGI(TAG, "Scan stopped, restarting");
        esp_ble_gap_start_scanning(scanTime);
        break;

    default:
        break;
    }
}

void victron_ble_init(void)
{
    // NVS init
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // BLE controller init
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_bt_controller_init(&bt_cfg));
    ESP_ERROR_CHECK(esp_bt_controller_enable(ESP_BT_MODE_BLE));
    ESP_ERROR_CHECK(esp_bluedroid_init());
    ESP_ERROR_CHECK(esp_bluedroid_enable());

    // Register GAP callback
    ESP_ERROR_CHECK(esp_ble_gap_register_callback(gap_cb));

    // Configure scan parameters
    esp_ble_scan_params_t scan_params = {
        .scan_type              = BLE_SCAN_TYPE_ACTIVE,
        .own_addr_type          = BLE_ADDR_TYPE_PUBLIC,
        .scan_filter_policy     = BLE_SCAN_FILTER_ALLOW_ALL,
        .scan_interval          = 0x50,
        .scan_window            = 0x30,
        .scan_duplicate         = BLE_SCAN_DUPLICATE_DISABLE
    };
    ESP_ERROR_CHECK(esp_ble_gap_set_scan_params(&scan_params));
}

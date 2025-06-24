/* main.c */
#include <stdio.h>
#include <inttypes.h>
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "victron_ble.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "ui.h"
#include "config_server.h"
#include "esp_timer.h"

static const char *TAG = "VICTRON_LVGL_APP";
#define logSection(section) ESP_LOGI(TAG, "\n\n***** %s *****\n", section)
#define LVGL_PORT_ROTATION_DEGREE 90
#define REBOOT_INTERVAL_US (12ULL * 60 * 60 * 1000000) // 12 hours in microseconds
// --- 24h reboot timer callback ---
static void reboot_timer_cb(void* arg) {
    ESP_LOGI(TAG, "Rebooting after 24h uptime (timer)...");
    esp_restart();
}

void setup(void);

void app_main(void) {
    setup();
}

void setup(void) {
    /* --- LVGL init start --- */
    logSection("LVGL init start");

    /* --- Chip info --- */
    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG,
        "This is %s chip, %d cores, features: %s%s%s%s",
        CONFIG_IDF_TARGET, chip_info.cores,
        (chip_info.features & CHIP_FEATURE_WIFI_BGN)    ? "WiFi/"     : "",
        (chip_info.features & CHIP_FEATURE_BT)          ? "BT/"       : "",
        (chip_info.features & CHIP_FEATURE_BLE)         ? "BLE/"      : "",
        (chip_info.features & CHIP_FEATURE_IEEE802154)  ? "802.15.4"  : ""
    );

    /* --- Flash and heap info --- */
    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }
    ESP_LOGI(TAG,
        "%" PRIu32 "MB flash, min free heap: %" PRIu32 ", free PSRAM: %u",
        flash_size / (1024 * 1024),
        esp_get_minimum_free_heap_size(),
        heap_caps_get_free_size(MALLOC_CAP_SPIRAM)
    );

    /* --- Display init --- */
    logSection("Display init");
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size   = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate        = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate        = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate        = LV_DISP_ROT_270,
#else
        .rotate        = LV_DISP_ROT_NONE,
#endif
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_brightness_set(5);

    /* --- Lock LVGL port and initialize UI --- */
    lvgl_port_lock(0);
    ui_init();

    /* --- Start Wi-Fi AP & config server --- */
    wifi_ap_init();
    config_server_start();

    /* --- Register BLE callback and start BLE --- */
    victron_ble_register_callback(ui_on_panel_data);
    victron_ble_init();

    /* --- Setup 24h reboot timer --- */
    static esp_timer_handle_t reboot_timer;
    const esp_timer_create_args_t reboot_timer_args = {
        .callback = &reboot_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "24h_reboot"
    };
    esp_timer_create(&reboot_timer_args, &reboot_timer);
    esp_timer_start_periodic(reboot_timer, REBOOT_INTERVAL_US);

    /* --- Setup complete --- */
    logSection("Setup complete");
}

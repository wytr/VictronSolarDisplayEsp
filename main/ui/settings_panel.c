#include "settings_panel.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <lvgl.h>
#include "config_storage.h"
#include "config_server.h"
#include "display.h"
#include "ui/relay_panel.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#define WIFI_NAMESPACE "wifi"

static const char *TAG_SETTINGS = "UI_SETTINGS";
static const char *APP_VERSION = "1.3.0";

static void ta_event_cb(lv_event_t *e);
static void wifi_event_cb(lv_event_t *e);
static void ap_checkbox_event_cb(lv_event_t *e);
static void password_toggle_btn_event_cb(lv_event_t *e);
static void save_key_btn_event_cb(lv_event_t *e);
static void reboot_btn_event_cb(lv_event_t *e);
static void brightness_slider_event_cb(lv_event_t *e);
static void cb_screensaver_event_cb(lv_event_t *e);
static void victron_debug_event_cb(lv_event_t *e);
static void slider_ss_brightness_event_cb(lv_event_t *e);
static void spinbox_ss_time_event_cb(lv_event_t *e);
static void spinbox_ss_time_increment_event_cb(lv_event_t *e);
static void spinbox_ss_time_decrement_event_cb(lv_event_t *e);
static void screensaver_timer_cb(lv_timer_t *timer);
static void screensaver_enable(ui_state_t *ui, bool enable);
static void screensaver_wake(ui_state_t *ui);

static void relay_tab_checkbox_event_cb(lv_event_t *e);
static void apply_relay_tab_state(ui_state_t *ui, bool enabled, bool update_checkbox);
static void relay_config_add_btn_event_cb(lv_event_t *e);
static void relay_config_remove_btn_event_cb(lv_event_t *e);
static void relay_dropdown_event_cb(lv_event_t *e);
static void relay_config_refresh_dropdowns(ui_state_t *ui);
static void relay_config_update_controls(ui_state_t *ui);
static uint8_t relay_config_find_first_available(ui_state_t *ui);
static bool relay_config_pin_in_use(const ui_state_t *ui, uint8_t pin, size_t skip_index);
static void relay_config_create_row(ui_state_t *ui, size_t index);
static void relay_config_append_option(char *buf, size_t buf_len, const char *line);
static void relay_config_append_gpio_option(char *buf, size_t buf_len, uint8_t pin);
static uint8_t relay_config_parse_gpio_label(const char *label);
static void relay_config_persist(ui_state_t *ui);
static void relay_label_ta_event_cb(lv_event_t *e);

static const uint8_t RELAY_GPIO_CHOICES[] = {5, 6, 7, 15, 16, 46, 9, 14};
static const size_t RELAY_GPIO_COUNT = sizeof(RELAY_GPIO_CHOICES) / sizeof(RELAY_GPIO_CHOICES[0]);

static void create_wifi_settings_page(ui_state_t *ui, lv_obj_t *page_wifi,
                                      const char *default_ssid,
                                      const char *default_pass,
                                      uint8_t ap_enabled)
{
    /* Root layout container */
    lv_obj_t *wifi_container = lv_obj_create(page_wifi);
    lv_obj_remove_style_all(wifi_container);
    lv_obj_set_size(wifi_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(wifi_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(wifi_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(wifi_container, 10, 0);
    lv_obj_set_style_pad_gap(wifi_container, 12, 0);
    lv_obj_set_scroll_dir(wifi_container, LV_DIR_VER);

    /* SSID label */
    lv_obj_t *lbl_ssid = lv_label_create(wifi_container);
    lv_obj_add_style(lbl_ssid, &ui->styles.small, 0);
    lv_label_set_text(lbl_ssid, "SSID:");

    /* SSID row: input + checkbox */
    lv_obj_t *ssid_row = lv_obj_create(wifi_container);
    lv_obj_remove_style_all(ssid_row);
    lv_obj_set_width(ssid_row, lv_pct(100));
    lv_obj_set_height(ssid_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(ssid_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ssid_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(ssid_row, 10, 0);
    lv_obj_set_flex_align(ssid_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* SSID input */
    ui->wifi.ssid = lv_textarea_create(ssid_row);
    lv_textarea_set_one_line(ui->wifi.ssid, true);
    lv_obj_set_width(ui->wifi.ssid, lv_pct(40));
    lv_textarea_set_text(ui->wifi.ssid, default_ssid);
    lv_obj_add_event_cb(ui->wifi.ssid, ta_event_cb, LV_EVENT_FOCUSED, ui);
    lv_obj_add_event_cb(ui->wifi.ssid, ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ui->wifi.ssid, ta_event_cb, LV_EVENT_CANCEL, ui);
    lv_obj_add_event_cb(ui->wifi.ssid, ta_event_cb, LV_EVENT_READY, ui);
    lv_obj_add_event_cb(ui->wifi.ssid, wifi_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(ui->wifi.ssid, &ui->styles.small, 0);

    /* AP enable checkbox */
    ui->wifi.ap_enable = lv_checkbox_create(ssid_row);
    lv_checkbox_set_text(ui->wifi.ap_enable, "Enable AP");
    lv_obj_add_style(ui->wifi.ap_enable, &ui->styles.medium, 0);
    if (ap_enabled) {
        lv_obj_add_state(ui->wifi.ap_enable, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui->wifi.ap_enable, ap_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, ui);

    /* Password label */
    lv_obj_t *lbl_pass = lv_label_create(wifi_container);
    lv_obj_add_style(lbl_pass, &ui->styles.small, 0);
    lv_label_set_text(lbl_pass, "Password:");

    const char *ap_password = (default_pass && default_pass[0] != '\0') ? default_pass : DEFAULT_AP_PASSWORD;

    /* Row for password + toggle button */
    lv_obj_t *pass_row = lv_obj_create(wifi_container);
    lv_obj_remove_style_all(pass_row);
    lv_obj_set_width(pass_row, lv_pct(100));
    lv_obj_set_height(pass_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(pass_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(pass_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(pass_row, 10, 0);
    lv_obj_set_flex_align(pass_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Password textarea */
    ui->wifi.password = lv_textarea_create(pass_row);
    lv_textarea_set_password_mode(ui->wifi.password, true);
    lv_textarea_set_one_line(ui->wifi.password, true);
    lv_obj_set_width(ui->wifi.password, lv_pct(40));
    lv_textarea_set_text(ui->wifi.password, ap_password);
    lv_obj_add_event_cb(ui->wifi.password, ta_event_cb, LV_EVENT_FOCUSED, ui);
    lv_obj_add_event_cb(ui->wifi.password, ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ui->wifi.password, ta_event_cb, LV_EVENT_CANCEL, ui);
    lv_obj_add_event_cb(ui->wifi.password, ta_event_cb, LV_EVENT_READY, ui);
    lv_obj_add_event_cb(ui->wifi.password, wifi_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(ui->wifi.password, &ui->styles.small, 0);

    /* Show/Hide password toggle */
    ui->wifi.password_toggle = lv_btn_create(pass_row);
    lv_obj_set_width(ui->wifi.password_toggle, lv_pct(20));
    lv_obj_add_event_cb(ui->wifi.password_toggle, password_toggle_btn_event_cb, LV_EVENT_CLICKED, ui);

    lv_obj_t *lbl_toggle = lv_label_create(ui->wifi.password_toggle);
    lv_label_set_text(lbl_toggle, "Show");
    lv_obj_center(lbl_toggle);
    lv_obj_add_style(lbl_toggle, &ui->styles.small, 0);
}

static void create_display_settings_page(ui_state_t *ui, lv_obj_t *page_display)
{
    /* Root container for display settings */
    lv_obj_t *disp_container = lv_obj_create(page_display);
    lv_obj_remove_style_all(disp_container);
    lv_obj_set_size(disp_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(disp_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(disp_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(disp_container, 10, 0);
    lv_obj_set_style_pad_gap(disp_container, 14, 0);
    lv_obj_set_scroll_dir(disp_container, LV_DIR_VER);

    /* --- Brightness --- */
    lv_obj_t *lbl_brightness = lv_label_create(disp_container);
    lv_obj_add_style(lbl_brightness, &ui->styles.small, 0);
    lv_label_set_text(lbl_brightness, "Brightness:");

    lv_obj_t *slider_brightness = lv_slider_create(disp_container);
    lv_obj_set_width(slider_brightness, lv_pct(50));
    lv_slider_set_range(slider_brightness, 1, 100);
    lv_slider_set_value(slider_brightness, ui->brightness, LV_ANIM_OFF);
    bsp_display_brightness_set(ui->brightness);
    lv_obj_add_event_cb(slider_brightness, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(slider_brightness, &ui->styles.medium, 0);

    /* --- Screensaver enable --- */
    ui->screensaver.checkbox = lv_checkbox_create(disp_container);
    lv_checkbox_set_text(ui->screensaver.checkbox, "Enable Screensaver");
    if (ui->screensaver.enabled) {
        lv_obj_add_state(ui->screensaver.checkbox, LV_STATE_CHECKED);
    }
    lv_obj_add_event_cb(ui->screensaver.checkbox, cb_screensaver_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(ui->screensaver.checkbox, &ui->styles.medium, 0);

    /* --- Screensaver brightness --- */
    lv_obj_t *lbl_ss_brightness = lv_label_create(disp_container);
    lv_obj_add_style(lbl_ss_brightness, &ui->styles.small, 0);
    lv_label_set_text(lbl_ss_brightness, "Screensaver Brightness:");

    ui->screensaver.slider_brightness = lv_slider_create(disp_container);
    lv_obj_set_width(ui->screensaver.slider_brightness, lv_pct(50));
    lv_slider_set_range(ui->screensaver.slider_brightness, 1, 100);
    lv_slider_set_value(ui->screensaver.slider_brightness, ui->screensaver.brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(ui->screensaver.slider_brightness, slider_ss_brightness_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(ui->screensaver.slider_brightness, &ui->styles.medium, 0);

    /* --- Screensaver timeout section --- */
    lv_obj_t *lbl_ss_time = lv_label_create(disp_container);
    lv_obj_add_style(lbl_ss_time, &ui->styles.small, 0);
    lv_label_set_text(lbl_ss_time, "Screensaver Timeout (s):");

    /* Row container for spinbox + buttons */
    lv_obj_t *timeout_row = lv_obj_create(disp_container);
    lv_obj_remove_style_all(timeout_row);
    lv_obj_set_width(timeout_row, lv_pct(100));
    lv_obj_set_height(timeout_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(timeout_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(timeout_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(timeout_row, 10, 0);
    lv_obj_set_flex_align(timeout_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Decrement button */
    lv_obj_t *btn_dec = lv_btn_create(timeout_row);
    lv_obj_set_size(btn_dec, 40, 40);
    lv_obj_t *lbl_dec = lv_label_create(btn_dec);
    lv_label_set_text(lbl_dec, LV_SYMBOL_MINUS);
    lv_obj_center(lbl_dec);
    lv_obj_add_event_cb(btn_dec, spinbox_ss_time_decrement_event_cb, LV_EVENT_ALL, ui);

    /* Timeout spinbox */
    ui->screensaver.spinbox_timeout = lv_spinbox_create(timeout_row);
    lv_spinbox_set_range(ui->screensaver.spinbox_timeout, 5, 3600);
    lv_spinbox_set_value(ui->screensaver.spinbox_timeout, ui->screensaver.timeout);
    lv_spinbox_set_digit_format(ui->screensaver.spinbox_timeout, 4, 0);
    lv_obj_set_width(ui->screensaver.spinbox_timeout, 120);
    lv_obj_add_event_cb(ui->screensaver.spinbox_timeout, spinbox_ss_time_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(ui->screensaver.spinbox_timeout, &ui->styles.small, 0);

    /* Increment button */
    lv_obj_t *btn_inc = lv_btn_create(timeout_row);
    lv_obj_set_size(btn_inc, 40, 40);
    lv_obj_t *lbl_inc = lv_label_create(btn_inc);
    lv_label_set_text(lbl_inc, LV_SYMBOL_PLUS);
    lv_obj_center(lbl_inc);
    lv_obj_add_event_cb(btn_inc, spinbox_ss_time_increment_event_cb, LV_EVENT_ALL, ui);

    /* --- Timer setup --- */
    ui->screensaver.timer = lv_timer_create(screensaver_timer_cb,
                                            ui->screensaver.timeout * 1000,
                                            ui);
    if (ui->screensaver.enabled) {
        lv_timer_reset(ui->screensaver.timer);
        lv_timer_resume(ui->screensaver.timer);
    } else {
        lv_timer_pause(ui->screensaver.timer);
    }
}

static void create_relay_settings_page(ui_state_t *ui, lv_obj_t *page_relay)
{
    /* Root container for relay settings */
    lv_obj_t *relay_container = lv_obj_create(page_relay);
    lv_obj_remove_style_all(relay_container);
    lv_obj_set_size(relay_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(relay_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(relay_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(relay_container, 10, 0);
    lv_obj_set_style_pad_gap(relay_container, 14, 0);
    lv_obj_set_scroll_dir(relay_container, LV_DIR_VER);

    ui->relay_checkbox = lv_checkbox_create(relay_container);
    lv_checkbox_set_text(ui->relay_checkbox, "Enable Relay Tab");
    lv_obj_add_style(ui->relay_checkbox, &ui->styles.medium, 0);
    lv_obj_add_event_cb(ui->relay_checkbox, relay_tab_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, ui);

    /* --- Relay configuration section --- */
    lv_obj_t *relay_section = lv_obj_create(relay_container);
    lv_obj_remove_style_all(relay_section);
    lv_obj_set_width(relay_section, lv_pct(100));
    lv_obj_set_height(relay_section, LV_SIZE_CONTENT);
    lv_obj_set_layout(relay_section, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(relay_section, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(relay_section, 14, 0);
    ui->relay_config.container = relay_section;

    /* --- Row for Add / Remove buttons --- */
    lv_obj_t *controls_row = lv_obj_create(relay_section);
    lv_obj_remove_style_all(controls_row);
    lv_obj_set_width(controls_row, lv_pct(100));
    lv_obj_set_height(controls_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(controls_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(controls_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(controls_row, 10, 0);
    lv_obj_set_flex_align(controls_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Add button */
    ui->relay_config.add_btn = lv_btn_create(controls_row);
    lv_obj_set_size(ui->relay_config.add_btn, 48, 48);
    lv_obj_t *lbl_add = lv_label_create(ui->relay_config.add_btn);
    lv_label_set_text(lbl_add, LV_SYMBOL_PLUS);
    lv_obj_center(lbl_add);
    lv_obj_add_event_cb(ui->relay_config.add_btn, relay_config_add_btn_event_cb, LV_EVENT_CLICKED, ui);

    /* Remove button */
    ui->relay_config.remove_btn = lv_btn_create(controls_row);
    lv_obj_set_size(ui->relay_config.remove_btn, 48, 48);
    lv_obj_t *lbl_remove = lv_label_create(ui->relay_config.remove_btn);
    lv_label_set_text(lbl_remove, LV_SYMBOL_MINUS);
    lv_obj_center(lbl_remove);
    lv_obj_add_event_cb(ui->relay_config.remove_btn, relay_config_remove_btn_event_cb, LV_EVENT_CLICKED, ui);

    /* --- Relay list section --- */
    ui->relay_config.list = lv_obj_create(relay_section);
    lv_obj_remove_style_all(ui->relay_config.list);
    lv_obj_set_width(ui->relay_config.list, lv_pct(100));
    lv_obj_set_height(ui->relay_config.list, LV_SIZE_CONTENT);
    lv_obj_set_layout(ui->relay_config.list, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(ui->relay_config.list, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_gap(ui->relay_config.list, 10, 0);
    lv_obj_set_scroll_dir(ui->relay_config.list, LV_DIR_VER);

    /* --- Populate existing relay rows --- */
    for (size_t i = 0; i < ui->relay_config.count; ++i) {
        relay_config_create_row(ui, i);
    }

    /* --- Apply saved state --- */
    apply_relay_tab_state(ui, ui->relay_tab_enabled, true);
    relay_config_refresh_dropdowns(ui);
    relay_config_update_controls(ui);
}

static void create_system_settings_page(ui_state_t *ui, lv_obj_t *page_system)
{
    /* Root container */
    lv_obj_t *sys_container = lv_obj_create(page_system);
    lv_obj_remove_style_all(sys_container);
    lv_obj_set_size(sys_container, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_layout(sys_container, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(sys_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_all(sys_container, 10, 0);
    lv_obj_set_style_pad_gap(sys_container, 14, 0);
    lv_obj_set_scroll_dir(sys_container, LV_DIR_VER);

    /* --- Version Label --- */
    lv_obj_t *lbl_version = lv_label_create(sys_container);
    lv_obj_add_style(lbl_version, &ui->styles.small, 0);
    lv_label_set_text_fmt(lbl_version, "Version: %s", APP_VERSION);

    /* --- Device Info Section --- */
    ui->lbl_device_type = lv_label_create(sys_container);
    lv_obj_add_style(ui->lbl_device_type, &ui->styles.small, 0);
    lv_label_set_text(ui->lbl_device_type, "Device: --");

    ui->lbl_product_name = lv_label_create(sys_container);
    lv_obj_add_style(ui->lbl_product_name, &ui->styles.small, 0);
    lv_label_set_text(ui->lbl_product_name, "Product: --");

    ui->lbl_error = lv_label_create(sys_container);
    lv_obj_add_style(ui->lbl_error, &ui->styles.small, 0);
    lv_label_set_text(ui->lbl_error, "Err: 0");

    /* --- MAC Address --- */
    lv_obj_t *lbl_mac = lv_label_create(sys_container);
    lv_obj_add_style(lbl_mac, &ui->styles.small, 0);
    lv_label_set_text(lbl_mac, "MAC Address:");

    ui->ta_mac = lv_textarea_create(sys_container);
    lv_textarea_set_one_line(ui->ta_mac, true);
    lv_obj_set_width(ui->ta_mac, lv_pct(50));
    lv_textarea_set_text(ui->ta_mac, "00:00:00:00:00:00");
    lv_obj_add_event_cb(ui->ta_mac, ta_event_cb, LV_EVENT_FOCUSED, ui);
    lv_obj_add_event_cb(ui->ta_mac, ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ui->ta_mac, ta_event_cb, LV_EVENT_CANCEL, ui);
    lv_obj_add_event_cb(ui->ta_mac, ta_event_cb, LV_EVENT_READY, ui);

    /* --- AES Key --- */
    lv_obj_t *lbl_key = lv_label_create(sys_container);
    lv_obj_add_style(lbl_key, &ui->styles.small, 0);
    lv_label_set_text(lbl_key, "AES Key:");

    uint8_t aes_key_bin[16] = {0};
    char aes_key_hex[33] = {0};
    if (load_aes_key(aes_key_bin) == ESP_OK) {
        for (int i = 0; i < 16; ++i) {
            sprintf(aes_key_hex + i * 2, "%02X", aes_key_bin[i]);
        }
    } else {
        strcpy(aes_key_hex, "00000000000000000000000000000000");
    }

    ui->ta_key = lv_textarea_create(sys_container);
    lv_textarea_set_one_line(ui->ta_key, true);
    lv_obj_set_width(ui->ta_key, lv_pct(80));
    lv_textarea_set_text(ui->ta_key, aes_key_hex);
    lv_obj_add_event_cb(ui->ta_key, ta_event_cb, LV_EVENT_FOCUSED, ui);
    lv_obj_add_event_cb(ui->ta_key, ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ui->ta_key, ta_event_cb, LV_EVENT_CANCEL, ui);
    lv_obj_add_event_cb(ui->ta_key, ta_event_cb, LV_EVENT_READY, ui);

    /* --- Buttons Row (Save + Reboot) --- */
    lv_obj_t *btn_row = lv_obj_create(sys_container);
    lv_obj_remove_style_all(btn_row);
    lv_obj_set_width(btn_row, lv_pct(100));
    lv_obj_set_height(btn_row, LV_SIZE_CONTENT);
    lv_obj_set_layout(btn_row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(btn_row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(btn_row, 10, 0);
    lv_obj_set_flex_align(btn_row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    /* Save Key Button */
    lv_obj_t *btn_save_key = lv_btn_create(btn_row);
    lv_obj_set_size(btn_save_key, 100, 45);
    lv_obj_t *lbl_save = lv_label_create(btn_save_key);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_center(lbl_save);
    lv_obj_add_event_cb(btn_save_key, save_key_btn_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_style(btn_save_key, &ui->styles.small, 0);

    /* Reboot Button */
    lv_obj_t *btn_reboot = lv_btn_create(btn_row);
    lv_obj_set_size(btn_reboot, 100, 45);
    lv_obj_t *lbl_reboot = lv_label_create(btn_reboot);
    lv_label_set_text(lbl_reboot, "Reboot");
    lv_obj_center(lbl_reboot);
    lv_obj_add_event_cb(btn_reboot, reboot_btn_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_style(btn_reboot, &ui->styles.small, 0);

    /* --- Victron Debug Checkbox --- */
    ui->victron_debug_checkbox = lv_checkbox_create(sys_container);
    lv_checkbox_set_text(ui->victron_debug_checkbox, "Enable Victron BLE Debug");
    lv_obj_add_style(ui->victron_debug_checkbox, &ui->styles.medium, 0);
    lv_obj_add_event_cb(ui->victron_debug_checkbox, victron_debug_event_cb, LV_EVENT_VALUE_CHANGED, ui);

    bool dbg = false;
    if (load_victron_debug(&dbg) == ESP_OK) {
        ui->victron_debug_enabled = dbg;
        if (dbg) {
            lv_obj_add_state(ui->victron_debug_checkbox, LV_STATE_CHECKED);
        }
    } else {
        ui->victron_debug_enabled = false;
    }

    /* Apply setting immediately to BLE module */
    victron_ble_set_debug(ui->victron_debug_enabled);
}

void ui_settings_panel_init(ui_state_t *ui,
                            const char *default_ssid,
                            const char *default_pass,
                            uint8_t ap_enabled)
{
    if (ui == NULL || ui->tab_settings == NULL) {
        return;
    }

    lv_obj_t *menu = lv_menu_create(ui->tab_settings);
    lv_obj_set_size(menu, lv_pct(100), lv_pct(100));
    lv_obj_center(menu);
    ui->settings_menu = menu;

    lv_obj_t *main_header = lv_menu_get_main_header(menu);
    lv_obj_add_style(main_header, &ui->styles.small, 0);

    lv_obj_t *back_btn = lv_menu_get_main_header_back_btn(menu);
    lv_obj_add_style(back_btn, &ui->styles.small, 0);

    lv_obj_t *back_label = lv_label_create(back_btn);
    lv_label_set_text(back_label, "Back");
    lv_obj_add_style(back_label, &ui->styles.small, 0);

    lv_obj_t *main_page = lv_menu_page_create(menu, NULL);
    lv_obj_t *page_wifi = lv_menu_page_create(menu, "Wi-Fi");

    lv_obj_t *page_display = lv_menu_page_create(menu, "Display");
    lv_obj_t *page_relay = lv_menu_page_create(menu, "Relay");
    lv_obj_t *page_system = lv_menu_page_create(menu, "System");

    lv_obj_t *cont;
    lv_obj_t *label;

    // Wi-Fi
    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Wi-Fi");
    lv_obj_add_style(cont, &ui->styles.small, 0);
    lv_obj_add_style(label, &ui->styles.small, 0);
    lv_menu_set_load_page_event(menu, cont, page_wifi);

    // Display
    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Display");
    lv_obj_add_style(cont, &ui->styles.small, 0);
    lv_obj_add_style(label, &ui->styles.small, 0);
    lv_menu_set_load_page_event(menu, cont, page_display);

    // Relay Configuration
    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "Relay Configuration");
    lv_obj_add_style(cont, &ui->styles.small, 0);
    lv_obj_add_style(label, &ui->styles.small, 0);
    lv_menu_set_load_page_event(menu, cont, page_relay);

    // System & Victron Key
    cont = lv_menu_cont_create(main_page);
    label = lv_label_create(cont);
    lv_label_set_text(label, "System & Victron Key");
    lv_obj_add_style(cont, &ui->styles.small, 0);
    lv_obj_add_style(label, &ui->styles.small, 0);
    lv_menu_set_load_page_event(menu, cont, page_system);


    lv_menu_set_page(menu, main_page);
    create_wifi_settings_page(ui, page_wifi, default_ssid, default_pass, ap_enabled);
    create_display_settings_page(ui, page_display);
    create_relay_settings_page(ui, page_relay);
    create_system_settings_page(ui, page_system);

    lv_obj_t *tab = ui->tab_settings;

    // remove default padding and layout effects
    lv_obj_set_style_pad_all(tab, 0, 0);
    lv_obj_set_style_pad_row(tab, 0, 0);
    lv_obj_set_style_pad_column(tab, 0, 0);
    lv_obj_set_style_border_width(tab, 0, 0);

    // also ensure the menu expands fully
    lv_obj_set_size(menu, lv_pct(100), lv_pct(100));
    lv_obj_align(menu, LV_ALIGN_CENTER, 0, 0);
    lv_obj_clear_flag(menu, LV_OBJ_FLAG_SCROLLABLE);

}

void ui_settings_panel_on_user_activity(ui_state_t *ui)
{
    screensaver_wake(ui);
}

void ui_settings_panel_set_mac(ui_state_t *ui, const char *mac_str)
{
    if (ui == NULL || ui->ta_mac == NULL || mac_str == NULL) {
        return;
    }
    lv_textarea_set_text(ui->ta_mac, mac_str);
}

static void ta_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    lv_obj_t *ta = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (ui == NULL || ui->keyboard == NULL) {
        return;
    }

    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(ui->keyboard, ta);
        lv_obj_move_foreground(ui->keyboard);
        lv_obj_clear_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_disp_t *disp = lv_disp_get_default();
        lv_coord_t screen_h = disp ? lv_disp_get_ver_res(disp) : LV_VER_RES;
        lv_coord_t kb_height = lv_obj_get_height(ui->keyboard);
        lv_coord_t available_h = screen_h - kb_height;
        if (available_h < screen_h / 3) {
            available_h = screen_h / 3;
        }
        lv_obj_update_layout(ui->tabview);
        lv_obj_set_height(ui->tabview, available_h);
        lv_obj_update_layout(ui->tabview);
        lv_obj_scroll_to_view_recursive(ta, LV_ANIM_OFF);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        if (ta == NULL) {
            return;
        }
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        lv_keyboard_set_textarea(ui->keyboard, NULL);
        lv_obj_add_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_disp_t *disp = lv_disp_get_default();
        lv_coord_t screen_h = disp ? lv_disp_get_ver_res(disp) : LV_VER_RES;
        lv_obj_set_height(ui->tabview, screen_h);
        lv_obj_update_layout(ui->tabview);
        lv_indev_reset(NULL, ta);
    }
}

static void wifi_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    lv_obj_t *ta = lv_event_get_target(e);
    const char *txt = lv_textarea_get_text(ta);
    if (ui == NULL) {
        return;
    }
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        if (ta == ui->wifi.ssid) {
            nvs_set_str(h, "ssid", txt);
        } else if (ta == ui->wifi.password) {
            nvs_set_str(h, "password", txt);
        }
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG_SETTINGS, "Wi-Fi config saved");
    } else {
        ESP_LOGE(TAG_SETTINGS, "nvs_open failed: %s", esp_err_to_name(err));
    }
}

static void password_toggle_btn_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->wifi.password == NULL) {
        return;
    }

    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    bool new_mode = !lv_textarea_get_password_mode(ui->wifi.password);
    lv_textarea_set_password_mode(ui->wifi.password, new_mode);

    lv_obj_t *btn = lv_event_get_target(e);
    lv_obj_t *label = lv_obj_get_child(btn, 0);
    if (label != NULL) {
        lv_label_set_text(label, new_mode ? "Show" : "Hide");
    }
}

static void ap_checkbox_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    lv_obj_t *checkbox = lv_event_get_target(e);
    if (ui == NULL || checkbox == NULL) {
        return;
    }
    bool en = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_set_u8(h, "enabled", en);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG_SETTINGS, "AP %s", en ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG_SETTINGS, "nvs_open failed: %s", esp_err_to_name(err));
    }

    if (en) {
        wifi_ap_init();
    } else {
        esp_err_t stop_err = esp_wifi_stop();
        if (stop_err == ESP_OK) {
            ESP_LOGI(TAG_SETTINGS, "Soft-AP stopped");
        } else {
            ESP_LOGE(TAG_SETTINGS, "Failed to stop AP: %s", esp_err_to_name(stop_err));
        }
    }
}

static void relay_tab_checkbox_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    lv_obj_t *checkbox = lv_event_get_target(e);
    if (checkbox == NULL) {
        return;
    }

    bool enabled = lv_obj_has_state(checkbox, LV_STATE_CHECKED);
    apply_relay_tab_state(ui, enabled, false);
}

static void apply_relay_tab_state(ui_state_t *ui, bool enabled, bool update_checkbox)
{
    if (ui == NULL) {
        return;
    }

    bool previous_state = ui->relay_tab_enabled;

    if (!update_checkbox && previous_state == enabled) {
        return;
    }

    if (ui->tab_relay_index == UINT16_MAX && ui->tab_relay != NULL) {
        ui->tab_relay_index = lv_obj_get_index(ui->tab_relay);
    }

    if (ui->tab_settings_index == UINT16_MAX && ui->tab_settings != NULL) {
        ui->tab_settings_index = lv_obj_get_index(ui->tab_settings);
    }

    ui->relay_tab_enabled = enabled;

    if (ui->tab_relay != NULL) {
        if (enabled) {
            lv_obj_clear_flag(ui->tab_relay, LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(ui->tab_relay, LV_OBJ_FLAG_HIDDEN);
        }
    }

    if (!enabled && ui->tabview != NULL && ui->tab_relay_index != UINT16_MAX) {
        uint16_t active = lv_tabview_get_tab_act(ui->tabview);
        if (active == ui->tab_relay_index) {
            uint16_t fallback = ui->tab_settings_index;
            if (fallback == ui->tab_relay_index) {
                fallback = 0;
            }
            lv_tabview_set_act(ui->tabview, fallback, LV_ANIM_OFF);
        }
    }

    lv_obj_t *btnm = NULL;
    if (ui->tabview != NULL) {
        btnm = lv_tabview_get_tab_btns(ui->tabview);
    }

    if (btnm != NULL && ui->tab_relay_index != UINT16_MAX) {
        if (enabled) {
            lv_btnmatrix_clear_btn_ctrl(btnm, ui->tab_relay_index, LV_BTNMATRIX_CTRL_DISABLED);
            lv_btnmatrix_clear_btn_ctrl(btnm, ui->tab_relay_index, LV_BTNMATRIX_CTRL_HIDDEN);
        } else {
            lv_btnmatrix_set_btn_ctrl(btnm, ui->tab_relay_index, LV_BTNMATRIX_CTRL_DISABLED);
            lv_btnmatrix_set_btn_ctrl(btnm, ui->tab_relay_index, LV_BTNMATRIX_CTRL_HIDDEN);
        }
    }

    if (update_checkbox && ui->relay_checkbox != NULL) {
        if (enabled) {
            lv_obj_add_state(ui->relay_checkbox, LV_STATE_CHECKED);
        } else {
            lv_obj_clear_state(ui->relay_checkbox, LV_STATE_CHECKED);
        }
    }

    relay_config_update_controls(ui);
    ui_relay_panel_refresh(ui);

    if (previous_state != ui->relay_tab_enabled) {
        relay_config_persist(ui);
    }
}

static void save_key_btn_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->ta_key == NULL) {
        return;
    }
    const char *hex = lv_textarea_get_text(ui->ta_key);
    if (strlen(hex) != 32) {
        ESP_LOGE(TAG_SETTINGS, "AES key must be 32 hex chars");
        return;
    }
    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
        char tmp[3] = { hex[i * 2], hex[i * 2 + 1], 0 };
        key[i] = (uint8_t)strtol(tmp, NULL, 16);
    }
    if (save_aes_key(key) == ESP_OK) {
        ESP_LOGI(TAG_SETTINGS, "AES key saved via UI");
    } else {
        ESP_LOGE(TAG_SETTINGS, "Failed to save AES key");
    }
}

static void reboot_btn_event_cb(lv_event_t *e)
{
    ESP_LOGI(TAG_SETTINGS, "Reboot requested via UI");
    esp_restart();
}

static void brightness_slider_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL) {
        return;
    }
    int val = lv_slider_get_value(lv_event_get_target(e));
    ui->brightness = (uint8_t)val;
    bsp_display_brightness_set(val);
    save_brightness(ui->brightness);
    ESP_LOGI(TAG_SETTINGS, "Brightness set to %d", val);
}

static void cb_screensaver_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->screensaver.checkbox == NULL) {
        return;
    }
    ui->screensaver.enabled = lv_obj_has_state(ui->screensaver.checkbox, LV_STATE_CHECKED);
    save_screensaver_settings(ui->screensaver.enabled,
                              ui->screensaver.brightness,
                              ui->screensaver.timeout);
    screensaver_enable(ui, ui->screensaver.enabled);
}

static void victron_debug_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->victron_debug_checkbox == NULL) return;
    bool enabled = lv_obj_has_state(ui->victron_debug_checkbox, LV_STATE_CHECKED);
    ui->victron_debug_enabled = enabled;
    if (save_victron_debug(enabled) == ESP_OK) {
        ESP_LOGI(TAG_SETTINGS, "Victron BLE debug %s", enabled ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG_SETTINGS, "Failed to persist Victron BLE debug setting");
    }
    // Apply immediately to BLE module
    victron_ble_set_debug(enabled);
}

static void slider_ss_brightness_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->screensaver.slider_brightness == NULL) {
        return;
    }
    ui->screensaver.brightness = lv_slider_get_value(ui->screensaver.slider_brightness);
    save_screensaver_settings(ui->screensaver.enabled,
                              ui->screensaver.brightness,
                              ui->screensaver.timeout);
    if (ui->screensaver.active) {
        bsp_display_brightness_set(ui->screensaver.brightness);
    }
}

static void spinbox_ss_time_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->screensaver.spinbox_timeout == NULL) {
        return;
    }
    ui->screensaver.timeout = lv_spinbox_get_value(ui->screensaver.spinbox_timeout);
    save_screensaver_settings(ui->screensaver.enabled,
                              ui->screensaver.brightness,
                              ui->screensaver.timeout);
    if (ui->screensaver.timer) {
        lv_timer_set_period(ui->screensaver.timer, ui->screensaver.timeout * 1000U);
    }
}

static void spinbox_ss_time_increment_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->screensaver.spinbox_timeout == NULL) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        lv_spinbox_increment(ui->screensaver.spinbox_timeout);
        ui->screensaver.timeout = lv_spinbox_get_value(ui->screensaver.spinbox_timeout);
        save_screensaver_settings(ui->screensaver.enabled,
                                  ui->screensaver.brightness,
                                  ui->screensaver.timeout);
        if (ui->screensaver.timer) {
            lv_timer_set_period(ui->screensaver.timer, ui->screensaver.timeout * 1000U);
        }
    }
}

static void spinbox_ss_time_decrement_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->screensaver.spinbox_timeout == NULL) {
        return;
    }
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        lv_spinbox_decrement(ui->screensaver.spinbox_timeout);
        ui->screensaver.timeout = lv_spinbox_get_value(ui->screensaver.spinbox_timeout);
        save_screensaver_settings(ui->screensaver.enabled,
                                  ui->screensaver.brightness,
                                  ui->screensaver.timeout);
        if (ui->screensaver.timer) {
            lv_timer_set_period(ui->screensaver.timer, ui->screensaver.timeout * 1000U);
        }
    }
}

static void screensaver_enable(ui_state_t *ui, bool enable)
{
    if (ui == NULL || ui->screensaver.timer == NULL) {
        return;
    }
    if (enable) {
        ui->screensaver.active = false;
        bsp_display_brightness_set(ui->brightness);
        lv_timer_set_period(ui->screensaver.timer, ui->screensaver.timeout * 1000U);
        lv_timer_reset(ui->screensaver.timer);
        lv_timer_resume(ui->screensaver.timer);
    } else {
        lv_timer_pause(ui->screensaver.timer);
        if (ui->screensaver.active) {
            bsp_display_brightness_set(ui->brightness);
            ui->screensaver.active = false;
        }
    }
}

static void screensaver_timer_cb(lv_timer_t *timer)
{
    ui_state_t *ui = timer ? (ui_state_t *)timer->user_data : NULL;
    if (ui == NULL) {
        return;
    }
    if (ui->screensaver.enabled && !ui->screensaver.active) {
        bsp_display_brightness_set(ui->screensaver.brightness);
        ui->screensaver.active = true;
    }
}

static void screensaver_wake(ui_state_t *ui)
{
    if (ui == NULL || ui->screensaver.timer == NULL) {
        return;
    }
    if (ui->screensaver.enabled) {
        lv_timer_reset(ui->screensaver.timer);
        if (ui->screensaver.active) {
            bsp_display_brightness_set(ui->brightness);
            ui->screensaver.active = false;
        }
    }
}

static void relay_config_add_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL) {
        return;
    }

    if (ui->relay_config.count >= UI_MAX_RELAY_BUTTONS) {
        return;
    }

    uint8_t pin = relay_config_find_first_available(ui);
    if (pin == UI_RELAY_GPIO_UNASSIGNED) {
        ESP_LOGW(TAG_SETTINGS, "No available GPIOs for relay buttons");
        return;
    }

    size_t index = ui->relay_config.count;
    ui->relay_config.count++;
    ui->relay_config.gpio_pins[index] = pin;
    ui->relay_button_state[index] = false;

    relay_config_create_row(ui, index);
    relay_config_refresh_dropdowns(ui);
    relay_config_update_controls(ui);
    ui_relay_panel_refresh(ui);
    relay_config_persist(ui);
}

static void relay_config_remove_btn_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }

    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL) {
        return;
    }

    if (ui->relay_config.count == 0) {
        return;
    }

    size_t index = ui->relay_config.count - 1;
    if (ui->relay_config.rows[index] != NULL) {
        lv_obj_del(ui->relay_config.rows[index]);
    }

    ui->relay_config.rows[index] = NULL;
    ui->relay_config.labels[index] = NULL;
    ui->relay_config.dropdowns[index] = NULL;
    /* row deletion already removes child objects (including textarea) if present; just clear pointer */
    ui->relay_config.textareas[index] = NULL;
    ui->relay_config.gpio_pins[index] = UI_RELAY_GPIO_UNASSIGNED;
    ui->relay_button_state[index] = false;
    ui->relay_config.count--;

    relay_config_refresh_dropdowns(ui);
    relay_config_update_controls(ui);
    ui_relay_panel_refresh(ui);
    relay_config_persist(ui);
}

static void relay_dropdown_event_cb(lv_event_t *e)
{
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL || ui->relay_config.dropdown_updating) {
        return;
    }

    lv_obj_t *dropdown = lv_event_get_target(e);
    size_t index = UI_MAX_RELAY_BUTTONS;
    for (size_t i = 0; i < ui->relay_config.count; ++i) {
        if (ui->relay_config.dropdowns[i] == dropdown) {
            index = i;
            break;
        }
    }

    if (index >= ui->relay_config.count) {
        return;
    }

    char selected[16];
    lv_dropdown_get_selected_str(dropdown, selected, sizeof(selected));
    uint8_t pin = relay_config_parse_gpio_label(selected);
    if (pin == UI_RELAY_GPIO_UNASSIGNED) {
        return;
    }

    ui->relay_button_state[index] = false;
    ui->relay_config.gpio_pins[index] = pin;
    relay_config_refresh_dropdowns(ui);
    relay_config_update_controls(ui);
    ui_relay_panel_refresh(ui);
    relay_config_persist(ui);
}

static void relay_config_refresh_dropdowns(ui_state_t *ui)
{
    if (!ui || ui->relay_config.dropdown_updating) return;
    ui->relay_config.dropdown_updating = true;

    for (size_t i = 0; i < ui->relay_config.count; ++i) {
        lv_obj_t *dropdown = ui->relay_config.dropdowns[i];
        if (!dropdown) continue;

        char options[256] = "";
        uint8_t current_pin = ui->relay_config.gpio_pins[i];

        if (current_pin != UI_RELAY_GPIO_UNASSIGNED) {
            relay_config_append_gpio_option(options, sizeof(options), current_pin);
        }
        for (size_t j = 0; j < RELAY_GPIO_COUNT; ++j) {
            uint8_t candidate = RELAY_GPIO_CHOICES[j];
            if (candidate == current_pin || relay_config_pin_in_use(ui, candidate, i)) continue;
            relay_config_append_gpio_option(options, sizeof(options), candidate);
        }

        const char *old_opts = lv_dropdown_get_options(dropdown);
        if (!old_opts || strcmp(old_opts, options) != 0) {
            lv_dropdown_set_options(dropdown, options);
        }
    }

    ui->relay_config.dropdown_updating = false;
}


static void relay_config_update_controls(ui_state_t *ui)
{
    if (ui == NULL) {
        return;
    }

    bool can_add = false;
    if (ui->relay_config.count < UI_MAX_RELAY_BUTTONS) {
        can_add = relay_config_find_first_available(ui) != UI_RELAY_GPIO_UNASSIGNED;
    }

    if (ui->relay_config.add_btn != NULL) {
        if (can_add) {
            lv_obj_clear_state(ui->relay_config.add_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(ui->relay_config.add_btn, LV_STATE_DISABLED);
        }
    }

    bool can_remove = ui->relay_config.count > 0;
    if (ui->relay_config.remove_btn != NULL) {
        if (can_remove) {
            lv_obj_clear_state(ui->relay_config.remove_btn, LV_STATE_DISABLED);
        } else {
            lv_obj_add_state(ui->relay_config.remove_btn, LV_STATE_DISABLED);
        }
    }
}

static uint8_t relay_config_find_first_available(ui_state_t *ui)
{
    for (size_t i = 0; i < RELAY_GPIO_COUNT; ++i) {
        uint8_t pin = RELAY_GPIO_CHOICES[i];
        if (!relay_config_pin_in_use(ui, pin, SIZE_MAX)) {
            return pin;
        }
    }
    return UI_RELAY_GPIO_UNASSIGNED;
}

static bool relay_config_pin_in_use(const ui_state_t *ui, uint8_t pin, size_t skip_index)
{
    if (ui == NULL || pin == UI_RELAY_GPIO_UNASSIGNED) {
        return false;
    }

    for (size_t i = 0; i < ui->relay_config.count; ++i) {
        if (i == skip_index) {
            continue;
        }
        if (ui->relay_config.gpio_pins[i] == pin) {
            return true;
        }
    }
    return false;
}

static void relay_config_create_row(ui_state_t *ui, size_t index)
{
    if (ui == NULL || ui->relay_config.list == NULL || index >= UI_MAX_RELAY_BUTTONS) {
        return;
    }

    lv_obj_t *row = lv_obj_create(ui->relay_config.list);
    lv_obj_remove_style_all(row);
    lv_obj_set_width(row, lv_pct(100));
    lv_obj_set_height(row, LV_SIZE_CONTENT);
    lv_obj_set_layout(row, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_gap(row, 12, 0);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t *dropdown = lv_dropdown_create(row);
    lv_obj_set_width(dropdown, 150);
    lv_obj_add_event_cb(dropdown, relay_dropdown_event_cb, LV_EVENT_VALUE_CHANGED, ui);
    lv_obj_add_style(dropdown, &ui->styles.small, 0);

    /* Add a textarea for an optional custom label */
    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_max_length(ta, 20);
    lv_obj_set_width(ta, 160);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "Label (optional)");
    lv_obj_add_style(ta, &ui->styles.small, 0);
    /* If user previously set a custom label, show it; otherwise leave empty */
    if (index < UI_MAX_RELAY_BUTTONS && ui->relay_button_text[index][0] != '\0') {
        lv_textarea_set_text(ta, ui->relay_button_text[index]);
    }
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_FOCUSED, ui);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ta, ta_event_cb, LV_EVENT_READY, ui);
    /* Save label on defocus/ready */
    lv_obj_add_event_cb(ta, relay_label_ta_event_cb, LV_EVENT_DEFOCUSED, ui);
    lv_obj_add_event_cb(ta, relay_label_ta_event_cb, LV_EVENT_READY, ui);

    ui->relay_config.rows[index] = row;
    ui->relay_config.dropdowns[index] = dropdown;
    ui->relay_config.textareas[index] = ta;
}

static void relay_config_append_option(char *buf, size_t buf_len, const char *line)
{
    if (buf == NULL || buf_len == 0 || line == NULL) {
        return;
    }

    size_t len = strnlen(buf, buf_len);
    if (len >= buf_len - 1) {
        return;
    }

    if (len > 0) {
        buf[len++] = '\n';
        if (len >= buf_len - 1) {
            buf[buf_len - 1] = '\0';
            return;
        }
    }

    snprintf(buf + len, buf_len - len, "%s", line);
}

static void relay_config_append_gpio_option(char *buf, size_t buf_len, uint8_t pin)
{
    char label[16];
    snprintf(label, sizeof(label), "GPIO %u", (unsigned)pin);
    relay_config_append_option(buf, buf_len, label);
}

static uint8_t relay_config_parse_gpio_label(const char *label)
{
    if (label == NULL) {
        return UI_RELAY_GPIO_UNASSIGNED;
    }

    unsigned value = 0;
    if (sscanf(label, "GPIO %u", &value) == 1) {
        if (value <= UINT8_MAX) {
            return (uint8_t)value;
        }
    }
    return UI_RELAY_GPIO_UNASSIGNED;
}

static void relay_config_persist(ui_state_t *ui)
{
    if (ui == NULL) {
        return;
    }

    uint8_t pins[UI_MAX_RELAY_BUTTONS];
    uint8_t count = ui->relay_config.count;
    if (count > UI_MAX_RELAY_BUTTONS) {
        count = UI_MAX_RELAY_BUTTONS;
    }

    for (size_t i = 0; i < UI_MAX_RELAY_BUTTONS; ++i) {
        if (i < count) {
            pins[i] = ui->relay_config.gpio_pins[i];
        } else {
            pins[i] = UI_RELAY_GPIO_UNASSIGNED;
        }
    }

    /* Prepare labels to persist: fixed 20-byte strings per slot */
    char labels[UI_MAX_RELAY_BUTTONS][20];
    for (size_t i = 0; i < UI_MAX_RELAY_BUTTONS; ++i) {
        if (i < count && ui->relay_button_text[i][0] != '\0') {
            strncpy(labels[i], ui->relay_button_text[i], sizeof(labels[i]));
            labels[i][sizeof(labels[i]) - 1] = '\0';
        } else {
            labels[i][0] = '\0';
        }
    }

    esp_err_t err = save_relay_config(ui->relay_tab_enabled, pins, labels, count);
    if (err != ESP_OK) {
        ESP_LOGW(TAG_SETTINGS, "Failed to save relay config: %s", esp_err_to_name(err));
    }
}

static void relay_label_ta_event_cb(lv_event_t *e)
{
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL) return;

    lv_event_code_t code = lv_event_get_code(e);
    if (!(code == LV_EVENT_DEFOCUSED || code == LV_EVENT_READY)) return;

    lv_obj_t *ta = lv_event_get_target(e);
    if (ta == NULL) return;

    for (size_t j = 0; j < ui->relay_config.count; ++j) {
        if (ui->relay_config.textareas[j] == ta) {
            const char *txt = lv_textarea_get_text(ta);
            if (txt && txt[0] != '\0') {
                strncpy(ui->relay_button_text[j], txt, sizeof(ui->relay_button_text[j]));
                ui->relay_button_text[j][sizeof(ui->relay_button_text[j]) - 1] = '\0';
            } else {
                ui->relay_button_text[j][0] = '\0';
            }
            relay_config_persist(ui);
            ui_relay_panel_refresh(ui);
            break;
        }
    }
}

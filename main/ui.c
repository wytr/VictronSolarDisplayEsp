/* ui.c */
#include "ui.h"
#include <stdlib.h>
#include <inttypes.h>
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "esp_log.h"
#include "victron_ble.h"
#include "nvs_flash.h"
#include "config_storage.h"
#include "config_server.h"
#include "esp_wifi.h"
#include <stdio.h>

// NVS namespace for Wi-Fi
#define WIFI_NAMESPACE "wifi"

// Font Awesome symbols (declared in main.c)
LV_FONT_DECLARE(font_awesome_solar_panel_40);
LV_FONT_DECLARE(font_awesome_bolt_40);

static const char *TAG_UI = "UI_MODULE";

// LVGL objects & styles
static lv_obj_t *tabview, *tab_live, *tab_info, *kb;
static lv_style_t style_title, style_val, style_big;
static lv_obj_t *lbl_battV, *lbl_battA, *lbl_loadA;
static lv_obj_t *lbl_solar, *lbl_yield, *lbl_state, *lbl_error;
static lv_obj_t *solar_symbol, *bolt_symbol;
static lv_obj_t *ta_mac, *ta_key, *lbl_load_watt;

// Global brightness variable
uint8_t brightness = 100;

// Wi-Fi AP config UI elements
static lv_obj_t *ta_ssid, *ta_password, *cb_ap_enable;

// --- Screensaver settings state ---
static lv_obj_t *cb_screensaver, *slider_ss_brightness, *spinbox_ss_time;
static bool screensaver_enabled = false;
static uint8_t screensaver_brightness = 1;
static uint16_t screensaver_timeout = 10;
static lv_timer_t *screensaver_timer = NULL;
static bool screensaver_active = false;

// Forward declarations
static const char *err_str(uint8_t e);
static const char *charger_state_str(uint8_t s);
static void ta_event_cb(lv_event_t *e);
static void brightness_slider_event_cb(lv_event_t *e);
static void wifi_event_cb(lv_event_t *e);
static void ap_checkbox_event_cb(lv_event_t *e);
static void save_key_btn_event_cb(lv_event_t *e);
static void reboot_btn_event_cb(lv_event_t *e);
static void screensaver_timer_cb(lv_timer_t *timer);
static void screensaver_enable(bool enable);
static void screensaver_wake(void);

// Forward declarations for screensaver UI event callbacks
static void cb_screensaver_event_cb(lv_event_t *e);
static void slider_ss_brightness_event_cb(lv_event_t *e);
static void spinbox_ss_time_event_cb(lv_event_t *e);
static void spinbox_ss_time_increment_event_cb(lv_event_t *e);
static void spinbox_ss_time_decrement_event_cb(lv_event_t *e);
// Forward declarations (already present, just for clarity)
static void tabview_touch_event_cb(lv_event_t *e);

void ui_init(void) {
    // Initialize NVS
    nvs_flash_init();
    load_brightness(&brightness); // use the global variable
    // Load defaults from storage
    char default_ssid[33]; size_t ssid_len = sizeof(default_ssid);
    char default_pass[65]; size_t pass_len = sizeof(default_pass);
    uint8_t ap_enabled;
    if (load_wifi_config(default_ssid, &ssid_len, default_pass, &pass_len, &ap_enabled) != ESP_OK) {
        strncpy(default_ssid, "VictronConfig", sizeof(default_ssid));
        default_ssid[sizeof(default_ssid)-1] = '\0';
        default_pass[0] = '\0';
        ap_enabled = 1;
    }

    // Initialize theme
#if LV_USE_THEME_DEFAULT
    lv_theme_default_init(NULL,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        LV_THEME_DEFAULT_DARK,
        &lv_font_montserrat_14
    );
#endif

    // Create tabs
    tabview  = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 40);
    tab_live = lv_tabview_add_tab(tabview, "Live");
    tab_info = lv_tabview_add_tab(tabview, "Info");

    // Add wake event callbacks to tabs
    lv_obj_add_event_cb(tab_live, tabview_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(tab_live, tabview_touch_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tab_live, tabview_touch_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_add_event_cb(tab_info, tabview_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(tab_info, tabview_touch_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tab_info, tabview_touch_event_cb, LV_EVENT_GESTURE, NULL);

    // Keyboard for textareas
    kb = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(kb, LV_HOR_RES, LV_VER_RES/2);
    lv_obj_align(kb, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);

    // Styles
    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_title, lv_color_white());

    lv_style_init(&style_big);
    lv_style_set_text_font(&style_big, &lv_font_montserrat_40);
    lv_style_set_text_color(&style_big, lv_color_white());

    lv_style_init(&style_val);
#if LV_FONT_MONTSERRAT_30
    lv_style_set_text_font(&style_val, &lv_font_montserrat_30);
#else
    lv_style_set_text_font(&style_val, LV_FONT_DEFAULT);
#endif
    lv_style_set_text_color(&style_val, lv_color_white());

    // Live tab layout
    lv_obj_t *row = lv_obj_create(tab_live);
    lv_obj_set_size(row, lv_pct(100), 100);
    lv_obj_set_flex_flow(row, LV_STYLE_PAD_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY,
                           LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    #define NEW_BOX(name, txt, ptr) do { \
        lv_obj_t *b = lv_obj_create(row); \
        lv_obj_set_size(b, lv_pct(30), 80); \
        lv_obj_set_style_pad_all(b, 8, 0); \
        lv_obj_set_style_bg_opa(b, LV_OPA_TRANSP, 0); \
        lv_obj_t *h = lv_label_create(b); \
        lv_label_set_text(h, name); \
        lv_obj_add_style(h, &style_title, 0); \
        lv_obj_align(h, LV_ALIGN_TOP_MID, 0, 0); \
        *(ptr) = lv_label_create(b); \
        lv_label_set_text(*(ptr), txt); \
        lv_obj_add_style(*(ptr), &style_val, 0); \
        lv_obj_align(*(ptr), LV_ALIGN_CENTER, 0, 10); \
    } while(0)

    NEW_BOX("Batt V", "0.00 V", &lbl_battV);
    NEW_BOX("Batt A", "0.0 A",  &lbl_battA);
    NEW_BOX("Load A", "0.0 A", &lbl_loadA);

    lbl_state = lv_label_create(tab_live);
    lv_obj_add_style(lbl_state, &style_big, 0);
    lv_label_set_text(lbl_state, "State");
    lv_obj_align(lbl_state, LV_ALIGN_CENTER, 0, 50);

    // Icons and labels
    solar_symbol = lv_label_create(tab_live);
    lv_obj_set_style_text_font(solar_symbol, &font_awesome_solar_panel_40, 0);
    lv_label_set_text(solar_symbol, "\xEF\x96\xBA");
    lv_obj_align(solar_symbol, LV_ALIGN_BOTTOM_LEFT, 25, -55);

    bolt_symbol = lv_label_create(tab_live);
    lv_obj_set_style_text_font(bolt_symbol, &font_awesome_bolt_40, 0);
    lv_label_set_text(bolt_symbol, "\xEF\x83\xA7");
    lv_obj_align(bolt_symbol, LV_ALIGN_BOTTOM_RIGHT, -28, -55);

    lbl_solar = lv_label_create(tab_live);
    lv_obj_add_style(lbl_solar, &style_title, 0);
    lv_label_set_text(lbl_solar, "");
    lv_obj_align(lbl_solar, LV_ALIGN_BOTTOM_LEFT, 32, -8);

    lbl_yield = lv_label_create(tab_live);
    lv_obj_add_style(lbl_yield, &style_title, 0);
    lv_label_set_text(lbl_yield, "");
    lv_obj_align(lbl_yield, LV_ALIGN_BOTTOM_MID, 0, -8);

    lbl_load_watt = lv_label_create(tab_live);
    lv_obj_add_style(lbl_load_watt, &style_title, 0);
    lv_label_set_text(lbl_load_watt, "");
    lv_obj_align(lbl_load_watt, LV_ALIGN_BOTTOM_RIGHT, -31, -8);

    // Wi-Fi SSID
    lv_obj_t *lbl_ssid = lv_label_create(tab_info);
    lv_obj_add_style(lbl_ssid, &style_title, 0);
    lv_label_set_text(lbl_ssid, "AP SSID:");
    lv_obj_align(lbl_ssid, LV_ALIGN_TOP_LEFT, 8, 20);

    ta_ssid = lv_textarea_create(tab_info);
    lv_textarea_set_one_line(ta_ssid, true);
    lv_obj_set_width(ta_ssid, lv_pct(80));
    lv_textarea_set_text(ta_ssid, default_ssid);
    lv_obj_align(ta_ssid, LV_ALIGN_TOP_LEFT, 8, 50);
    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_ssid, ta_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ta_ssid, wifi_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Wi-Fi Password
    lv_obj_t *lbl_pass = lv_label_create(tab_info);
    lv_obj_add_style(lbl_pass, &style_title, 0);
    lv_label_set_text(lbl_pass, "AP Password:");
    lv_obj_align(lbl_pass, LV_ALIGN_TOP_LEFT, 8, 90);

    ta_password = lv_textarea_create(tab_info);
    lv_textarea_set_password_mode(ta_password, true);
    lv_textarea_set_one_line(ta_password, true);
    lv_obj_set_width(ta_password, lv_pct(80));
    lv_textarea_set_text(ta_password, default_pass);
    lv_obj_align(ta_password, LV_ALIGN_TOP_LEFT, 8, 120);
    lv_obj_add_event_cb(ta_password, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_password, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_password, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_password, ta_event_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(ta_password, wifi_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Enable AP checkbox
    cb_ap_enable = lv_checkbox_create(tab_info);
    lv_checkbox_set_text(cb_ap_enable, "Enable AP");
    if (ap_enabled) lv_obj_add_state(cb_ap_enable, LV_STATE_CHECKED);
    lv_obj_align(cb_ap_enable, LV_ALIGN_TOP_LEFT, 8, 170);

    // Info tab: error, MAC, AES Key
    lbl_error = lv_label_create(tab_info);
    lv_obj_add_style(lbl_error, &style_title, 0);
    lv_label_set_text(lbl_error, "Err: 0");
    lv_obj_align(lbl_error, LV_ALIGN_TOP_LEFT, 8, 210);

    lv_obj_t *lmac = lv_label_create(tab_info);
    lv_obj_add_style(lmac, &style_title, 0);
    lv_label_set_text(lmac, "MAC Address:");
    lv_obj_align(lmac, LV_ALIGN_TOP_LEFT, 8, 250);

    ta_mac = lv_textarea_create(tab_info);
    lv_textarea_set_one_line(ta_mac, true);
    lv_obj_set_width(ta_mac, lv_pct(80));
    lv_textarea_set_text(ta_mac, "00:00:00:00:00:00");
    lv_obj_align(ta_mac, LV_ALIGN_TOP_LEFT, 8, 280);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_READY, NULL);

    lv_obj_t *lkey = lv_label_create(tab_info);
    lv_obj_add_style(lkey, &style_title, 0);
    lv_label_set_text(lkey, "AES Key:");
    lv_obj_align(lkey, LV_ALIGN_TOP_LEFT, 8, 320);

    // --- Set AES key text area from NVS ---
    uint8_t aes_key_bin[16] = {0};
    char aes_key_hex[33] = {0};
    if (load_aes_key(aes_key_bin) == ESP_OK) {
        for (int i = 0; i < 16; ++i) {
            sprintf(aes_key_hex + i * 2, "%02X", aes_key_bin[i]);
        }
    } else {
        strcpy(aes_key_hex, "00000000000000000000000000000000");
    }

    ta_key = lv_textarea_create(tab_info);
    lv_textarea_set_one_line(ta_key, true);
    lv_obj_set_width(ta_key, lv_pct(80));
    lv_textarea_set_text(ta_key, aes_key_hex);
    lv_obj_align(ta_key, LV_ALIGN_TOP_LEFT, 8, 350);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_READY, NULL);

    // --- Add Save button for AES Key ---
    lv_obj_t *btn_save_key = lv_btn_create(tab_info);
    lv_obj_align(btn_save_key, LV_ALIGN_TOP_LEFT, 8, 400); // moved down by 50px
    lv_obj_set_width(btn_save_key, lv_pct(18));
    lv_obj_t *lbl_btn = lv_label_create(btn_save_key);
    lv_label_set_text(lbl_btn, "Save");
    lv_obj_center(lbl_btn);
    lv_obj_add_event_cb(btn_save_key, save_key_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // --- Add Reboot button next to Save ---
    lv_obj_t *btn_reboot = lv_btn_create(tab_info);
    lv_obj_align(btn_reboot, LV_ALIGN_TOP_LEFT, 8 + lv_pct(20), 400); // right of Save
    lv_obj_set_width(btn_reboot, lv_pct(18));
    lv_obj_t *lbl_reboot = lv_label_create(btn_reboot);
    lv_label_set_text(lbl_reboot, "Reboot");
    lv_obj_center(lbl_reboot);
    lv_obj_add_event_cb(btn_reboot, reboot_btn_event_cb, LV_EVENT_CLICKED, NULL);

    // Brightness slider label
    lv_obj_t *lbl_brightness = lv_label_create(tab_info);
    lv_obj_add_style(lbl_brightness, &style_title, 0);
    lv_label_set_text(lbl_brightness, "Brightness:");
    lv_obj_align(lbl_brightness, LV_ALIGN_TOP_LEFT, 8, 450); // moved down by 50px

    // Brightness slider
    lv_obj_t *slider = lv_slider_create(tab_info);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 8, 500); // moved down by 50px
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, brightness, LV_ANIM_OFF); // set loaded value
    bsp_display_brightness_set(brightness); // set display brightness
    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Add extra space at the bottom
    lv_obj_t *spacer = lv_obj_create(tab_info);
    lv_obj_set_size(spacer, 10, 40);
    lv_obj_align(spacer, LV_ALIGN_TOP_LEFT, 8, 550); // moved down by 50px
    lv_obj_add_flag(spacer, LV_OBJ_FLAG_HIDDEN | LV_OBJ_FLAG_EVENT_BUBBLE);

    lv_obj_add_event_cb(cb_ap_enable, ap_checkbox_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Screensaver Enable Checkbox
    cb_screensaver = lv_checkbox_create(tab_info);
    lv_checkbox_set_text(cb_screensaver, "Enable Screensaver");
    lv_obj_align(cb_screensaver, LV_ALIGN_TOP_LEFT, 8, 600);
    lv_obj_add_event_cb(cb_screensaver, cb_screensaver_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Screensaver Brightness Slider
    lv_obj_t *lbl_ss_brightness = lv_label_create(tab_info);
    lv_obj_add_style(lbl_ss_brightness, &style_title, 0);
    lv_label_set_text(lbl_ss_brightness, "Screensaver Brightness:");
    lv_obj_align(lbl_ss_brightness, LV_ALIGN_TOP_LEFT, 8, 650);

    slider_ss_brightness = lv_slider_create(tab_info);
    lv_obj_set_width(slider_ss_brightness, lv_pct(80));
    lv_obj_align(slider_ss_brightness, LV_ALIGN_TOP_LEFT, 8, 680);
    lv_slider_set_range(slider_ss_brightness, 1, 100);
    lv_slider_set_value(slider_ss_brightness, screensaver_brightness, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider_ss_brightness, slider_ss_brightness_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Screensaver Timeout Spinbox
    lv_obj_t *lbl_ss_time = lv_label_create(tab_info);
    lv_obj_add_style(lbl_ss_time, &style_title, 0);
    lv_label_set_text(lbl_ss_time, "Screensaver Timeout (s):");
    lv_obj_align(lbl_ss_time, LV_ALIGN_TOP_LEFT, 8, 730);

    spinbox_ss_time = lv_spinbox_create(tab_info);
    lv_spinbox_set_range(spinbox_ss_time, 5, 3600);
    lv_spinbox_set_value(spinbox_ss_time, screensaver_timeout);
    lv_spinbox_set_digit_format(spinbox_ss_time, 4, 0);
    lv_obj_set_width(spinbox_ss_time, 100);
    lv_obj_align(spinbox_ss_time, LV_ALIGN_TOP_LEFT, 8 + 40, 760);
    lv_obj_add_event_cb(spinbox_ss_time, spinbox_ss_time_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    // Add increment and decrement buttons for the spinbox
    lv_coord_t h = lv_obj_get_height(spinbox_ss_time);

    // Decrement button
    lv_obj_t *btn_dec = lv_btn_create(tab_info);
    lv_obj_set_size(btn_dec, h, h);
    lv_obj_align_to(btn_dec, spinbox_ss_time, LV_ALIGN_OUT_LEFT_MID, -5, 0);
    lv_obj_set_style_bg_img_src(btn_dec, LV_SYMBOL_MINUS, 0);
    lv_obj_add_event_cb(btn_dec, spinbox_ss_time_decrement_event_cb, LV_EVENT_ALL, NULL);

    // Increment button
    lv_obj_t *btn_inc = lv_btn_create(tab_info);
    lv_obj_set_size(btn_inc, h, h);
    lv_obj_align_to(btn_inc, spinbox_ss_time, LV_ALIGN_OUT_RIGHT_MID, 5, 0);
    lv_obj_set_style_bg_img_src(btn_inc, LV_SYMBOL_PLUS, 0);
    lv_obj_add_event_cb(btn_inc, spinbox_ss_time_increment_event_cb, LV_EVENT_ALL, NULL);

    // Screensaver timer setup
    screensaver_timer = lv_timer_create(screensaver_timer_cb, screensaver_timeout * 1000, NULL);
    lv_timer_pause(screensaver_timer); // Start paused

    // Touch event: wake/reset screensaver timer
    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_GESTURE, NULL);

    lv_obj_add_event_cb(tabview, tabview_touch_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(tabview, tabview_touch_event_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(tabview, tabview_touch_event_cb, LV_EVENT_GESTURE, NULL);

    lvgl_port_unlock();
}

void ui_on_panel_data(const victronPanelData_t *d) {
    int battVraw = d->batteryVoltage;
    int battV_i  = battVraw / 100;
    int battV_f  = battVraw % 100;

    int battAraw = d->batteryCurrent;
    int battA_i  = battAraw / 10;
    int battA_f  = abs(battAraw % 10);

    int loadRaw = ((d->outputCurrentHi & 1) << 8) | d->outputCurrentLo;
    int load_i = loadRaw / 10;
    int load_f = loadRaw % 10;

    uint32_t solarW   = d->inputPower;
    uint32_t yieldWh  = (uint32_t)(d->todayYield * 0.01f * 1000.0f);
    uint32_t loadWatt = (loadRaw * battVraw) / 1000;

    lv_label_set_text_fmt(lbl_battV, "%d.%02d V", battV_i, battV_f);
    lv_label_set_text_fmt(lbl_battA, "%d.%1d A", battA_i, battA_f);
    lv_label_set_text_fmt(lbl_loadA, "%d.%1d A", load_i, load_f);
    lv_label_set_text_fmt(lbl_solar, "%lu W", solarW);
    lv_label_set_text_fmt(lbl_yield, "Yield: %lu Wh", yieldWh);
    lv_label_set_text_fmt(lbl_state, "%s", charger_state_str(d->deviceState));
    lv_label_set_text_fmt(lbl_error, "%s", err_str(d->errorCode));
    lv_label_set_text_fmt(lbl_load_watt, "%lu W", loadWatt);
}

static const char *err_str(uint8_t e) {
    switch (e) {
        case 0:   return "OK";
        case 1:   return "Battery temp too high";
        case 2:   return "Battery voltage too high";
        case 3:
        case 4:   return "Remote temp-sensor failure";
        case 5:   return "Remote temp-sensor lost";
        case 6:
        case 7:   return "Remote voltage-sense failure";
        case 8:   return "Remote voltage-sense lost";
        case 11:  return "Battery high ripple voltage";
        case 14:  return "Battery too cold for LiFePO4";
        case 17:  return "Controller overheating";
        case 18:  return "Controller over-current";
        case 20:  return "Max bulk time exceeded";
        case 21:  return "Current-sensor out of range";
        case 22:
        case 23:  return "Internal temp-sensor failure";
        case 24:  return "Fan failure";
        case 26:  return "Power terminal overheated";
        case 27:  return "Battery-side short circuit";
        case 28:  return "Power-stage hardware issue";
        case 29:  return "Over-charge protection triggered";
        case 33:  return "PV over-voltage";
        case 34:  return "PV over-current";
        case 35:  return "PV over-power";
        case 38:
        case 39:  return "PV input shorted to protect battery";
        case 40:  return "PV input failed to short";
        case 41:  return "Inverter-mode PV isolation";
        case 42:
        case 43:  return "PV side ground-fault";
        default:  return "Unknown error";
    }
}

const char *charger_state_str(uint8_t s) {
    switch (s) {
        case 0: return "Off";
        case 1: return "Low Power";
        case 2: return "Fault";
        case 3: return "Bulk";
        case 4: return "Absorption";
        case 5: return "Float";
        case 6: return "Storage";
        case 7: return "Equalize (Man)";
        case 8: return "Equalize (Auto)";
        case 9: return "Inverting";
        case 10: return "Power Supply";
        case 11: return "Starting";
        default: return "Unknown";
    }
}

static void ta_event_cb(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_FOCUSED) {
        lv_keyboard_set_textarea(kb, ta);
        lv_obj_move_foreground(kb);
        lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
    } else if (code == LV_EVENT_DEFOCUSED || code == LV_EVENT_CANCEL || code == LV_EVENT_READY) {
        lv_keyboard_set_textarea(kb, NULL);
        lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
    }
}

static void brightness_slider_event_cb(lv_event_t *e) {
    int val = lv_slider_get_value(lv_event_get_target(e));
    brightness = (uint8_t)val; // <-- update global variable
    bsp_display_brightness_set(val);
    save_brightness(brightness); // Persist to NVS
    ESP_LOGI(TAG_UI, "Brightness set to %d", val);
}

static void wifi_event_cb(lv_event_t *e) {
    lv_obj_t *ta = lv_event_get_target(e);
    const char *txt = lv_textarea_get_text(ta);
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        if (ta == ta_ssid)
            nvs_set_str(h, "ssid", txt);
        else if (ta == ta_password)
            nvs_set_str(h, "password", txt);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG_UI, "Wi-Fi config saved");
    } else {
        ESP_LOGE(TAG_UI, "nvs_open failed: %s", esp_err_to_name(err));
    }
}


static void ap_checkbox_event_cb(lv_event_t *e) {
    bool en = lv_obj_has_state(cb_ap_enable, LV_STATE_CHECKED);
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err == ESP_OK) {
        nvs_set_u8(h, "enabled", en);
        nvs_commit(h);
        nvs_close(h);
        ESP_LOGI(TAG_UI, "AP %s", en ? "enabled" : "disabled");
    } else {
        ESP_LOGE(TAG_UI, "nvs_open failed: %s", esp_err_to_name(err));
    }

    if (en) {
        wifi_ap_init();
    } else {
        esp_err_t stop_err = esp_wifi_stop();
        if (stop_err == ESP_OK) {
            ESP_LOGI(TAG_UI, "Soft-AP stopped");
        } else {
            ESP_LOGE(TAG_UI, "Failed to stop AP: %s", esp_err_to_name(stop_err));
        }
        // (optionally)
        // esp_wifi_deinit();
    }
}

static void save_key_btn_event_cb(lv_event_t *e) {
    const char *hex = lv_textarea_get_text(ta_key);
    if (strlen(hex) != 32) {
        ESP_LOGE(TAG_UI, "AES key must be 32 hex chars");
        // Optionally show a message box or error label
        return;
    }
    uint8_t key[16];
    for (int i = 0; i < 16; i++) {
        char tmp[3] = { hex[i*2], hex[i*2+1], 0 };
        key[i] = (uint8_t)strtol(tmp, NULL, 16);
    }
    if (save_aes_key(key) == ESP_OK) {
        ESP_LOGI(TAG_UI, "AES key saved via UI");
        // Optionally show a success message
    } else {
        ESP_LOGE(TAG_UI, "Failed to save AES key");
        // Optionally show a failure message
    }
}

static void reboot_btn_event_cb(lv_event_t *e) {
    ESP_LOGI(TAG_UI, "Reboot requested via UI");
    esp_restart();
}

void ui_set_ble_mac(const uint8_t *mac) {
    // Format MAC as "XX:XX:XX:XX:XX:XX"
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    lv_textarea_set_text(ta_mac, mac_str);
}

// --- Screensaver logic ---
static void screensaver_enable(bool enable) {
    if (enable) {
        screensaver_active = false; // Ensure not active on enable
        bsp_display_brightness_set(brightness); // Restore normal brightness
        lv_timer_set_period(screensaver_timer, screensaver_timeout * 1000);
        lv_timer_reset(screensaver_timer);      // Reset timer so timeout starts now
        lv_timer_resume(screensaver_timer);
    } else {
        lv_timer_pause(screensaver_timer);
        if (screensaver_active) {
            bsp_display_brightness_set(brightness); // Restore normal brightness
            screensaver_active = false;
        }
    }
}

static void screensaver_timer_cb(lv_timer_t *timer) {
    if (screensaver_enabled && !screensaver_active) {
        bsp_display_brightness_set(screensaver_brightness);
        screensaver_active = true;
    }
}

static void screensaver_wake(void) {
    if (screensaver_enabled) {
        lv_timer_reset(screensaver_timer);
        if (screensaver_active) {
            bsp_display_brightness_set(brightness); // uses up-to-date value
            screensaver_active = false;
        }
    }
}

// Screensaver UI event callbacks
static void cb_screensaver_event_cb(lv_event_t *e) {
    screensaver_enabled = lv_obj_has_state(cb_screensaver, LV_STATE_CHECKED);
    screensaver_enable(screensaver_enabled);
}

static void slider_ss_brightness_event_cb(lv_event_t *e) {
    screensaver_brightness = lv_slider_get_value(slider_ss_brightness);
}

static void spinbox_ss_time_event_cb(lv_event_t *e) {
    screensaver_timeout = lv_spinbox_get_value(spinbox_ss_time);
    if (screensaver_timer) lv_timer_set_period(screensaver_timer, screensaver_timeout * 1000);
}

// Add this callback implementation at file scope:
static void tabview_touch_event_cb(lv_event_t *e) {
    screensaver_wake();
}

// Add these callbacks at file scope:
static void spinbox_ss_time_increment_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        lv_spinbox_increment(spinbox_ss_time);
        screensaver_timeout = lv_spinbox_get_value(spinbox_ss_time);
        if (screensaver_timer) lv_timer_set_period(screensaver_timer, screensaver_timeout * 1000);
    }
}

static void spinbox_ss_time_decrement_event_cb(lv_event_t *e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_SHORT_CLICKED || code == LV_EVENT_LONG_PRESSED_REPEAT) {
        lv_spinbox_decrement(spinbox_ss_time);
        screensaver_timeout = lv_spinbox_get_value(spinbox_ss_time);
        if (screensaver_timer) lv_timer_set_period(screensaver_timer, screensaver_timeout * 1000);
    }
}



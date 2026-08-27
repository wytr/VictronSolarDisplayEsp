/* ui.c */
#include "ui.h"
#include <stdlib.h>
#include <stdbool.h>
#include <inttypes.h>
#include <string.h>
#include <lvgl.h>
#include "lv_port.h"
#include "esp_log.h"
#include "victron_ble.h"
#include "victron_products.h"
#include "nvs_flash.h"
#include "config_storage.h"
#include <stdio.h>
#include "ui/ui_state.h"
#include "ui/device_view.h"
#include "ui/view_registry.h"
#include "ui/settings_panel.h"
#include "ui/relay_panel.h"

// Font Awesome symbols (declared in main.c)
LV_FONT_DECLARE(font_awesome_solar_panel_40);
LV_FONT_DECLARE(font_awesome_bolt_40);

static const char *TAG_UI = "UI_MODULE";

static ui_state_t g_ui = {
    .brightness = 100,
    .current_device_type = VICTRON_BLE_RECORD_TEST,
    .current_product_id = 0,
    .has_received_data = false,
    .tab_settings_index = UINT16_MAX,
    .tab_relay_index = UINT16_MAX,
    .relay_tab_enabled = true,
};

// Forward declarations
static void tabview_touch_event_cb(lv_event_t *e);
static void ensure_device_layout(ui_state_t *ui, victron_record_type_t type);
static const char *device_type_name(victron_record_type_t type);

static bool obj_is_descendant(const lv_obj_t *obj, const lv_obj_t *parent)
{
    if (obj == NULL || parent == NULL) {
        return false;
    }
    const lv_obj_t *current = obj;
    while (current != NULL) {
        if (current == parent) {
            return true;
        }
        current = lv_obj_get_parent(current);
    }
    return false;
}

void ui_init(void) {
    ui_state_t *ui = &g_ui;

    nvs_flash_init();
    load_brightness(&ui->brightness);

    ui->active_view = NULL;
    ui->current_device_type = VICTRON_BLE_RECORD_TEST;
    for (size_t i = 0; i < UI_MAX_DEVICE_VIEWS; ++i) {
        ui->views[i] = NULL;
    }

    ui->relay_config.count = 0;
    ui->relay_config.container = NULL;
    ui->relay_config.list = NULL;
    ui->relay_config.add_btn = NULL;
    ui->relay_config.remove_btn = NULL;
    ui->relay_config.dropdown_updating = false;
    for (size_t i = 0; i < UI_MAX_RELAY_BUTTONS; ++i) {
        ui->relay_config.gpio_pins[i] = UI_RELAY_GPIO_UNASSIGNED;
        ui->relay_config.rows[i] = NULL;
        ui->relay_config.labels[i] = NULL;
        ui->relay_config.dropdowns[i] = NULL;
        ui->relay_config.textareas[i] = NULL;
        ui->relay_button_text[i][0] = '\0';
        ui->relay_buttons[i] = NULL;
        ui->relay_button_labels[i] = NULL;
        ui->relay_button_state[i] = false;
    }
    ui->relay_grid = NULL;
    ui->relay_description = NULL;
    ui->relay_refresh_in_progress = false;

    bool relay_enabled = ui->relay_tab_enabled;
    uint8_t saved_count = 0;
    uint8_t saved_pins[UI_MAX_RELAY_BUTTONS];
    char saved_labels[UI_MAX_RELAY_BUTTONS][20];
    for (size_t i = 0; i < UI_MAX_RELAY_BUTTONS; ++i) {
        saved_pins[i] = UI_RELAY_GPIO_UNASSIGNED;
        saved_labels[i][0] = '\0';
    }

    esp_err_t relay_err = load_relay_config(&relay_enabled,
                                            &saved_count,
                                            saved_pins,
                                            saved_labels,
                                            UI_MAX_RELAY_BUTTONS);
    if (relay_err == ESP_OK) {
        if (saved_count > UI_MAX_RELAY_BUTTONS) {
            saved_count = UI_MAX_RELAY_BUTTONS;
        }
        ui->relay_tab_enabled = relay_enabled;
        ui->relay_config.count = saved_count;
        for (size_t i = 0; i < UI_MAX_RELAY_BUTTONS; ++i) {
            ui->relay_config.gpio_pins[i] = (i < saved_count)
                ? saved_pins[i]
                : UI_RELAY_GPIO_UNASSIGNED;
            if (i < saved_count && saved_labels[i][0] != '\0') {
                strncpy(ui->relay_button_text[i], saved_labels[i], sizeof(ui->relay_button_text[i]));
                ui->relay_button_text[i][sizeof(ui->relay_button_text[i]) - 1] = '\0';
            } else {
                ui->relay_button_text[i][0] = '\0';
            }
        }
    } else {
        ui->relay_tab_enabled = true;
    }

    char default_ssid[33]; size_t ssid_len = sizeof(default_ssid);
    char default_pass[65]; size_t pass_len = sizeof(default_pass);
    uint8_t ap_enabled;
    esp_err_t wifi_err = load_wifi_config(default_ssid, &ssid_len, default_pass, &pass_len, &ap_enabled);
    if (wifi_err != ESP_OK) {
        strncpy(default_ssid, "VictronConfig", sizeof(default_ssid));
        default_ssid[sizeof(default_ssid) - 1] = '\0';
        strncpy(default_pass, DEFAULT_AP_PASSWORD, sizeof(default_pass));
        default_pass[sizeof(default_pass) - 1] = '\0';
        ap_enabled = 1;
        save_wifi_config(default_ssid, default_pass, ap_enabled);
    } else if (default_pass[0] == '\0') {
        strncpy(default_pass, DEFAULT_AP_PASSWORD, sizeof(default_pass));
        default_pass[sizeof(default_pass) - 1] = '\0';
        save_wifi_config(default_ssid, default_pass, ap_enabled);
    }

    load_screensaver_settings(&ui->screensaver.enabled,
                              &ui->screensaver.brightness,
                              &ui->screensaver.timeout);

#if LV_USE_THEME_DEFAULT
    lv_theme_default_init(NULL,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        LV_THEME_DEFAULT_DARK,
        &lv_font_montserrat_14
    );
#endif

    ui->tabview   = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 32);
    ui->tab_live  = lv_tabview_add_tab(ui->tabview, "Live");
    ui->tab_settings = lv_tabview_add_tab(ui->tabview, "Settings");
    ui->tab_relay = lv_tabview_add_tab(ui->tabview, "Relay");

    ui->tab_settings_index = lv_obj_get_index(ui->tab_settings);
    ui->tab_relay_index = lv_obj_get_index(ui->tab_relay);

    lv_obj_add_event_cb(ui->tab_live, tabview_touch_event_cb, LV_EVENT_PRESSED, ui);
    lv_obj_add_event_cb(ui->tab_live, tabview_touch_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->tab_live, tabview_touch_event_cb, LV_EVENT_GESTURE, ui);

    lv_obj_add_event_cb(ui->tab_settings, tabview_touch_event_cb, LV_EVENT_PRESSED, ui);
    lv_obj_add_event_cb(ui->tab_settings, tabview_touch_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->tab_settings, tabview_touch_event_cb, LV_EVENT_GESTURE, ui);

    lv_obj_add_event_cb(ui->tab_relay, tabview_touch_event_cb, LV_EVENT_PRESSED, ui);
    lv_obj_add_event_cb(ui->tab_relay, tabview_touch_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->tab_relay, tabview_touch_event_cb, LV_EVENT_GESTURE, ui);

    ui->keyboard = lv_keyboard_create(lv_layer_top());
    lv_obj_set_size(ui->keyboard, LV_HOR_RES, LV_VER_RES/2);
    lv_obj_align(ui->keyboard, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);

    // Styles
    lv_style_init(&ui->styles.small);
    /* Use montserrat 22 for titles as requested */
#if LV_FONT_MONTSERRAT_22
    lv_style_set_text_font(&ui->styles.small, &lv_font_montserrat_22);
#else
    lv_style_set_text_font(&ui->styles.title, &lv_font_montserrat_16);
#endif
    lv_style_set_text_color(&ui->styles.small, lv_color_white());

    lv_style_init(&ui->styles.medium);
    lv_style_set_text_font(&ui->styles.medium, &lv_font_montserrat_24);
    lv_style_set_text_color(&ui->styles.medium, lv_color_white());

    lv_style_init(&ui->styles.big);
    lv_style_set_text_font(&ui->styles.big, &lv_font_montserrat_40);
    lv_style_set_text_color(&ui->styles.big, lv_color_white());

    lv_style_init(&ui->styles.value);
#if LV_FONT_MONTSERRAT_32
    lv_style_set_text_font(&ui->styles.value, &lv_font_montserrat_32);
#elif LV_FONT_MONTSERRAT_30
    lv_style_set_text_font(&ui->styles.value, &lv_font_montserrat_30);
#else
    lv_style_set_text_font(&ui->styles.value, LV_FONT_DEFAULT);
#endif
    lv_style_set_text_color(&ui->styles.value, lv_color_white());

    ui->lbl_no_data = lv_label_create(ui->tab_live);
    lv_label_set_text(ui->lbl_no_data, "No live data received yet");
    lv_obj_add_style(ui->lbl_no_data, &ui->styles.medium, 0);
    lv_label_set_long_mode(ui->lbl_no_data, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(ui->lbl_no_data, lv_pct(90));
    lv_obj_set_style_text_align(ui->lbl_no_data, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(ui->lbl_no_data);

    ui_settings_panel_init(ui, default_ssid, default_pass, ap_enabled);
    ui_relay_panel_init(ui);

    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_PRESSED, ui);
    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(lv_scr_act(), tabview_touch_event_cb, LV_EVENT_GESTURE, ui);

    lv_obj_add_event_cb(ui->tabview, tabview_touch_event_cb, LV_EVENT_PRESSED, ui);
    lv_obj_add_event_cb(ui->tabview, tabview_touch_event_cb, LV_EVENT_CLICKED, ui);
    lv_obj_add_event_cb(ui->tabview, tabview_touch_event_cb, LV_EVENT_GESTURE, ui);

    lvgl_port_unlock();
}

void ui_on_panel_data(const victron_data_t *d) {
    if (d == NULL) {
        return;
    }

    ui_state_t *ui = &g_ui;

    lvgl_port_lock(0);

    if (!ui->has_received_data) {
        ui->has_received_data = true;
        if (ui->lbl_no_data) {
            lv_obj_add_flag(ui->lbl_no_data, LV_OBJ_FLAG_HIDDEN);
        }
    }

    const char *type_str = device_type_name(d->type);
    if (ui->lbl_device_type) {
        lv_label_set_text_fmt(ui->lbl_device_type, "Device: %s", type_str);
    }

    ui->current_product_id = d->product_id;
    if (ui->lbl_product_name) {
        if (d->product_id != 0) {
            const char *prod_name = victron_product_name(d->product_id);
            if (prod_name != NULL) {
                lv_label_set_text_fmt(ui->lbl_product_name,
                                      "Product: %s (0x%04X)",
                                      prod_name,
                                      (unsigned)d->product_id);
            } else {
                lv_label_set_text_fmt(ui->lbl_product_name,
                                      "Product: 0x%04X",
                                      (unsigned)d->product_id);
            }
        } else {
            lv_label_set_text(ui->lbl_product_name, "Product: --");
        }
    }

    ensure_device_layout(ui, d->type);

    if (ui->active_view && ui->active_view->update) {
        ui->active_view->update(ui->active_view, d);
    } else if (ui->lbl_error) {
        if (d->type == VICTRON_BLE_RECORD_TEST) {
            lv_label_set_text(ui->lbl_error, "Unknown device type");
        } else {
            lv_label_set_text(ui->lbl_error, "No renderer for device type");
        }
    }

    lvgl_port_unlock();
}

void ui_notify_user_activity(void)
{
    ui_state_t *ui = &g_ui;
    ui_settings_panel_on_user_activity(ui);
}

void ui_set_ble_mac(const uint8_t *mac) {
    // Format MAC as "XX:XX:XX:XX:XX:XX"
    char mac_str[18];
    snprintf(mac_str, sizeof(mac_str),
             "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[5], mac[4], mac[3], mac[2], mac[1], mac[0]);
    ui_state_t *ui = &g_ui;
    lvgl_port_lock(0);
    ui_settings_panel_set_mac(ui, mac_str);
    lvgl_port_unlock();
}

static void ensure_device_layout(ui_state_t *ui, victron_record_type_t type)
{
    if (ui == NULL) {
        return;
    }

    if (type == ui->current_device_type) {
        return;
    }

    if (ui->active_view && ui->active_view->hide) {
        ui->active_view->hide(ui->active_view);
    }

    ui->active_view = NULL;

    ui_device_view_t *view = ui_view_registry_ensure(ui, type, ui->tab_live);
    if (view && view->show) {
        view->show(view);
        ui->active_view = view;
    } else if (type != VICTRON_BLE_RECORD_TEST) {
        ESP_LOGW(TAG_UI, "No view available for device type 0x%02X", (unsigned)type);
    }

    ui->current_device_type = type;
}

static const char *device_type_name(victron_record_type_t type)
{
    return ui_view_registry_name(type);
}

static void tabview_touch_event_cb(lv_event_t *e) {
    ui_state_t *ui = lv_event_get_user_data(e);
    if (ui == NULL) {
        return;
    }

    lv_event_code_t code = lv_event_get_code(e);
    if (code != LV_EVENT_PRESSED || ui->keyboard == NULL) {
        return;
    }

    if (lv_obj_has_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN)) {
        return;
    }

    lv_obj_t *target = lv_event_get_target(e);
    if (obj_is_descendant(target, ui->keyboard)) {
        return;
    }

    if (ui->wifi.password_toggle != NULL &&
        obj_is_descendant(target, ui->wifi.password_toggle)) {
        return;
    }

    lv_obj_t *ta = lv_keyboard_get_textarea(ui->keyboard);
    if (obj_is_descendant(target, ta)) {
        return;
    }

    if (ta != NULL) {
        lv_obj_clear_state(ta, LV_STATE_FOCUSED);
        lv_event_send(ta, LV_EVENT_DEFOCUSED, NULL);
    } else {
        lv_keyboard_set_textarea(ui->keyboard, NULL);
        lv_obj_add_flag(ui->keyboard, LV_OBJ_FLAG_HIDDEN);
        lv_disp_t *disp = lv_disp_get_default();
        lv_coord_t screen_h = disp ? lv_disp_get_ver_res(disp) : LV_VER_RES;
        lv_obj_set_height(ui->tabview, screen_h);
        lv_obj_update_layout(ui->tabview);
    }
}

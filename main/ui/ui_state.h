#ifndef UI_UI_STATE_H
#define UI_UI_STATE_H

#include <stdbool.h>
#include <stdint.h>
#include <lvgl.h>
#include "victron_ble.h"

struct ui_device_view;

#define UI_MAX_DEVICE_VIEWS 32
#define UI_MAX_RELAY_BUTTONS 8
#define UI_RELAY_GPIO_UNASSIGNED UINT8_MAX

typedef struct {
    lv_style_t small;
    lv_style_t value;
    lv_style_t big;
    lv_style_t medium;
} ui_styles_t;

typedef struct {
    lv_obj_t *ssid;
    lv_obj_t *password;
    lv_obj_t *ap_enable;
    lv_obj_t *password_toggle;
} ui_wifi_controls_t;

typedef struct {
    bool enabled;
    uint8_t brightness;
    uint16_t timeout;
    bool active;
    lv_timer_t *timer;
    lv_obj_t *checkbox;
    lv_obj_t *slider_brightness;
    lv_obj_t *spinbox_timeout;
} ui_screensaver_state_t;

typedef struct {
    uint8_t count;
    uint8_t gpio_pins[UI_MAX_RELAY_BUTTONS];
    lv_obj_t *container;
    lv_obj_t *list;
    lv_obj_t *add_btn;
    lv_obj_t *remove_btn;
    lv_obj_t *rows[UI_MAX_RELAY_BUTTONS];
    lv_obj_t *labels[UI_MAX_RELAY_BUTTONS];
    lv_obj_t *dropdowns[UI_MAX_RELAY_BUTTONS];
    lv_obj_t *textareas[UI_MAX_RELAY_BUTTONS];
    bool dropdown_updating;
} ui_relay_config_t;

typedef struct ui_state {
    lv_obj_t *tabview;
    lv_obj_t *tab_live;
    lv_obj_t *tab_settings;
    lv_obj_t *settings_menu;
    lv_obj_t *tab_relay;
    lv_obj_t *keyboard;
    ui_styles_t styles;
    ui_wifi_controls_t wifi;
    ui_screensaver_state_t screensaver;
    lv_obj_t *lbl_error;
    lv_obj_t *lbl_device_type;
    lv_obj_t *lbl_product_name;
    lv_obj_t *lbl_no_data;
    lv_obj_t *ta_mac;
    lv_obj_t *ta_key;
    uint8_t brightness;
    bool victron_debug_enabled;
    lv_obj_t *victron_debug_checkbox;
    victron_record_type_t current_device_type;
    uint16_t current_product_id;
    struct ui_device_view *active_view;
    struct ui_device_view *views[UI_MAX_DEVICE_VIEWS];
    bool has_received_data;
    lv_obj_t *relay_checkbox;
    uint16_t tab_settings_index;
    uint16_t tab_relay_index;
    bool relay_tab_enabled;
    ui_relay_config_t relay_config;
    lv_obj_t *relay_grid;
    lv_obj_t *relay_description;
    lv_obj_t *relay_buttons[UI_MAX_RELAY_BUTTONS];
    lv_obj_t *relay_button_labels[UI_MAX_RELAY_BUTTONS];
    char relay_button_text[UI_MAX_RELAY_BUTTONS][20];
    bool relay_button_state[UI_MAX_RELAY_BUTTONS];
    bool relay_refresh_in_progress;
} ui_state_t;

#endif /* UI_UI_STATE_H */

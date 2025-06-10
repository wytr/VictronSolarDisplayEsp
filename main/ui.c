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

// Forward declarations
static const char *err_str(uint8_t e);
static const char *charger_state_str(uint8_t s);
static void ta_event_cb(lv_event_t *e);
static void brightness_slider_event_cb(lv_event_t *e);

void ui_init(void) {
    //ESP_LOGI(TAG_UI, "Initializing UI...");

    // Dark theme initialization
#if LV_USE_THEME_DEFAULT
    lv_theme_default_init(NULL,
        lv_palette_main(LV_PALETTE_BLUE),
        lv_palette_main(LV_PALETTE_RED),
        LV_THEME_DEFAULT_DARK,
        &lv_font_montserrat_14
    );
#endif

    // Create tabview
    tabview  = lv_tabview_create(lv_scr_act(), LV_DIR_TOP, 40);
    tab_live = lv_tabview_add_tab(tabview, "Live");
    tab_info = lv_tabview_add_tab(tabview, "Info");

    // Keyboard for Info tab
    kb = lv_keyboard_create(tab_info);
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

    // Icons
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

    // Info tab fields
    lbl_error = lv_label_create(tab_info);
    lv_obj_add_style(lbl_error, &style_title, 0);
    lv_label_set_text(lbl_error, "Err: 0");
    lv_obj_align(lbl_error, LV_ALIGN_BOTTOM_LEFT, 8, -24);

    lv_obj_t *lmac = lv_label_create(tab_info);
    lv_obj_add_style(lmac, &style_title, 0);
    lv_label_set_text(lmac, "MAC Address:");
    lv_obj_align(lmac, LV_ALIGN_TOP_LEFT, 8, 8);

    ta_mac = lv_textarea_create(tab_info);
    lv_textarea_set_one_line(ta_mac, true);
    lv_obj_set_width(ta_mac, lv_pct(80));
    lv_textarea_set_text(ta_mac, "00:00:00:00:00:00");
    lv_obj_align(ta_mac, LV_ALIGN_TOP_LEFT, 8, 32);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_mac, ta_event_cb, LV_EVENT_READY, NULL);

    lv_obj_t *lkey = lv_label_create(tab_info);
    lv_obj_add_style(lkey, &style_title, 0);
    lv_label_set_text(lkey, "AES Key:");
    lv_obj_align(lkey, LV_ALIGN_TOP_LEFT, 8, 72);

    ta_key = lv_textarea_create(tab_info);
    lv_textarea_set_one_line(ta_key, true);
    lv_obj_set_width(ta_key, lv_pct(80));
    lv_textarea_set_text(ta_key, "4B7178E64C828A262CDD5161E3404B7A");
    lv_obj_align(ta_key, LV_ALIGN_TOP_LEFT, 8, 96);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_DEFOCUSED, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_CANCEL, NULL);
    lv_obj_add_event_cb(ta_key, ta_event_cb, LV_EVENT_READY, NULL);

    // Brightness slider
    lv_obj_t *lbl_brightness = lv_label_create(tab_info);
    lv_obj_add_style(lbl_brightness, &style_title, 0);
    lv_label_set_text(lbl_brightness, "Brightness:");
    lv_obj_align(lbl_brightness, LV_ALIGN_TOP_LEFT, 8, 136);

    lv_obj_t *slider = lv_slider_create(tab_info);
    lv_obj_set_width(slider, lv_pct(80));
    lv_obj_align(slider, LV_ALIGN_TOP_LEFT, 8, 160);
    lv_slider_set_range(slider, 1, 100);
    lv_slider_set_value(slider, 5, LV_ANIM_OFF);
    lv_obj_add_event_cb(slider, brightness_slider_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    lvgl_port_unlock();
    //ESP_LOGI(TAG_UI, "UI Initialized");
}

void ui_on_panel_data(const victronPanelData_t *d) {
    int battVraw = d->batteryVoltage;     // centi-volts
    int battV_i  = battVraw / 100;
    int battV_f  = battVraw % 100;

    int battAraw = d->batteryCurrent;     // deci-amps
    int battA_i  = battAraw / 10;
    int battA_f  = abs(battAraw % 10);

    int loadRaw  = ((d->outputCurrentHi & 1) << 8) | d->outputCurrentLo;
    int load_i   = loadRaw / 10;
    int load_f   = loadRaw % 10;

    uint32_t solarW  = d->inputPower;
    uint32_t yieldWh = (uint32_t)(d->todayYield * 0.01f * 1000.0f);
    uint32_t loadWatt = ((loadRaw * battVraw) / 1000);

    //ESP_LOGI(TAG_UI, "Battery: %d.%02d V, %d.%1d A (raw: %d cV, %d dA)",
    //         battV_i, battV_f, battA_i, battA_f, battVraw, battAraw);
    //ESP_LOGI(TAG_UI, "Load: %d.%1d A (raw: %d dA), approx. %lu W",
    //         load_i, load_f, loadRaw, loadWatt);
    //ESP_LOGI(TAG_UI, "Solar input: %lu W, Yield today: %lu Wh",
    //         solarW, yieldWh);
    //ESP_LOGI(TAG_UI, "Device state: %s, Error: %s",
    //         charger_state_str(d->deviceState), err_str(d->errorCode));

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
    switch(e) {
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
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t *ta = lv_event_get_target(e);
    switch (code) {
        case LV_EVENT_FOCUSED:
            //ESP_LOGI(TAG_UI, "TextArea focused");
            lv_keyboard_set_textarea(kb, ta);
            lv_obj_move_foreground(kb);
            lv_obj_clear_flag(kb, LV_OBJ_FLAG_HIDDEN);
            break;
        case LV_EVENT_DEFOCUSED:
            //ESP_LOGI(TAG_UI, "TextArea defocused");
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            break;
        case LV_EVENT_CANCEL:
            //ESP_LOGI(TAG_UI, "Keyboard cancel event");
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            break;
        case LV_EVENT_READY:
            //ESP_LOGI(TAG_UI, "Keyboard ready event");
            lv_keyboard_set_textarea(kb, NULL);
            lv_obj_add_flag(kb, LV_OBJ_FLAG_HIDDEN);
            break;
        default:
            //ESP_LOGI(TAG_UI, "Unhandled event: %d", code);
            break;
    }
}

static void brightness_slider_event_cb(lv_event_t *e) {
    lv_obj_t *slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    bsp_display_brightness_set(val);
    ESP_LOGI(TAG_UI, "Brightness set to %d", val);
}

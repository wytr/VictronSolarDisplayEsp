#include "view_simple_devices.h"

#include <stdio.h>
#include <string.h>

#include "view_simple.h"
#include "ui_format.h"
#include "victron_records.h"

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char *device_state_to_string(uint8_t state)
{
    switch (state) {
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

static const char *charger_error_to_string(uint8_t code)
{
    switch (code) {
    case 0:   return "OK";
    case 1:   return "Battery temp high";
    case 2:   return "Battery volt high";
    case 3:
    case 4:   return "Remote temp sensor";
    case 5:   return "Remote temp lost";
    case 6:
    case 7:   return "Remote volt sense";
    case 8:   return "Remote volt lost";
    case 11:  return "High ripple";
    case 14:  return "Temp too low";
    case 17:  return "Charger temp";
    case 18:  return "Over current";
    case 19:  return "Polarity";
    case 26:  return "Overheated";
    case 27:  return "Short circuit";
    case 33:  return "Input volt high";
    case 34:  return "Input curr high";
    case 38:  return "Input shutdown";
    case 114: return "CPU temp";
    case 116: return "Cal lost";
    default:  return "Unknown";
    }
}

/* -------------------------------------------------------------------------- */
/*  Shared helpers                                                            */
/* -------------------------------------------------------------------------- */

static void format_raw_hex(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL) {
        return;
    }

    char buf[(VICTRON_ENCRYPTED_DATA_MAX_SIZE * 3) + 1];
    size_t pos = 0;
    for (size_t i = 0; i < VICTRON_ENCRYPTED_DATA_MAX_SIZE; ++i) {
        int written = snprintf(buf + pos, sizeof(buf) - pos,
                               "%02X%s",
                               data->record.raw[i],
                               (i + 1U < VICTRON_ENCRYPTED_DATA_MAX_SIZE) ? " " : "");
        if (written < 0) {
            break;
        }
        pos += (size_t)written;
        if (pos >= sizeof(buf)) {
            break;
        }
    }
    buf[sizeof(buf) - 1] = '\0';
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

/* -------------------------------------------------------------------------- */
/*  Inverter (0x03)                                                           */
/* -------------------------------------------------------------------------- */

static void format_inverter_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }

    const victron_record_inverter_t *r = &data->record.inverter;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_inverter_alarm(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }
    const victron_record_inverter_t *r = &data->record.inverter;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->alarm_reason);
}

static void format_inverter_battery_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }
    const victron_record_inverter_t *r = &data->record.inverter;
    ui_label_set_signed_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_inverter_ac_voltage(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }
    const victron_record_inverter_t *r = &data->record.inverter;
    ui_label_set_unsigned_fixed(label, r->ac_voltage_centi, 100, 2, " V");
}

static void format_inverter_ac_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }
    const victron_record_inverter_t *r = &data->record.inverter;
    ui_label_set_unsigned_fixed(label, r->ac_current_deci, 10, 1, " A");
}

static void format_inverter_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_INVERTER) {
        return;
    }
    const victron_record_inverter_t *r = &data->record.inverter;
    lv_label_set_text_fmt(label, "%u VA",
                          (unsigned)r->ac_apparent_power_va);
}

static const ui_simple_label_descriptor_t INVERTER_LABELS[] = {
    { "State", format_inverter_state },
    { "Alarm", format_inverter_alarm },
    { "Battery V", format_inverter_battery_v },
    { "AC Voltage", format_inverter_ac_voltage },
    { "AC Current", format_inverter_ac_current },
    { "Apparent Power", format_inverter_power },
};

static const ui_simple_view_config_t INVERTER_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_INVERTER,
    .labels = INVERTER_LABELS,
    .label_count = ARRAY_SIZE(INVERTER_LABELS),
};

ui_device_view_t *ui_inverter_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &INVERTER_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  DC/DC Converter (0x04)                                                    */
/* -------------------------------------------------------------------------- */

static void format_dcdc_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DCDC_CONVERTER) {
        return;
    }
    const victron_record_dcdc_converter_t *r = &data->record.dcdc;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_dcdc_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DCDC_CONVERTER) {
        return;
    }
    const victron_record_dcdc_converter_t *r = &data->record.dcdc;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          charger_error_to_string(r->charger_error),
                          (unsigned)r->charger_error);
}

static void format_dcdc_input_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DCDC_CONVERTER) {
        return;
    }
    const victron_record_dcdc_converter_t *r = &data->record.dcdc;
    ui_label_set_unsigned_fixed(label, r->input_voltage_centi, 100, 2, " V");
}

static void format_dcdc_output_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DCDC_CONVERTER) {
        return;
    }
    const victron_record_dcdc_converter_t *r = &data->record.dcdc;
    ui_label_set_unsigned_fixed(label, r->output_voltage_centi, 100, 2, " V");
}

static void format_dcdc_off_reason(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DCDC_CONVERTER) {
        return;
    }
    const victron_record_dcdc_converter_t *r = &data->record.dcdc;
    lv_label_set_text_fmt(label, "0x%08lX", (unsigned long)r->off_reason);
}

static const ui_simple_label_descriptor_t DCDC_LABELS[] = {
    { "State", format_dcdc_state },
    { "Error", format_dcdc_error },
    { "Input V", format_dcdc_input_v },
    { "Output V", format_dcdc_output_v },
    { "Off Reason", format_dcdc_off_reason },
};

static const ui_simple_view_config_t DCDC_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_DCDC_CONVERTER,
    .labels = DCDC_LABELS,
    .label_count = ARRAY_SIZE(DCDC_LABELS),
};

ui_device_view_t *ui_dcdc_converter_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &DCDC_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Smart Lithium (0x05)                                                      */
/* -------------------------------------------------------------------------- */

static void format_lithium_flags(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    lv_label_set_text_fmt(label, "0x%08lX", (unsigned long)r->bms_flags);
}

static void format_lithium_errors(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->error_flags);
}

static void format_lithium_batt_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    ui_label_set_unsigned_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_lithium_balancer(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    lv_label_set_text_fmt(label, "%u", (unsigned)r->balancer_status);
}

static void format_lithium_temp(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    int temp_c = (int)r->temperature_c - 40;
    lv_label_set_text_fmt(label, "%d °C", temp_c);
}

static void format_lithium_cells_raw(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_LITHIUM) {
        return;
    }
    const victron_record_smart_lithium_t *r = &data->record.lithium;
    char buf[128];
    snprintf(buf, sizeof(buf),
             "C1=%u C2=%u C3=%u C4=%u C5=%u C6=%u C7=%u C8=%u",
             (unsigned)r->cell1_centi, (unsigned)r->cell2_centi,
             (unsigned)r->cell3_centi, (unsigned)r->cell4_centi,
             (unsigned)r->cell5_centi, (unsigned)r->cell6_centi,
             (unsigned)r->cell7_centi, (unsigned)r->cell8_centi);
    lv_label_set_text(label, buf);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

static const ui_simple_label_descriptor_t LITHIUM_LABELS[] = {
    { "BMS Flags", format_lithium_flags },
    { "Error Flags", format_lithium_errors },
    { "Battery V", format_lithium_batt_v },
    { "Balancer", format_lithium_balancer },
    { "Temperature", format_lithium_temp },
    { "Cells (raw)", format_lithium_cells_raw },
};

static const ui_simple_view_config_t LITHIUM_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_SMART_LITHIUM,
    .labels = LITHIUM_LABELS,
    .label_count = ARRAY_SIZE(LITHIUM_LABELS),
};

ui_device_view_t *ui_smart_lithium_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &LITHIUM_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Inverter RS (0x06)                                                        */
/* -------------------------------------------------------------------------- */

static void format_inverter_rs_note(lv_obj_t *label, const victron_data_t *data)
{
    (void)data;
    if (label == NULL) {
        return;
    }
    lv_label_set_text(label, "Parser not implemented yet");
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

static const ui_simple_label_descriptor_t INVERTER_RS_LABELS[] = {
    { "Status", format_inverter_rs_note },
    { "Raw Data", format_raw_hex },
};

static const ui_simple_view_config_t INVERTER_RS_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_INVERTER_RS,
    .labels = INVERTER_RS_LABELS,
    .label_count = ARRAY_SIZE(INVERTER_RS_LABELS),
};

ui_device_view_t *ui_inverter_rs_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &INVERTER_RS_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  AC Charger (0x08)                                                         */
/* -------------------------------------------------------------------------- */

static void format_ac_charger_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    const victron_record_ac_charger_t *r = &data->record.ac_charger;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_ac_charger_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    const victron_record_ac_charger_t *r = &data->record.ac_charger;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          charger_error_to_string(r->charger_error),
                          (unsigned)r->charger_error);
}

static void format_ac_charger_batt_v(lv_obj_t *label, const victron_data_t *data, uint16_t value)
{
    (void)data;
    if (label == NULL) {
        return;
    }
    ui_label_set_unsigned_fixed(label, value, 100, 2, " V");
}

static void format_ac_charger_batt_a(lv_obj_t *label, uint16_t value)
{
    if (label == NULL) {
        return;
    }
    ui_label_set_unsigned_fixed(label, value, 10, 1, " A");
}

static void format_ac_charger_batt1_v(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_v(label, data, data->record.ac_charger.battery_voltage_1_centi);
}

static void format_ac_charger_batt1_a(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_a(label, data->record.ac_charger.battery_current_1_deci);
}

static void format_ac_charger_batt2_v(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_v(label, data, data->record.ac_charger.battery_voltage_2_centi);
}

static void format_ac_charger_batt2_a(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_a(label, data->record.ac_charger.battery_current_2_deci);
}

static void format_ac_charger_batt3_v(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_v(label, data, data->record.ac_charger.battery_voltage_3_centi);
}

static void format_ac_charger_batt3_a(lv_obj_t *label, const victron_data_t *data)
{
    if (data == NULL || data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    format_ac_charger_batt_a(label, data->record.ac_charger.battery_current_3_deci);
}

static void format_ac_charger_temp(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    const victron_record_ac_charger_t *r = &data->record.ac_charger;
    lv_label_set_text_fmt(label, "%d °C", (int)r->temperature_c);
}

static void format_ac_charger_ac_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_AC_CHARGER) {
        return;
    }
    const victron_record_ac_charger_t *r = &data->record.ac_charger;
    ui_label_set_unsigned_fixed(label, r->ac_current_deci, 10, 1, " A");
}

static const ui_simple_label_descriptor_t AC_CHARGER_LABELS[] = {
    { "State", format_ac_charger_state },
    { "Error", format_ac_charger_error },
    { "Battery 1 V", format_ac_charger_batt1_v },
    { "Battery 1 A", format_ac_charger_batt1_a },
    { "Battery 2 V", format_ac_charger_batt2_v },
    { "Battery 2 A", format_ac_charger_batt2_a },
    { "Battery 3 V", format_ac_charger_batt3_v },
    { "Battery 3 A", format_ac_charger_batt3_a },
    { "AC Current", format_ac_charger_ac_current },
    { "Temperature", format_ac_charger_temp },
};

static const ui_simple_view_config_t AC_CHARGER_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_AC_CHARGER,
    .labels = AC_CHARGER_LABELS,
    .label_count = ARRAY_SIZE(AC_CHARGER_LABELS),
};

ui_device_view_t *ui_ac_charger_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &AC_CHARGER_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Smart Battery Protect (0x09)                                              */
/* -------------------------------------------------------------------------- */

static void format_sbp_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_sbp_output(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "0x%02X", (unsigned)r->output_state);
}

static void format_sbp_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "0x%02X", (unsigned)r->error_code);
}

static void format_sbp_alarm(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->alarm_reason);
}

static void format_sbp_warning(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->warning_reason);
}

static void format_sbp_input_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    ui_label_set_unsigned_fixed(label, r->input_voltage_centi, 100, 2, " V");
}

static void format_sbp_output_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    ui_label_set_unsigned_fixed(label, r->output_voltage_centi, 100, 2, " V");
}

static void format_sbp_off_reason(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT) {
        return;
    }
    const victron_record_smart_battery_protect_t *r = &data->record.sbp;
    lv_label_set_text_fmt(label, "0x%08lX", (unsigned long)r->off_reason);
}

static const ui_simple_label_descriptor_t SBP_LABELS[] = {
    { "State", format_sbp_state },
    { "Output", format_sbp_output },
    { "Error", format_sbp_error },
    { "Alarm", format_sbp_alarm },
    { "Warning", format_sbp_warning },
    { "Input V", format_sbp_input_v },
    { "Output V", format_sbp_output_v },
    { "Off Reason", format_sbp_off_reason },
};

static const ui_simple_view_config_t SBP_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT,
    .labels = SBP_LABELS,
    .label_count = ARRAY_SIZE(SBP_LABELS),
};

ui_device_view_t *ui_sbp_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &SBP_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Lynx Smart BMS (0x0A)                                                     */
/* -------------------------------------------------------------------------- */

static void format_lynx_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    lv_label_set_text_fmt(label, "0x%02X", (unsigned)r->error);
}

static void format_lynx_ttg(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    uint16_t minutes = r->time_to_go_min;
    lv_label_set_text_fmt(label, "%uh %02um",
                          (unsigned)(minutes / 60U),
                          (unsigned)(minutes % 60U));
}

static void format_lynx_batt_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    ui_label_set_unsigned_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_lynx_batt_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    ui_label_set_signed_fixed(label, r->battery_current_deci, 10, 1, " A");
}

static void format_lynx_soc(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    ui_label_set_unsigned_fixed(label, r->soc_deci_percent, 10, 1, " %");
}

static void format_lynx_consumed(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    ui_label_set_signed_fixed(label, r->consumed_ah_deci, 10, 1, " Ah");
}

static void format_lynx_warnings(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    lv_label_set_text_fmt(label, "0x%05lX", (unsigned long)r->warnings_alarms);
}

static void format_lynx_io_status(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->io_status);
}

static void format_lynx_temp(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_LYNX_SMART_BMS) {
        return;
    }
    const victron_record_lynx_smart_bms_t *r = &data->record.lynx;
    lv_label_set_text_fmt(label, "%d °C", (int)r->temperature_c);
}

static const ui_simple_label_descriptor_t LYNX_LABELS[] = {
    { "Error", format_lynx_error },
    { "Time to Go", format_lynx_ttg },
    { "Battery V", format_lynx_batt_v },
    { "Battery Current", format_lynx_batt_current },
    { "SOC", format_lynx_soc },
    { "Consumed Ah", format_lynx_consumed },
    { "Warnings", format_lynx_warnings },
    { "IO Status", format_lynx_io_status },
    { "Temperature", format_lynx_temp },
};

static const ui_simple_view_config_t LYNX_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_LYNX_SMART_BMS,
    .labels = LYNX_LABELS,
    .label_count = ARRAY_SIZE(LYNX_LABELS),
};

ui_device_view_t *ui_lynx_bms_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &LYNX_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Multi RS (0x0B)                                                           */
/* -------------------------------------------------------------------------- */

static void format_multi_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_multi_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          charger_error_to_string(r->charger_error),
                          (unsigned)r->charger_error);
}

static void format_multi_batt_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    ui_label_set_unsigned_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_multi_batt_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    ui_label_set_signed_fixed(label, r->battery_current_deci, 10, 1, " A");
}

static void format_multi_ac_in(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%u", (unsigned)r->active_ac_in);
}

static void format_multi_ac_in_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%u W", (unsigned)r->active_ac_in_power_w);
}

static void format_multi_ac_out_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%u W", (unsigned)r->active_ac_out_power_w);
}

static void format_multi_pv_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    lv_label_set_text_fmt(label, "%u W", (unsigned)r->pv_power_w);
}

static void format_multi_yield(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_MULTI_RS) {
        return;
    }
    const victron_record_multi_rs_t *r = &data->record.multi;
    ui_label_set_unsigned_fixed(label, r->yield_today_centikwh, 100, 2, " kWh");
}

static const ui_simple_label_descriptor_t MULTI_LABELS[] = {
    { "State", format_multi_state },
    { "Error", format_multi_error },
    { "Battery V", format_multi_batt_v },
    { "Battery Current", format_multi_batt_current },
    { "Active AC In", format_multi_ac_in },
    { "AC In Power", format_multi_ac_in_power },
    { "AC Out Power", format_multi_ac_out_power },
    { "PV Power", format_multi_pv_power },
    { "Yield Today", format_multi_yield },
};

static const ui_simple_view_config_t MULTI_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_MULTI_RS,
    .labels = MULTI_LABELS,
    .label_count = ARRAY_SIZE(MULTI_LABELS),
};

ui_device_view_t *ui_multi_rs_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &MULTI_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  VE.Bus (0x0C)                                                             */
/* -------------------------------------------------------------------------- */

static void format_vebus_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_vebus_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "0x%02X", (unsigned)r->ve_bus_error);
}

static void format_vebus_batt_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    ui_label_set_unsigned_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_vebus_batt_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    ui_label_set_signed_fixed(label, r->battery_current_deci, 10, 1, " A");
}

static void format_vebus_ac_in(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%u", (unsigned)r->active_ac_in);
}

static void format_vebus_ac_in_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%lu W", (unsigned long)r->active_ac_in_power_w);
}

static void format_vebus_ac_out_power(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%lu W", (unsigned long)r->ac_out_power_w);
}

static void format_vebus_alarm(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "0x%02X", (unsigned)r->alarm_state);
}

static void format_vebus_temp(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%d °C", (int)r->battery_temp_c);
}

static void format_vebus_soc(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_VE_BUS) {
        return;
    }
    const victron_record_ve_bus_t *r = &data->record.vebus;
    lv_label_set_text_fmt(label, "%u %%", (unsigned)r->soc_percent);
}

static const ui_simple_label_descriptor_t VEBUS_LABELS[] = {
    { "State", format_vebus_state },
    { "VE.Bus Error", format_vebus_error },
    { "Battery V", format_vebus_batt_v },
    { "Battery Current", format_vebus_batt_current },
    { "Active AC In", format_vebus_ac_in },
    { "AC In Power", format_vebus_ac_in_power },
    { "AC Out Power", format_vebus_ac_out_power },
    { "Alarm", format_vebus_alarm },
    { "Battery Temp", format_vebus_temp },
    { "SOC", format_vebus_soc },
};

static const ui_simple_view_config_t VEBUS_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_VE_BUS,
    .labels = VEBUS_LABELS,
    .label_count = ARRAY_SIZE(VEBUS_LABELS),
};

ui_device_view_t *ui_ve_bus_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &VEBUS_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  DC Energy Meter (0x0D)                                                    */
/* -------------------------------------------------------------------------- */

static void format_dcem_mode(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DC_ENERGY_METER) {
        return;
    }
    const victron_record_dc_energy_meter_t *r = &data->record.dcem;
    lv_label_set_text_fmt(label, "%d", (int)r->monitor_mode);
}

static void format_dcem_batt_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DC_ENERGY_METER) {
        return;
    }
    const victron_record_dc_energy_meter_t *r = &data->record.dcem;
    ui_label_set_signed_fixed(label, r->battery_voltage_centi, 100, 2, " V");
}

static void format_dcem_batt_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DC_ENERGY_METER) {
        return;
    }
    const victron_record_dc_energy_meter_t *r = &data->record.dcem;
    ui_label_set_signed_fixed(label, r->battery_current_milli, 1000, 3, " A");
}

static void format_dcem_aux(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DC_ENERGY_METER) {
        return;
    }
    const victron_record_dc_energy_meter_t *r = &data->record.dcem;
    char buf[32];
    ui_format_aux_value(r->aux_input, r->aux_value, buf, sizeof(buf));
    lv_label_set_text_fmt(label, "Input %u: %s",
                          (unsigned)r->aux_input, buf);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_LEFT, 0);
}

static void format_dcem_alarm(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_DC_ENERGY_METER) {
        return;
    }
    const victron_record_dc_energy_meter_t *r = &data->record.dcem;
    lv_label_set_text_fmt(label, "0x%04X", (unsigned)r->alarm_reason);
}

static const ui_simple_label_descriptor_t DCEM_LABELS[] = {
    { "Monitor Mode", format_dcem_mode },
    { "Battery V", format_dcem_batt_v },
    { "Battery Current", format_dcem_batt_current },
    { "Aux", format_dcem_aux },
    { "Alarm", format_dcem_alarm },
};

static const ui_simple_view_config_t DCEM_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_DC_ENERGY_METER,
    .labels = DCEM_LABELS,
    .label_count = ARRAY_SIZE(DCEM_LABELS),
};

ui_device_view_t *ui_dc_energy_meter_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &DCEM_VIEW_CONFIG);
}

/* -------------------------------------------------------------------------- */
/*  Orion XS (0x0F)                                                           */
/* -------------------------------------------------------------------------- */

static void format_orion_state(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          device_state_to_string(r->device_state),
                          (unsigned)r->device_state);
}

static void format_orion_error(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    lv_label_set_text_fmt(label, "%s (0x%02X)",
                          charger_error_to_string(r->charger_error),
                          (unsigned)r->charger_error);
}

static void format_orion_input_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    ui_label_set_unsigned_fixed(label, r->input_voltage_centi, 100, 2, " V");
}

static void format_orion_input_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    ui_label_set_unsigned_fixed(label, r->input_current_deci, 10, 1, " A");
}

static void format_orion_output_v(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    ui_label_set_unsigned_fixed(label, r->output_voltage_centi, 100, 2, " V");
}

static void format_orion_output_current(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    ui_label_set_unsigned_fixed(label, r->output_current_deci, 10, 1, " A");
}

static void format_orion_off_reason(lv_obj_t *label, const victron_data_t *data)
{
    if (label == NULL || data == NULL ||
        data->type != VICTRON_BLE_RECORD_ORION_XS) {
        return;
    }
    const victron_record_orion_xs_t *r = &data->record.orion;
    lv_label_set_text_fmt(label, "0x%08lX", (unsigned long)r->off_reason);
}

static const ui_simple_label_descriptor_t ORION_LABELS[] = {
    { "State", format_orion_state },
    { "Error", format_orion_error },
    { "Input V", format_orion_input_v },
    { "Input Current", format_orion_input_current },
    { "Output V", format_orion_output_v },
    { "Output Current", format_orion_output_current },
    { "Off Reason", format_orion_off_reason },
};

static const ui_simple_view_config_t ORION_VIEW_CONFIG = {
    .type = VICTRON_BLE_RECORD_ORION_XS,
    .labels = ORION_LABELS,
    .label_count = ARRAY_SIZE(ORION_LABELS),
};

ui_device_view_t *ui_orion_xs_view_create(ui_state_t *ui, lv_obj_t *parent)
{
    return ui_simple_view_create(ui, parent, &ORION_VIEW_CONFIG);
}

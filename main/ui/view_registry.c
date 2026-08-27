#include "view_registry.h"

#include "view_solar_charger.h"
#include "view_battery_monitor.h"
#include "view_simple_devices.h"
#include "esp_log.h"

static const char *TAG = "UI_VIEW_REGISTRY";

static const ui_view_descriptor_t VIEW_DESCRIPTORS[] = {
    { VICTRON_BLE_RECORD_SOLAR_CHARGER, "0x01 Solar Charger", ui_solar_view_create },
    { VICTRON_BLE_RECORD_BATTERY_MONITOR, "0x02 Battery Monitor", ui_battery_view_create },
    { VICTRON_BLE_RECORD_INVERTER, "0x03 Inverter", ui_inverter_view_create },
    { VICTRON_BLE_RECORD_DCDC_CONVERTER, "0x04 DC/DC Converter", ui_dcdc_converter_view_create },
    { VICTRON_BLE_RECORD_SMART_LITHIUM, "0x05 Smart Lithium", ui_smart_lithium_view_create },
    { VICTRON_BLE_RECORD_INVERTER_RS, "0x06 Inverter RS", ui_inverter_rs_view_create },
    { VICTRON_BLE_RECORD_AC_CHARGER, "0x08 AC Charger", ui_ac_charger_view_create },
    { VICTRON_BLE_RECORD_SMART_BATTERY_PROTECT, "0x09 Smart Battery Protect", ui_sbp_view_create },
    { VICTRON_BLE_RECORD_LYNX_SMART_BMS, "0x0A Lynx Smart BMS", ui_lynx_bms_view_create },
    { VICTRON_BLE_RECORD_MULTI_RS, "0x0B Multi RS", ui_multi_rs_view_create },
    { VICTRON_BLE_RECORD_VE_BUS, "0x0C VE.Bus", ui_ve_bus_view_create },
    { VICTRON_BLE_RECORD_DC_ENERGY_METER, "0x0D DC Energy Meter", ui_dc_energy_meter_view_create },
    { VICTRON_BLE_RECORD_ORION_XS, "0x0F Orion XS", ui_orion_xs_view_create },
};

static size_t descriptor_count(void)
{
    return sizeof(VIEW_DESCRIPTORS) / sizeof(VIEW_DESCRIPTORS[0]);
}

static size_t type_to_index(victron_record_type_t type)
{
    return (size_t)type;
}

const ui_view_descriptor_t *ui_view_registry_find(victron_record_type_t type)
{
    size_t count = descriptor_count();
    for (size_t i = 0; i < count; ++i) {
        if (VIEW_DESCRIPTORS[i].type == type) {
            return &VIEW_DESCRIPTORS[i];
        }
    }
    return NULL;
}

ui_device_view_t *ui_view_registry_ensure(ui_state_t *ui,
                                          victron_record_type_t type,
                                          lv_obj_t *parent)
{
    if (ui == NULL) {
        return NULL;
    }

    size_t index = type_to_index(type);
    if (index >= UI_MAX_DEVICE_VIEWS) {
        return NULL;
    }

    ui_device_view_t *view = ui->views[index];
    if (view == NULL) {
        const ui_view_descriptor_t *desc = ui_view_registry_find(type);
        if (desc == NULL || desc->create == NULL || parent == NULL) {
            return NULL;
        }
        view = desc->create(ui, parent);
        if (view == NULL) {
            ESP_LOGE(TAG, "Failed to create view for type 0x%02X", (unsigned)type);
            return NULL;
        }
        ui->views[index] = view;
    }

    return view;
}

const char *ui_view_registry_name(victron_record_type_t type)
{
    const ui_view_descriptor_t *desc = ui_view_registry_find(type);
    return desc ? desc->name : "Unknown";
}

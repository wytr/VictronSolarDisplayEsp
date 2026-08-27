#ifndef UI_VIEW_SIMPLE_DEVICES_H
#define UI_VIEW_SIMPLE_DEVICES_H

#include "ui/device_view.h"
#include "ui/ui_state.h"

ui_device_view_t *ui_inverter_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_dcdc_converter_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_smart_lithium_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_inverter_rs_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_ac_charger_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_sbp_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_lynx_bms_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_multi_rs_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_ve_bus_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_dc_energy_meter_view_create(ui_state_t *ui, lv_obj_t *parent);
ui_device_view_t *ui_orion_xs_view_create(ui_state_t *ui, lv_obj_t *parent);

#endif /* UI_VIEW_SIMPLE_DEVICES_H */


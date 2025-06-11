/* ui.h */
#ifndef UI_H
#define UI_H

#include <stdint.h>
#include <lvgl.h>
#include "victron_ble.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize all LVGL UI elements, including Live and Info tabs.
 */
void ui_init(void);

/**
 * BLE data callback to update the UI with new panel data.
 * @param d Pointer to the victronPanelData_t structure containing sensor readings.
 */
void ui_on_panel_data(const victronPanelData_t *d);
void ui_set_ble_mac(const uint8_t *mac);

#ifdef __cplusplus
}
#endif

#endif /* UI_H */

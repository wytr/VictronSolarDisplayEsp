// victron_ble.h
#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <stdint.h>
#include <stdbool.h>
#include "victron_records.h"

#ifdef __cplusplus
extern "C" {
#endif

// -----------------------------------------------------------------------------
// Unified Victron BLE data container
// -----------------------------------------------------------------------------

typedef struct {
    victron_record_type_t type;  // record type (e.g., SOLAR_CHARGER, BATTERY_MONITOR)
    uint16_t              product_id; // Victron product identifier
    victron_record_t      record; // parsed record data (union of all device types)
} victron_data_t;

// -----------------------------------------------------------------------------
// Public interface
// -----------------------------------------------------------------------------

// Called by BLE layer when a valid frame is received (UI can override this)
extern void ui_set_ble_mac(const uint8_t *mac);

// Callback for receiving new Victron data frames
typedef void (*victron_data_cb_t)(const victron_data_t *data);

// Initialize BLE scanning and decryption for Victron Smart devices
void victron_ble_init(void);

// Register a callback to receive decoded Victron BLE data
void victron_ble_register_callback(victron_data_cb_t cb);

// Enable or disable verbose/debug logging
void victron_ble_set_debug(bool enabled);

#ifdef __cplusplus
}
#endif

#endif // VICTRON_BLE_H

// victron_ble.h
#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Parsed Victron panel data
typedef struct {
    uint8_t  deviceState;
    uint8_t  errorCode;
    int16_t  batteryVoltage;    // centi-volts
    int16_t  batteryCurrent;    // deci-amps
    uint16_t todayYield;        // centi-amp-hours
    uint16_t inputPower;        // watts
    uint8_t  outputCurrentLo;   // low 8 bits of 9-bit load current
    uint8_t  outputCurrentHi;   // msb + unused bits
    uint8_t  unused[4];
} victronPanelData_t;

extern void ui_set_ble_mac(const uint8_t *mac);
// Callback for receiving new panel data
typedef void (*victron_data_cb_t)(const victronPanelData_t* data);

// Initialize BLE scanning and decryption for Victron SmartSolar
void victron_ble_init(void);

// Register a callback to be invoked with each decrypted victronPanelData_t
void victron_ble_register_callback(victron_data_cb_t cb);

#ifdef __cplusplus
}
#endif

#endif // VICTRON_BLE_H



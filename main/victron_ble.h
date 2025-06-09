// victron_ble.h
#ifndef VICTRON_BLE_H
#define VICTRON_BLE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize BLE scanning and decryption for Victron SmartSolar beacons
 */
void victron_ble_init(void);

/**
 * Shared data updated by BLE callbacks
 */
extern float batVol;
extern float powIn;
extern int readings;
extern long nread;
extern int yeTod;
extern int deState;
extern char savedDeviceName[32];

#ifdef __cplusplus
}
#endif

#endif // VICTRON_BLE_H

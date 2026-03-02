/* SPDX-License-Identifier: Apache-2.0 */

#ifndef BLE_CENTRAL_H_
#define BLE_CENTRAL_H_

#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>

/**
 * Initialize BLE central mode and start scanning for HID devices
 * @return 0 on success, negative error code on failure
 */
int ble_central_init(void);

/**
 * Start scanning for BLE HID devices
 * @return 0 on success, negative error code on failure
 */
int ble_central_start_scan(void);

/**
 * Stop scanning
 */
void ble_central_stop_scan(void);

/**
 * Register a bonded device for reconnection scanning
 * @param addr Pointer to device address
 * @return 0 on success, negative error code on failure
 */
int ble_central_add_bonded_filter(const bt_addr_le_t *addr);

/**
 * Get the current BLE connection
 * @return Pointer to connection object, or NULL if not connected
 */
struct bt_conn *ble_central_get_conn(void);

/**
 * Check if connected to a HID device
 * @return true if connected, false otherwise
 */
bool ble_central_is_connected(void);

/**
 * Disconnect from current device
 * @return 0 on success, negative error code on failure
 */
int ble_central_disconnect(void);

#endif /* BLE_CENTRAL_H_ */

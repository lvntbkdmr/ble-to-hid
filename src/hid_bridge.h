/* SPDX-License-Identifier: Apache-2.0 */

#ifndef HID_BRIDGE_H_
#define HID_BRIDGE_H_

#include <stdint.h>

/**
 * Initialize the HID bridge
 * Sets up the connection between BLE HOGP reports and USB HID output
 * @return 0 on success, negative error code on failure
 */
int hid_bridge_init(void);

/**
 * Handle incoming BLE HID report and forward to USB
 * Called from HOGP client when a report is received
 * @param report Pointer to report data (8 bytes for boot keyboard)
 * @param len Length of report data
 */
void hid_bridge_handle_report(const uint8_t *report, uint8_t len);

/**
 * Handle BLE disconnection
 * Releases all keys on USB to prevent stuck keys
 */
void hid_bridge_on_disconnect(void);

#endif /* HID_BRIDGE_H_ */

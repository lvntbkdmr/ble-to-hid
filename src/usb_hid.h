/* SPDX-License-Identifier: Apache-2.0 */

#ifndef USB_HID_H_
#define USB_HID_H_

#include <stdint.h>

/* Boot keyboard report: 8 bytes
 * Byte 0: Modifier keys (Ctrl, Shift, Alt, GUI)
 * Byte 1: Reserved
 * Bytes 2-7: Key codes (up to 6 simultaneous keys)
 */
#define USB_HID_REPORT_SIZE 8

/**
 * Initialize USB HID keyboard device
 * @return 0 on success, negative error code on failure
 */
int usb_hid_init(void);

/**
 * Send a keyboard report over USB
 * @param report Pointer to 8-byte boot keyboard report
 * @return 0 on success, negative error code on failure
 */
int usb_hid_send_report(const uint8_t *report);

/**
 * Release all keys (send empty report)
 * Used when BLE disconnects to prevent stuck keys
 * @return 0 on success, negative error code on failure
 */
int usb_hid_release_all(void);

/**
 * Check if USB HID is ready to send reports
 * @return true if ready, false otherwise
 */
bool usb_hid_ready(void);

#endif /* USB_HID_H_ */

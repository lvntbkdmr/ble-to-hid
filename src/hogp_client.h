/* SPDX-License-Identifier: Apache-2.0 */

#ifndef HOGP_CLIENT_H_
#define HOGP_CLIENT_H_

#include <zephyr/bluetooth/conn.h>

/**
 * Callback type for receiving HID input reports
 * @param report Pointer to report data
 * @param len Length of report data
 */
typedef void (*hogp_report_cb_t)(const uint8_t *report, uint8_t len);

/**
 * Initialize HOGP client
 * @param cb Callback for received HID reports
 * @return 0 on success, negative error code on failure
 */
int hogp_client_init(hogp_report_cb_t cb);

/**
 * Start HOGP service discovery on a connection
 * @param conn BLE connection
 * @return 0 on success, negative error code on failure
 */
int hogp_client_discover(struct bt_conn *conn);

/**
 * Check if HOGP discovery is complete and subscribed to reports
 * @return true if ready, false otherwise
 */
bool hogp_client_ready(void);

#endif /* HOGP_CLIENT_H_ */

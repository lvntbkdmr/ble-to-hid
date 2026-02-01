/* SPDX-License-Identifier: Apache-2.0 */

#ifndef PAIRING_H_
#define PAIRING_H_

/**
 * Initialize pairing callbacks for passkey display
 * This sets up Bluetooth authentication callbacks for
 * displaying pairing passkeys over USB serial console
 */
void pairing_init(void);

/**
 * Clear all stored bonds
 * @return 0 on success, negative error code on failure
 */
int pairing_clear_bonds(void);

#endif /* PAIRING_H_ */

/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/settings/settings.h>
#include <zephyr/logging/log.h>

#include "pairing.h"

LOG_MODULE_REGISTER(pairing, LOG_LEVEL_INF);

/*
 * Passkey display callback
 * Called when the Corne keyboard requests pairing
 * The passkey is displayed on USB serial console
 * User must enter this passkey on the Corne keyboard
 */
static void auth_passkey_display(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	LOG_INF("========================================");
	LOG_INF("PAIRING REQUEST from %s", addr);
	LOG_INF("Enter this passkey on your keyboard:");
	LOG_INF("");
	LOG_INF("        %06u", passkey);
	LOG_INF("");
	LOG_INF("========================================");

	/* Also print to console directly for visibility */
	printk("\n");
	printk("========================================\n");
	printk("PAIRING REQUEST from %s\n", addr);
	printk("Enter this passkey on your keyboard:\n");
	printk("\n");
	printk("        %06u\n", passkey);
	printk("\n");
	printk("========================================\n");
	printk("\n");
}

/*
 * Passkey entry request callback
 * This is called if WE need to enter a passkey
 * For this use case, we display passkey, not enter it
 */
static void auth_passkey_entry(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_WRN("Passkey entry requested by %s - not supported", addr);

	/* Cancel authentication - we can only display passkeys */
	bt_conn_auth_cancel(conn);
}

/*
 * Pairing confirmation callback
 * Called for Just Works pairing or numeric comparison
 */
static void auth_passkey_confirm(struct bt_conn *conn, unsigned int passkey)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Passkey confirm for %s: %06u", addr, passkey);

	/* Auto-confirm for numeric comparison */
	bt_conn_auth_passkey_confirm(conn);
}

/*
 * Pairing cancelled callback
 */
static void auth_cancel(struct bt_conn *conn)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_WRN("Pairing cancelled: %s", addr);
}

/*
 * Pairing completed callback
 */
static void pairing_complete(struct bt_conn *conn, bool bonded)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (bonded) {
		LOG_INF("========================================");
		LOG_INF("PAIRING SUCCESSFUL with %s", addr);
		LOG_INF("Bond stored - will auto-reconnect");
		LOG_INF("========================================");

		printk("\n");
		printk("========================================\n");
		printk("PAIRING SUCCESSFUL with %s\n", addr);
		printk("Bond stored - will auto-reconnect\n");
		printk("========================================\n");
		printk("\n");
	} else {
		LOG_INF("Pairing complete (not bonded): %s", addr);
	}
}

/*
 * Pairing failed callback
 */
static void pairing_failed(struct bt_conn *conn, enum bt_security_err reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_ERR("Pairing failed: %s, reason %d", addr, reason);

	printk("\n");
	printk("PAIRING FAILED with %s (reason %d)\n", addr, reason);
	printk("Please try again.\n");
	printk("\n");
}

/* Authentication callbacks */
static struct bt_conn_auth_cb auth_cb = {
	.passkey_display = auth_passkey_display,
	.passkey_entry = auth_passkey_entry,
	.passkey_confirm = auth_passkey_confirm,
	.cancel = auth_cancel,
};

/* Pairing info callbacks */
static struct bt_conn_auth_info_cb auth_info_cb = {
	.pairing_complete = pairing_complete,
	.pairing_failed = pairing_failed,
};

void pairing_init(void)
{
	int err;

	err = bt_conn_auth_cb_register(&auth_cb);
	if (err) {
		LOG_ERR("Failed to register auth callbacks: %d", err);
	}

	err = bt_conn_auth_info_cb_register(&auth_info_cb);
	if (err) {
		LOG_ERR("Failed to register auth info callbacks: %d", err);
	}

	LOG_INF("Pairing callbacks registered");
	LOG_INF("Passkeys will be displayed on USB serial console");
}

/* Helper to iterate and unpair all bonds */
static void unpair_cb(const struct bt_bond_info *info, void *user_data)
{
	int err;
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(&info->addr, addr, sizeof(addr));

	err = bt_unpair(BT_ID_DEFAULT, &info->addr);
	if (err) {
		LOG_ERR("Failed to unpair %s: %d", addr, err);
	} else {
		LOG_INF("Unpaired: %s", addr);
	}
}

int pairing_clear_bonds(void)
{
	LOG_INF("Clearing all bonds...");
	bt_foreach_bond(BT_ID_DEFAULT, unpair_cb, NULL);
	LOG_INF("All bonds cleared");
	return 0;
}

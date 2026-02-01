/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/hci.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/uuid.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/scan.h>
#include <zephyr/logging/log.h>

#include "ble_central.h"
#include "hogp_client.h"
#include "pairing.h"
#include "hid_bridge.h"

LOG_MODULE_REGISTER(ble_central, LOG_LEVEL_INF);

/* HID Service UUID */
static struct bt_uuid_16 hid_uuid = BT_UUID_INIT_16(BT_UUID_HIDS_VAL);

static struct bt_conn *current_conn;
static bool scanning;

/* BLE connection parameters for low latency */
static struct bt_le_conn_param conn_param = {
	.interval_min = 6,   /* 7.5ms (6 * 1.25ms) */
	.interval_max = 12,  /* 15ms (12 * 1.25ms) */
	.latency = 0,
	.timeout = 400,      /* 4s */
};

static void scan_filter_match(struct bt_scan_device_info *device_info,
			      struct bt_scan_filter_match *filter_match,
			      bool connectable)
{
	char addr[BT_ADDR_LE_STR_LEN];

	if (!connectable) {
		return;
	}

	bt_addr_le_to_str(device_info->recv_info->addr, addr, sizeof(addr));
	LOG_INF("Found HID device: %s, RSSI %d", addr,
		device_info->recv_info->rssi);
}

static void scan_connecting_error(struct bt_scan_device_info *device_info)
{
	LOG_WRN("Connecting failed");
}

static void scan_connecting(struct bt_scan_device_info *device_info,
			    struct bt_conn *conn)
{
	LOG_INF("Connecting to device...");
	current_conn = bt_conn_ref(conn);
}

BT_SCAN_CB_INIT(scan_cb, scan_filter_match, NULL,
		scan_connecting_error, scan_connecting);

static void connected(struct bt_conn *conn, uint8_t err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Failed to connect to %s (err %u)", addr, err);
		if (current_conn) {
			bt_conn_unref(current_conn);
			current_conn = NULL;
		}
		/* Restart scanning */
		ble_central_start_scan();
		return;
	}

	LOG_INF("Connected: %s", addr);

	/* Request connection parameter update for low latency */
	int ret = bt_conn_le_param_update(conn, &conn_param);
	if (ret) {
		LOG_WRN("Failed to request connection params update: %d", ret);
	}

	/* Set security level to trigger pairing */
	ret = bt_conn_set_security(conn, BT_SECURITY_L2);
	if (ret) {
		LOG_ERR("Failed to set security: %d", ret);
	}
}

static void disconnected(struct bt_conn *conn, uint8_t reason)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));
	LOG_INF("Disconnected: %s (reason %u)", addr, reason);

	/* Release all keys on USB to prevent stuck keys */
	hid_bridge_on_disconnect();

	if (current_conn) {
		bt_conn_unref(current_conn);
		current_conn = NULL;
	}

	/* Restart scanning to reconnect */
	ble_central_start_scan();
}

static void security_changed(struct bt_conn *conn, bt_security_t level,
			     enum bt_security_err err)
{
	char addr[BT_ADDR_LE_STR_LEN];

	bt_addr_le_to_str(bt_conn_get_dst(conn), addr, sizeof(addr));

	if (err) {
		LOG_ERR("Security failed: %s level %u err %d", addr, level, err);
		bt_conn_disconnect(conn, BT_HCI_ERR_AUTH_FAIL);
		return;
	}

	LOG_INF("Security changed: %s level %u", addr, level);

	/* Security established, now discover HOGP service */
	hogp_client_discover(conn);
}

static void le_param_updated(struct bt_conn *conn, uint16_t interval,
			     uint16_t latency, uint16_t timeout)
{
	LOG_INF("Connection params updated: interval %u (%.2f ms), "
		"latency %u, timeout %u",
		interval, interval * 1.25, latency, timeout);
}

BT_CONN_CB_DEFINE(conn_callbacks) = {
	.connected = connected,
	.disconnected = disconnected,
	.security_changed = security_changed,
	.le_param_updated = le_param_updated,
};

int ble_central_init(void)
{
	int err;

	/* Initialize Bluetooth */
	err = bt_enable(NULL);
	if (err) {
		LOG_ERR("Bluetooth init failed: %d", err);
		return err;
	}

	LOG_INF("Bluetooth initialized");

	/* Load stored bonds */
	settings_load();

	/* Initialize pairing callbacks */
	pairing_init();

	/* Initialize scan module */
	struct bt_scan_init_param scan_init = {
		.connect_if_match = 1,
		.scan_param = NULL, /* Use default params */
		.conn_param = &conn_param,
	};

	bt_scan_init(&scan_init);
	bt_scan_cb_register(&scan_cb);

	/* Add filter for HID Service UUID */
	err = bt_scan_filter_add(BT_SCAN_FILTER_TYPE_UUID, &hid_uuid);
	if (err) {
		LOG_ERR("Failed to add UUID filter: %d", err);
		return err;
	}

	err = bt_scan_filter_enable(BT_SCAN_UUID_FILTER, false);
	if (err) {
		LOG_ERR("Failed to enable filters: %d", err);
		return err;
	}

	LOG_INF("BLE Central initialized");
	return 0;
}

int ble_central_start_scan(void)
{
	int err;

	if (scanning) {
		return 0;
	}

	if (current_conn) {
		LOG_INF("Already connected, not scanning");
		return 0;
	}

	err = bt_scan_start(BT_SCAN_TYPE_SCAN_ACTIVE);
	if (err) {
		LOG_ERR("Scanning failed to start: %d", err);
		return err;
	}

	scanning = true;
	LOG_INF("Scanning for HID devices...");
	return 0;
}

void ble_central_stop_scan(void)
{
	if (!scanning) {
		return;
	}

	bt_scan_stop();
	scanning = false;
	LOG_INF("Scanning stopped");
}

struct bt_conn *ble_central_get_conn(void)
{
	return current_conn;
}

bool ble_central_is_connected(void)
{
	return current_conn != NULL;
}

int ble_central_disconnect(void)
{
	if (!current_conn) {
		return -ENOTCONN;
	}

	return bt_conn_disconnect(current_conn, BT_HCI_ERR_REMOTE_USER_TERM_CONN);
}

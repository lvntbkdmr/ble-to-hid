/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/hogp.h>
#include <zephyr/logging/log.h>

#include "hogp_client.h"

LOG_MODULE_REGISTER(hogp_client, LOG_LEVEL_INF);

static struct bt_hogp hogp;
static hogp_report_cb_t report_callback;
static bool hogp_ready;

/* Boot keyboard input report handler */
static uint8_t hogp_boot_kbd_report_read(struct bt_hogp *hogp_ctx,
					 struct bt_hogp_rep_info *rep,
					 uint8_t err,
					 const uint8_t *data)
{
	if (err) {
		LOG_ERR("Boot keyboard report read error: %u", err);
		return BT_GATT_ITER_STOP;
	}

	if (!data) {
		LOG_DBG("Boot keyboard report unsubscribed");
		return BT_GATT_ITER_STOP;
	}

	uint8_t len = bt_hogp_rep_size(rep);
	LOG_DBG("Boot keyboard report received, len=%u", len);

	/* Forward to registered callback */
	if (report_callback) {
		report_callback(data, len);
	}

	return BT_GATT_ITER_CONTINUE;
}

/* HOGP ready callback */
static void hogp_ready_cb(struct bt_hogp *hogp_ctx)
{
	int err;
	struct bt_hogp_rep_info *boot_kbd_rep;

	LOG_INF("HOGP service ready");

	/* Get boot keyboard input report */
	boot_kbd_rep = bt_hogp_rep_boot_kbd_in(hogp_ctx);
	if (!boot_kbd_rep) {
		LOG_ERR("Boot keyboard input report not found");
		return;
	}

	LOG_INF("Boot keyboard report found, size=%u",
		bt_hogp_rep_size(boot_kbd_rep));

	/* Subscribe to boot keyboard reports */
	err = bt_hogp_rep_subscribe(hogp_ctx, boot_kbd_rep,
				    hogp_boot_kbd_report_read);
	if (err) {
		LOG_ERR("Failed to subscribe to boot keyboard: %d", err);
		return;
	}

	LOG_INF("Subscribed to boot keyboard input reports");
	hogp_ready = true;
}

/* HOGP protocol mode change callback */
static void hogp_pm_update_cb(struct bt_hogp *hogp_ctx)
{
	uint8_t pm = bt_hogp_pm_get(hogp_ctx);
	LOG_INF("Protocol mode: %s", pm == BT_HIDS_PM_BOOT ? "Boot" : "Report");
}

static struct bt_hogp_init_params hogp_init_params = {
	.ready_cb = hogp_ready_cb,
	.pm_update_cb = hogp_pm_update_cb,
};

/* GATT Discovery completed callback */
static void discovery_completed(struct bt_gatt_dm *dm, void *ctx)
{
	int err;

	ARG_UNUSED(ctx);
	LOG_INF("GATT discovery completed");

	bt_gatt_dm_data_print(dm);

	/* Initialize HOGP with discovered services */
	err = bt_hogp_handles_assign(dm, &hogp);
	if (err) {
		LOG_ERR("Failed to assign HOGP handles: %d", err);
		goto release;
	}

	/* Set boot protocol mode for standard 8-byte reports */
	err = bt_hogp_pm_write(&hogp, BT_HIDS_PM_BOOT);
	if (err) {
		LOG_ERR("Failed to set boot protocol mode: %d", err);
		/* Continue anyway, some devices only support boot protocol */
	}

release:
	err = bt_gatt_dm_data_release(dm);
	if (err) {
		LOG_ERR("Failed to release discovery data: %d", err);
	}
}

/* GATT Discovery service not found callback */
static void discovery_service_not_found(struct bt_conn *conn, void *ctx)
{
	LOG_ERR("HID service not found");
}

/* GATT Discovery error callback */
static void discovery_error_found(struct bt_conn *conn, int err, void *ctx)
{
	LOG_ERR("GATT discovery error: %d", err);
}

static struct bt_gatt_dm_cb discovery_cb = {
	.completed = discovery_completed,
	.service_not_found = discovery_service_not_found,
	.error_found = discovery_error_found,
};

int hogp_client_init(hogp_report_cb_t cb)
{
	report_callback = cb;
	hogp_ready = false;

	bt_hogp_init(&hogp, &hogp_init_params);

	LOG_INF("HOGP client initialized");
	return 0;
}

int hogp_client_discover(struct bt_conn *conn)
{
	int err;

	LOG_INF("Starting HOGP discovery...");
	hogp_ready = false;

	err = bt_gatt_dm_start(conn, BT_UUID_HIDS, &discovery_cb, NULL);
	if (err) {
		LOG_ERR("Failed to start GATT discovery: %d", err);
		return err;
	}

	return 0;
}

bool hogp_client_ready(void)
{
	return hogp_ready;
}

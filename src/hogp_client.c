/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/bluetooth/bluetooth.h>
#include <zephyr/bluetooth/conn.h>
#include <zephyr/bluetooth/gatt.h>
#include <bluetooth/gatt_dm.h>
#include <bluetooth/services/hogp.h>
#include <bluetooth/services/hids.h>
#include <zephyr/logging/log.h>

#include "hogp_client.h"

LOG_MODULE_REGISTER(hogp_client, LOG_LEVEL_INF);

static struct bt_hogp hogp;
static hogp_report_cb_t report_callback;
static bool hogp_ready;
static uint8_t subscribed_reports;

/* Input report notification handler */
static uint8_t hogp_report_notify(struct bt_hogp *hogp_ctx,
				  struct bt_hogp_rep_info *rep,
				  uint8_t err,
				  const uint8_t *data)
{
	if (err) {
		LOG_ERR("Report notification error: %u", err);
		return BT_GATT_ITER_STOP;
	}

	if (!data) {
		LOG_DBG("Report unsubscribed (id=%u)", bt_hogp_rep_id(rep));
		return BT_GATT_ITER_STOP;
	}

	uint8_t id = bt_hogp_rep_id(rep);
	size_t len = bt_hogp_rep_size(rep);

	LOG_DBG("Report received: id=%u, len=%zu", id, len);

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
	struct bt_hogp_rep_info *rep = NULL;
	size_t rep_count;

	LOG_INF("HOGP service ready");

	rep_count = bt_hogp_rep_count(hogp_ctx);
	LOG_INF("Found %zu HID reports", rep_count);

	subscribed_reports = 0;

	/* Iterate through all reports and subscribe to input reports */
	while ((rep = bt_hogp_rep_next(hogp_ctx, rep)) != NULL) {
		uint8_t rep_id = bt_hogp_rep_id(rep);
		enum bt_hids_report_type rep_type = bt_hogp_rep_type(rep);

		LOG_INF("Report: id=%u, type=%u", rep_id, rep_type);

		/* Subscribe to input reports only */
		if (rep_type == BT_HIDS_REPORT_TYPE_INPUT) {
			err = bt_hogp_rep_subscribe(hogp_ctx, rep, hogp_report_notify);
			if (err) {
				LOG_ERR("Failed to subscribe to report %u: %d", rep_id, err);
			} else {
				LOG_INF("Subscribed to input report %u", rep_id);
				subscribed_reports++;
			}
		}
	}

	if (subscribed_reports > 0) {
		LOG_INF("Subscribed to %u input reports", subscribed_reports);
		hogp_ready = true;
	} else {
		LOG_ERR("No input reports found to subscribe");
	}
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

	/* Use Report Protocol mode (default) - no need to switch to boot mode.
	 * Modern keyboards like ZMK only support Report Protocol which provides
	 * more features (consumer keys, etc.).
	 */

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

/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "hid_bridge.h"
#include "usb_hid.h"
#include "hogp_client.h"

LOG_MODULE_REGISTER(hid_bridge, LOG_LEVEL_INF);

/* NKRO report parameters (ZMK default NKRO: 15 bytes) */
#define NKRO_REPORT_LEN		15
#define NKRO_BITMAP_OFFSET	2 /* modifier(1) + reserved(1) */
#define NKRO_BITMAP_LEN		13 /* 13 bytes = 104 key bits */
#define BOOT_REPORT_LEN		8
#define BOOT_MAX_KEYS		6

/**
 * Convert a ZMK NKRO report (15 bytes) to boot keyboard format (8 bytes).
 */
static void nkro_to_boot(const uint8_t *nkro, uint8_t *boot)
{
	memset(boot, 0, BOOT_REPORT_LEN);

	/* Modifier byte is identical in both formats */
	boot[0] = nkro[0];
	/* boot[1] = 0 (reserved) */

	uint8_t key_idx = 0;

	for (int byte = 0; byte < NKRO_BITMAP_LEN && key_idx < BOOT_MAX_KEYS; byte++) {
		uint8_t bits = nkro[NKRO_BITMAP_OFFSET + byte];
		if (!bits) {
			continue;
		}
		for (int bit = 0; bit < 8 && key_idx < BOOT_MAX_KEYS; bit++) {
			if (bits & (1 << bit)) {
				uint8_t keycode = (byte * 8) + bit;
				/* Keycodes 0-3 are reserved/error in HID spec */
				if (keycode >= 4) {
					boot[2 + key_idx] = keycode;
					key_idx++;
				}
			}
		}
	}
}

/* LED for status indication - use built-in LED on XIAO */
static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

/* Statistics */
static uint32_t reports_received;
static uint32_t reports_forwarded;
static uint32_t reports_dropped;

/* Last report for deduplication (optional) */
static uint8_t last_report[8];

/* Work queue for LED blink on activity */
static struct k_work_delayable led_off_work;

static void led_off_handler(struct k_work *work)
{
	if (led.port) {
		gpio_pin_set_dt(&led, 0);
	}
}

static void led_blink(void)
{
	if (led.port) {
		gpio_pin_set_dt(&led, 1);
		k_work_reschedule(&led_off_work, K_MSEC(10));
	}
}

int hid_bridge_init(void)
{
	int err;

	/* Initialize LED */
	if (led.port && device_is_ready(led.port)) {
		err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
		if (err) {
			LOG_WRN("Failed to configure LED: %d", err);
		} else {
			LOG_INF("Status LED configured");
		}
	}

	k_work_init_delayable(&led_off_work, led_off_handler);

	/* Initialize HOGP client with our report callback */
	err = hogp_client_init(hid_bridge_handle_report);
	if (err) {
		LOG_ERR("Failed to init HOGP client: %d", err);
		return err;
	}

	LOG_INF("HID bridge initialized");
	return 0;
}

void hid_bridge_handle_report(const uint8_t *report, uint8_t len)
{
	int err;
	uint8_t usb_report[BOOT_REPORT_LEN] = {0};

	reports_received++;

	if (len == NKRO_REPORT_LEN) {
		/* ZMK NKRO report - convert bitmap to 6KRO boot format */
		nkro_to_boot(report, usb_report);
	} else if (len == BOOT_REPORT_LEN) {
		/* Standard boot report - use as-is */
		memcpy(usb_report, report, BOOT_REPORT_LEN);
	} else {
		LOG_WRN("Unexpected report length: %u", len);
		/* Best effort: copy what fits */
		memcpy(usb_report, report, len < BOOT_REPORT_LEN ? len : BOOT_REPORT_LEN);
	}

	/* Log the report for debugging */
	LOG_DBG("BLE report: %02x %02x %02x %02x %02x %02x %02x %02x",
		report[0], len > 1 ? report[1] : 0,
		len > 2 ? report[2] : 0, len > 3 ? report[3] : 0,
		len > 4 ? report[4] : 0, len > 5 ? report[5] : 0,
		len > 6 ? report[6] : 0, len > 7 ? report[7] : 0);

	/* Check if USB is ready */
	if (!app_usb_hid_ready()) {
		reports_dropped++;
		if (reports_dropped % 100 == 1) {
			LOG_WRN("USB not ready, reports dropped: %u", reports_dropped);
		}
		return;
	}

	err = app_usb_hid_send_report(usb_report);
	if (err) {
		reports_dropped++;
		LOG_DBG("Failed to send USB report: %d", err);
		return;
	}

	reports_forwarded++;

	/* Blink LED on activity */
	led_blink();

	/* Store for potential deduplication */
	memcpy(last_report, usb_report, 8);

	/* Periodic stats logging */
	if (reports_forwarded % 1000 == 0) {
		LOG_INF("Stats: received=%u, forwarded=%u, dropped=%u",
			reports_received, reports_forwarded, reports_dropped);
	}
}

void hid_bridge_on_disconnect(void)
{
	LOG_INF("BLE disconnected, releasing all keys");

	/* Release all keys to prevent stuck keys */
	app_usb_hid_release_all();

	/* Clear last report */
	memset(last_report, 0, sizeof(last_report));
}

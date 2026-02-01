/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/logging/log.h>

#include "hid_bridge.h"
#include "usb_hid.h"
#include "hogp_client.h"

LOG_MODULE_REGISTER(hid_bridge, LOG_LEVEL_INF);

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

	reports_received++;

	/* Validate report length - boot keyboard is 8 bytes */
	if (len != 8) {
		LOG_WRN("Unexpected report length: %u (expected 8)", len);
		/* Try to handle anyway if it's smaller */
		if (len > 8) {
			len = 8;
		}
	}

	/* Log the report for debugging */
	LOG_DBG("BLE report: %02x %02x %02x %02x %02x %02x %02x %02x",
		report[0], len > 1 ? report[1] : 0,
		len > 2 ? report[2] : 0, len > 3 ? report[3] : 0,
		len > 4 ? report[4] : 0, len > 5 ? report[5] : 0,
		len > 6 ? report[6] : 0, len > 7 ? report[7] : 0);

	/* Check if USB is ready */
	if (!usb_hid_ready()) {
		reports_dropped++;
		if (reports_dropped % 100 == 1) {
			LOG_WRN("USB not ready, reports dropped: %u", reports_dropped);
		}
		return;
	}

	/* Forward to USB with proper 8-byte buffer */
	uint8_t usb_report[8] = {0};
	memcpy(usb_report, report, len < 8 ? len : 8);

	err = usb_hid_send_report(usb_report);
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
	usb_hid_release_all();

	/* Clear last report */
	memset(last_report, 0, sizeof(last_report));
}

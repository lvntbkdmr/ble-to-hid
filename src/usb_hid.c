/* SPDX-License-Identifier: Apache-2.0 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/usb/class/usb_hid.h>
#include <zephyr/logging/log.h>

#include "usb_hid.h"

LOG_MODULE_REGISTER(app_usb_hid, LOG_LEVEL_INF);

/* Boot keyboard HID report descriptor */
static const uint8_t hid_report_desc[] = {
	/* Usage Page (Generic Desktop) */
	0x05, 0x01,
	/* Usage (Keyboard) */
	0x09, 0x06,
	/* Collection (Application) */
	0xA1, 0x01,

	/* Modifier keys (8 bits) */
	/* Usage Page (Key Codes) */
	0x05, 0x07,
	/* Usage Minimum (Left Control) */
	0x19, 0xE0,
	/* Usage Maximum (Right GUI) */
	0x29, 0xE7,
	/* Logical Minimum (0) */
	0x15, 0x00,
	/* Logical Maximum (1) */
	0x25, 0x01,
	/* Report Size (1) */
	0x75, 0x01,
	/* Report Count (8) */
	0x95, 0x08,
	/* Input (Data, Variable, Absolute) */
	0x81, 0x02,

	/* Reserved byte */
	/* Report Count (1) */
	0x95, 0x01,
	/* Report Size (8) */
	0x75, 0x08,
	/* Input (Constant) */
	0x81, 0x01,

	/* LED output report (for Caps/Num/Scroll Lock LEDs) */
	/* Usage Page (LEDs) */
	0x05, 0x08,
	/* Usage Minimum (Num Lock) */
	0x19, 0x01,
	/* Usage Maximum (Scroll Lock) */
	0x29, 0x03,
	/* Report Count (3) */
	0x95, 0x03,
	/* Report Size (1) */
	0x75, 0x01,
	/* Output (Data, Variable, Absolute) */
	0x91, 0x02,
	/* Report Count (1) */
	0x95, 0x01,
	/* Report Size (5) */
	0x75, 0x05,
	/* Output (Constant) */
	0x91, 0x01,

	/* Key array (6 keys) */
	/* Usage Page (Key Codes) */
	0x05, 0x07,
	/* Usage Minimum (0) */
	0x19, 0x00,
	/* Usage Maximum (101) */
	0x29, 0x65,
	/* Logical Minimum (0) */
	0x15, 0x00,
	/* Logical Maximum (101) */
	0x25, 0x65,
	/* Report Count (6) */
	0x95, 0x06,
	/* Report Size (8) */
	0x75, 0x08,
	/* Input (Data, Array) */
	0x81, 0x00,

	/* End Collection */
	0xC0
};

static const struct device *hid_dev;
static bool usb_configured;
static K_SEM_DEFINE(hid_sem, 1, 1);

static void int_in_ready_cb(const struct device *dev)
{
	ARG_UNUSED(dev);
	k_sem_give(&hid_sem);
}

static void status_cb(enum usb_dc_status_code status, const uint8_t *param)
{
	ARG_UNUSED(param);

	switch (status) {
	case USB_DC_CONFIGURED:
		LOG_INF("USB configured");
		usb_configured = true;
		break;
	case USB_DC_DISCONNECTED:
		LOG_INF("USB disconnected");
		usb_configured = false;
		break;
	case USB_DC_SUSPEND:
		LOG_INF("USB suspended");
		break;
	case USB_DC_RESUME:
		LOG_INF("USB resumed");
		break;
	default:
		break;
	}
}

static const struct hid_ops hid_ops = {
	.int_in_ready = int_in_ready_cb,
};

int app_usb_hid_init(void)
{
	int ret;

	hid_dev = device_get_binding("HID_0");
	if (hid_dev == NULL) {
		LOG_ERR("Cannot get HID device binding");
		return -ENODEV;
	}

	usb_hid_register_device(hid_dev, hid_report_desc,
				sizeof(hid_report_desc), &hid_ops);

	if (usb_hid_set_proto_code(hid_dev, HID_BOOT_IFACE_CODE_KEYBOARD)) {
		LOG_WRN("Failed to set Protocol Code");
	}

	ret = usb_hid_init(hid_dev);
	if (ret != 0) {
		LOG_ERR("Failed to init HID device: %d", ret);
		return ret;
	}

	ret = usb_enable(status_cb);
	if (ret != 0) {
		LOG_ERR("Failed to enable USB: %d", ret);
		return ret;
	}

	LOG_INF("USB HID keyboard initialized");
	return 0;
}

int app_usb_hid_send_report(const uint8_t *report)
{
	int ret;

	if (!usb_configured) {
		return -ENOTCONN;
	}

	/* Wait for previous report to complete */
	ret = k_sem_take(&hid_sem, K_MSEC(100));
	if (ret != 0) {
		LOG_WRN("HID report timeout");
		return ret;
	}

	ret = hid_int_ep_write(hid_dev, report, APP_USB_HID_REPORT_SIZE, NULL);
	if (ret != 0) {
		LOG_ERR("Failed to send HID report: %d", ret);
		k_sem_give(&hid_sem);
		return ret;
	}

	return 0;
}

int app_usb_hid_release_all(void)
{
	static const uint8_t empty_report[APP_USB_HID_REPORT_SIZE] = {0};

	LOG_DBG("Releasing all keys");
	return app_usb_hid_send_report(empty_report);
}

bool app_usb_hid_ready(void)
{
	return usb_configured;
}

/* SPDX-License-Identifier: Apache-2.0 */

/*
 * BLE-to-USB-HID Bridge for XIAO-nRF52840
 *
 * This firmware connects to a Corne wireless keyboard via Bluetooth
 * and presents itself as a USB HID keyboard to the host computer.
 * Designed for use with Deskhop KVM switch.
 *
 * Flow:
 * 1. USB initializes and enumerates as HID keyboard
 * 2. BLE scans for HID devices (Corne keyboard)
 * 3. Connects and pairs (passkey displayed on USB serial)
 * 4. Subscribes to boot keyboard reports
 * 5. Forwards all reports from BLE to USB
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/usb/usb_device.h>
#include <zephyr/logging/log.h>

#include "usb_hid.h"
#include "ble_central.h"
#include "hid_bridge.h"

LOG_MODULE_REGISTER(main, LOG_LEVEL_INF);

/* Status LED - blinks during scanning, solid when connected */
static const struct gpio_dt_spec status_led = GPIO_DT_SPEC_GET_OR(DT_ALIAS(led0), gpios, {0});

static void print_banner(void)
{
	printk("\n");
	printk("========================================\n");
	printk("  BLE-to-USB-HID Bridge\n");
	printk("  for XIAO-nRF52840\n");
	printk("========================================\n");
	printk("\n");
	printk("This device bridges a Bluetooth HID keyboard\n");
	printk("to USB for use with Deskhop KVM.\n");
	printk("\n");
	printk("Pairing passkeys will be displayed here.\n");
	printk("Connect with: screen /dev/tty.usbmodem*\n");
	printk("\n");
}

int main(void)
{
	int err;

	LOG_INF("BLE-to-USB-HID Bridge starting...");

	/* Initialize USB HID keyboard first (before USB is enabled) */
	LOG_INF("Initializing USB HID...");
	err = app_usb_hid_init();
	if (err) {
		LOG_ERR("USB HID init failed: %d", err);
		/* Continue anyway - BLE scanning might still work */
	}

	/* Wait for USB to enumerate and console to be ready */
	LOG_INF("Waiting for USB enumeration...");
	k_msleep(2000);

	print_banner();

	/* Initialize HID bridge (also initializes HOGP client) */
	LOG_INF("Initializing HID bridge...");
	err = hid_bridge_init();
	if (err) {
		LOG_ERR("HID bridge init failed: %d", err);
		return err;
	}

	/* Initialize BLE central */
	LOG_INF("Initializing BLE...");
	err = ble_central_init();
	if (err) {
		LOG_ERR("BLE init failed: %d", err);
		return err;
	}

	/* Start scanning for HID devices */
	LOG_INF("Starting BLE scan...");
	err = ble_central_start_scan();
	if (err) {
		LOG_ERR("Failed to start scanning: %d", err);
		return err;
	}

	printk("\n");
	printk("Scanning for Bluetooth HID devices...\n");
	printk("Make sure your keyboard is in pairing mode.\n");
	printk("\n");

	/* Main loop - status monitoring */
	bool was_connected = false;
	int blink_counter = 0;

	while (1) {
		bool connected = ble_central_is_connected();

		if (connected != was_connected) {
			if (connected) {
				LOG_INF("=== CONNECTED ===");
				printk("\nConnected to Bluetooth keyboard!\n\n");
				/* Solid LED when connected */
				if (status_led.port) {
					gpio_pin_set_dt(&status_led, 1);
				}
			} else {
				LOG_INF("=== DISCONNECTED ===");
				printk("\nDisconnected from Bluetooth keyboard.\n");
				printk("Scanning for devices...\n\n");
			}
			was_connected = connected;
		}

		/* Blink LED while scanning */
		if (!connected && status_led.port) {
			blink_counter++;
			if (blink_counter >= 10) {
				gpio_pin_toggle_dt(&status_led);
				blink_counter = 0;
			}
		}

		k_msleep(100);
	}

	return 0;
}

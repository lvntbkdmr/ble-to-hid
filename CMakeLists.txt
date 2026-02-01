# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(ble_to_hid)

target_sources(app PRIVATE
    src/main.c
    src/usb_hid.c
    src/ble_central.c
    src/hogp_client.c
    src/pairing.c
    src/hid_bridge.c
)

# BLE-to-USB-HID Bridge for XIAO-nRF52840

Firmware that connects to a Corne wireless keyboard via Bluetooth and presents itself as a USB HID keyboard for use with Deskhop KVM.

## Features

- BLE Central mode with HID over GATT Protocol (HOGP) client
- USB HID keyboard device (boot protocol, 8-byte reports)
- Passkey pairing support (displayed via USB serial console)
- Bond storage in flash for automatic reconnection
- Low latency: 7.5-15ms BLE interval, 1ms USB polling

## Prerequisites

### Install nRF Connect SDK

```bash
# macOS dependencies
brew install cmake ninja gperf python3 ccache qemu dtc wget
pip3 install west

# Initialize SDK workspace
mkdir ~/ncs && cd ~/ncs
west init -m https://github.com/nrfconnect/sdk-nrf --mr v2.6.0
west update
west zephyr-export

# Python dependencies
pip3 install -r zephyr/scripts/requirements.txt
pip3 install -r nrf/scripts/requirements.txt

# Zephyr SDK toolchain
cd ~
wget https://github.com/zephyrproject-rtos/sdk-ng/releases/download/v0.16.5/zephyr-sdk-0.16.5_macos-x86_64.tar.xz
tar xf zephyr-sdk-0.16.5_macos-x86_64.tar.xz
cd zephyr-sdk-0.16.5
./setup.sh

# Add to ~/.zshrc
export ZEPHYR_BASE=~/ncs/zephyr
```

## Build

```bash
cd ~/ncs
west build -b xiao_ble/nrf52840 /path/to/ble-to-hid
```

## Flash

1. Double-tap the reset button on XIAO-nRF52840 to enter bootloader mode
2. Copy `build/zephyr/zephyr.uf2` to the USB drive that appears

## Usage

1. Connect XIAO to your computer via USB
2. Open serial console: `screen /dev/tty.usbmodem* 115200`
3. Put your Corne keyboard in pairing mode
4. When prompted, enter the displayed 6-digit passkey on the Corne
5. Once paired, keystrokes from Corne will appear on the USB host

## LED Status

- Blinking: Scanning for BLE devices
- Solid: Connected to keyboard
- Brief flash: Keystroke forwarded

## Project Structure

```
ble-to-hid/
├── CMakeLists.txt        # Build configuration
├── prj.conf              # Kconfig settings
├── app.overlay           # Devicetree overlay
└── src/
    ├── main.c            # Entry point
    ├── usb_hid.c/h       # USB HID keyboard
    ├── ble_central.c/h   # BLE scanning/connection
    ├── hogp_client.c/h   # HID over GATT client
    ├── pairing.c/h       # Passkey authentication
    └── hid_bridge.c/h    # BLE->USB forwarding
```

## Troubleshooting

**Device not found**: Ensure Corne is in pairing mode and close enough.

**Pairing fails**: Clear bonds on both devices and try again.

**Keys stuck**: Disconnect and reconnect USB to reset HID state.

## References

- [nRF Connect SDK Central HIDS Sample](https://docs.nordicsemi.com/bundle/ncs-latest/page/nrf/samples/bluetooth/central_hids/README.html)
- [Zephyr USB HID Keyboard Sample](https://docs.zephyrproject.org/latest/samples/subsys/usb/hid-keyboard/README.html)

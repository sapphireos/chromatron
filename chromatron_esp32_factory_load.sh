#!/usr/bin/env bash

pushd src/bootloaders/loader_esp32
./load_esp32_bootloader.sh
popd
sapphiremake -p chromatron --load_esp32

echo "wait for boot..."
sleep 20

echo "set wifi"
chromatron --host usb keys set wifi_ssid $CHROMATRON_WIFI_SSID
chromatron --host usb keys set wifi_password $CHROMATRON_WIFI_PASS

echo "set board type"
chromatron --host usb keys set hw_board_type 2

chromatron --host usb reboot
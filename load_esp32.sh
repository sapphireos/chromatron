#!/usr/bin/env bash
./esptool.py --chip esp32 --baud 2000000 write_flash 0x10000 firmware.bin
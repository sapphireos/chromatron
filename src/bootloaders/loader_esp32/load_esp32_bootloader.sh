#!/usr/bin/env bash

./esptool.py --chip esp32 write_flash 0x8000 partition-table.espbin 0x1000 main.bin
#!/usr/bin/env bash


avrdude -p atmega128rfa1 -P usb -c jtag2slow -U flash:w:src/bootloaders/loader_atmega128rfa1/main.hex


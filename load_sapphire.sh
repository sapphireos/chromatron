#!/usr/bin/env bash


avrdude -p atmega128rfa1 -P usb -c jtag2slow -U flash:w:src/chromatron/loader_image.hex


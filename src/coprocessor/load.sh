#!/usr/bin/env bash

avrdude -p atxmega128a4u -P usb -c jtag2pdi -U flash:w:main.hex
#!/usr/bin/env bash

avrdude -p atxmega128a4u -P usb -c jtag2pdi -U flash:w:src/coprocessor/firmware.bin:r -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m
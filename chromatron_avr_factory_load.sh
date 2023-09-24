#!/usr/bin/env bash

# avrdude -p atxmega128a4u -P usb -c jtag2pdi -U boot:w:src/bootloaders/loader_xmega128a4u/loader.hex -U flash:w:src/legacy/coprocessor/main.hex -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m
avrdude -p atxmega128a4u -P usb -c jtag2pdi -U flash:w:src/legacy/coprocessor/main.hex


# previous:
# avrdude -p atxmega128a4u -P usb -c jtag2pdi -U boot:w:src/bootloaders/loader_xmega128a4u/loader.hex -U flash:w:src/chromatron/main.hex -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m




# avrdude -p atxmega128a4u -P usb -c jtag2pdi -U boot:w:src/loader_xmega128a4u/loader.hex -U flash:w:src/chromatron/firmware.bin:r -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m
# avrdude -p atxmega128a4u -P usb -c jtag2pdi -U boot:w:src/loader_xmega128a4u/loader.hex -U flash:w:src/chromatron/recovery.bin:r -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m
# avrdude -p atxmega128a4u -P usb -c dragon_pdi -e -U boot:w:src/bootloaders/loader_xmega128a4u/loader.hex -U flash:w:src/chromatron/firmware.bin:r -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m

# avrdude -p atxmega128a4u -P usb -c jtag2pdi -U flash:w:src/chromatron/firmware.bin:r -U fuse2:w:0xBE:m -U fuse5:w:0xE4:m
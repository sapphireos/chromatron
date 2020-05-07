#!/usr/bin/env bash

# linux
tools/0.10.0-11-20190118-1134/bin/openocd -f st_nucleo_h7.cfg -c "program src/bootloaders/loader_stm32h7/main.hex exit"
tools/0.10.0-11-20190118-1134/bin/openocd -f st_nucleo_h7.cfg -c "program src/chromatron2/main.hex exit"

# mac
# openocd -f st_nucleo_h7.cfg -c "program src/bootloaders/loader_stm32h7/main.hex exit"
# openocd -f st_nucleo_h7.cfg -c "program src/chromatron2/main.hex exit"
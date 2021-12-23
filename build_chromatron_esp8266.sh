#!/usr/bin/env bash

# ./build_coprocessor.sh

sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_battery -p lib_veml7700 -p lib_rfm95w -p lib_motion -p lib_sht40 -p lib_VL53L0X -p lib_ssd1306 -p lib_gfx -p lib_chromatron -p chromatron -c
sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_battery -p lib_veml7700 -p lib_rfm95w -p lib_motion -p lib_sht40 -p lib_VL53L0X -p lib_ssd1306 -p lib_gfx -p lib_chromatron -p chromatron -t chromatron_classic

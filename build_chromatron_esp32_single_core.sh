#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_motion -p lib_sht40 -p lib_ssd1306 -p lib_VL53L0X -p lib_gfx -p lib_chromatron -p chromatron -c
sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_motion -p lib_sht40 -p lib_ssd1306 -p lib_VL53L0X -p lib_gfx -p lib_chromatron -p chromatron -t esp32_single


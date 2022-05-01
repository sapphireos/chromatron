#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_gfx -p lib_chromatron -p sensor -c
sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_gfx -p lib_chromatron -p sensor -t esp32_single


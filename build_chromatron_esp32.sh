#!/usr/bin/env bash

# sapphiremake -p sapphire -p hal_esp32 -p chromatron_no_led -c
# sapphiremake -p sapphire -p hal_esp32 -p chromatron_no_led -t esp32


# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p chromatron -c
# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p chromatron -t esp32_single

sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_veml7700 -p lib_gfx -p lib_chromatron -p chromatron -c
sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_veml7700 -p lib_gfx -p lib_chromatron -p chromatron -t esp32


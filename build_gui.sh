#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p lib_gui -p lib_touchscreen -p lib_lvgl -c
sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p lib_gui -p lib_touchscreen -p lib_lvgl -t esp32


# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_gfx -p lib_chromatron -p lib_gui -p lib_touchscreen -p lib_lvgl -p powermaster_9001 -c
# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_rfm95w -p lib_veml7700 -p lib_gfx -p lib_chromatron -p lib_gui -p lib_touchscreen -p lib_lvgl -p powermaster_9001 -t esp32

# sapphiremake -p sapphire -p hal_esp32 -p chromatron_no_led -c
# sapphiremake -p sapphire -p hal_esp32 -p chromatron_no_led -t esp32

#!/usr/bin/env bash

# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p lib_veml7700 -p lib_ssd1306 -p lib_rfm95w -p lib_gui -p lib_touchscreen -p lib_lvgl -p touch_controller -c
# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p lib_veml7700 -p lib_ssd1306 -p lib_rfm95w -p lib_gui -p lib_touchscreen -p lib_lvgl -p touch_controller -t esp32

sapphiremake -p lib_touchscreen -p lib_lvgl -p lib_gui -p touch_controller -c
sapphiremake -p lib_touchscreen -p lib_lvgl -p lib_gui -p touch_controller -t esp32

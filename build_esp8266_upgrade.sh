#!/usr/bin/env bash

# ./build_coprocessor.sh

sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_chromatron -p lib_gfx -p lib_battery -p esp8266_upgrade_1mb -p esp8266_upgrade_4mb -c

# sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_chromatron -p lib_gfx -p lib_battery -t chromatron_classic_upgrade

# sapphiremake -p esp8266_upgrade_1mb -t chromatron_classic_upgrade
# sapphiremake -p esp8266_upgrade_4mb -t chromatron_classic_upgrade


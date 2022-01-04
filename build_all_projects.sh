#!/usr/bin/env bash

set -e
set -x

./build_chromatron_esp8266.sh
sapphiremake -p stairway

./build_chromatron_esp32_single_core.sh
sapphiremake -p controller -t esp32_single

./build_chromatron_esp32.sh

# sapphiremake -p chromatron_batt
# sapphiremake -p pyramid
# sapphiremake -p printer
# sapphiremake -p gps
# sapphiremake -p air_quality
# sapphiremake -p radiation_lightning

./build_powermaster.sh

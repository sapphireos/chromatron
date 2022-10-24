#!/usr/bin/env bash

set -e
set -x

./build_chromatron_esp8266.sh

pushd ../playground
sapphiremake -p lib_sharp_prox -p stairway
sapphiremake -p lib_gps -p lib_serial_led -p gps
sapphiremake -p lib_amg8833 -p chromatron_amg8833
popd

# ./build_chromatron_esp32_single_core.sh
# sapphiremake -p controller -t esp32_single

./build_chromatron_esp32.sh

pushd ../playground
sapphiremake -p lib_hx711 -p weight_sensor -t esp32
popd

# sapphiremake -p chromatron_batt
# sapphiremake -p pyramid
# sapphiremake -p printer
# sapphiremake -p gps
# sapphiremake -p air_quality
# sapphiremake -p radiation_lightning

# ./build_powermaster.sh

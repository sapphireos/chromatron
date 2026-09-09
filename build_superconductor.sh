#!/usr/bin/env bash

# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p superconductor -c
# sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p superconductor -t esp32

# ./build_chromatron_esp32.sh

sapphiremake -p superconductor -c
sapphiremake -p superconductor -t esp32

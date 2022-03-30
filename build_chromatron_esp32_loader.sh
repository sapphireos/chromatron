#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p loader_esp32 -c
sapphiremake -p sapphire -p hal_esp32 -p lib_battery -p lib_gfx -p lib_chromatron -p loader_esp32 -t esp32_loader --loader

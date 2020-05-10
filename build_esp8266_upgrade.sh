#!/usr/bin/env bash

./build_coprocessor.sh

sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p esp8266_upgrade -c
sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p esp8266_upgrade -t chromatron_classic_upgrade

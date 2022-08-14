#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_xmega128a4u -p lib_coprocessor -p coprocessor -c
sapphiremake -p sapphire -p hal_xmega128a4u -p lib_coprocessor -p coprocessor -t coprocessor

pushd src/hal/esp8266
python3 bin_to_c_array.py
popd



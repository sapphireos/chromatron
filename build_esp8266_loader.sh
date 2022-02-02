#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_battery -p loader_esp8266 -c
sapphiremake -p sapphire -p hal_esp8266 -p lib_coprocessor -p lib_battery -p loader_esp8266 -t esp8266_loader --loader

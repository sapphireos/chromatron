#!/bin/bash

pushd ../playground
sapphiremake -p lib_hx711 -p weight_sensor -t esp32
popd
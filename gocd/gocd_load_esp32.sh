#!/usr/bin/env bash

cd ..

source python/.chromatron_venv/bin/activate

# need to add arg to select com port

sapphiremake -p chromatron --load_esp32

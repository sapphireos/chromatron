#!/usr/bin/env bash

rm -rf .chromatron_venv

python3 -m venv .chromatron_venv
source .chromatron_venv/bin/activate

source install_all.sh

cd ..
sapphiremake --discover

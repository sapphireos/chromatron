#!/usr/bin/env bash

python make_fw_package.py
pushd ../playground

sapphiremake -p chromatron_batt
sapphiremake -p pyramid
sapphiremake -p printer
sapphiremake -p gps
sapphiremake -p air_quality
sapphiremake -p radiation_lightningsubsub

sapphiremake -p stairway

popd
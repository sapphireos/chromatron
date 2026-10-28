#!/usr/bin/env bash

# the sim target builds the application with ExeBuilder: it compiles the whole
# library closure itself, so only the application project needs to be named

sapphiremake -p chromatron -t sim -c
sapphiremake -p chromatron -t sim

# generate the firmware info blob that the sim HAL programs into the
# simulated flash at boot (same fields as the firmware builds)
python3 "$(dirname "$0")/tools/sim_fwinfo.py"



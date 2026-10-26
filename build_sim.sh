#!/usr/bin/env bash

# the sim target builds the application with ExeBuilder: it compiles the whole
# library closure itself, so only the application project needs to be named

sapphiremake -p chromatron -t sim -c
sapphiremake -p chromatron -t sim


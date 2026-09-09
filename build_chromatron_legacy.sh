#!/usr/bin/env bash

sapphiremake -p sapphire -p hal_xmega128a4u -p chromatron_no_led -c

./make_wifi.sh

sapphiremake -p sapphire -p hal_xmega128a4u -p chromatron_no_led -t chromatron_legacy

#!/usr/bin/env bash

pushd sapphire
python3 setup.py install
popd

pushd catbus
python3 setup.py install
popd

pushd chromatron
python3 setup.py install
popd

pushd elysianfields
python3 setup.py install
popd

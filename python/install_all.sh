#!/usr/bin/env bash

pushd sapphire
python3 setup.py install
rm -rf build
popd

pushd catbus
python3 setup.py install
rm -rf build
popd

pushd chromatron
python3 setup.py install
rm -rf build
popd

pushd elysianfields
python3 setup.py install
rm -rf build
popd

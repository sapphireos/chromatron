#!/usr/bin/env bash

pushd chromatron
# python3 setup.py install
pip3 install .
rm -rf build
popd

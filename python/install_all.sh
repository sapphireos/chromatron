#!/usr/bin/env bash

pushd chromatron
python3 setup.py install
rm -rf build
popd

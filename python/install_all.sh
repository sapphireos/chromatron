#!/usr/bin/env bash

pushd sapphire
python setup.py install
popd

pushd catbus
python setup.py install
popd

pushd chromatron
python setup.py install
popd

pushd elysianfields
python setup.py install
popd

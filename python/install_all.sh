#!/usr/bin/env bash

cd sapphire
python setup.py install

cd ../catbus
python setup.py install

cd ../chromatron
python setup.py install
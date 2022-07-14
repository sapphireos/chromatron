#!/bin/sh

# local tests:
PYTHONPATH=. python3 -m pytest -n auto --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html tests/compiler/* -m local -v

# device tests:
# PYTHONPATH=. python3 -m pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/test_device.py -v


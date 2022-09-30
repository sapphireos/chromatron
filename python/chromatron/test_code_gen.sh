#!/bin/sh

# local tests:
# PYTHONPATH=. python3 -m pytest -n auto --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html tests/compiler/* -m local -v

# PYTHONPATH=. python3 -m pytest tests/compiler/test_code_gen.py -m local -v


# PYTHONPATH=. python3 -m pytest -n auto tests/compiler/* -m local -v -x

# device tests:
PYTHONPATH=. python3 -m pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html tests/compiler/test_device.py -v -x


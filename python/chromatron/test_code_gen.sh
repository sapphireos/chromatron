#!/bin/sh

PYTHONPATH=. python3 -m pytest -n auto --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html tests/compiler/* -v
# PYTHONPATH=. python3 -m pytest -n auto --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/test_code_gen.py -v

# PYTHONPATH=. python3 -m pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/test_code_gen.py -v


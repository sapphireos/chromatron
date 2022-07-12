#!/bin/sh

# PYTHONPATH=. python3 -m pytest -n 12 --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/* -v
PYTHONPATH=. python3 -m pytest -n 12 --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/test_code_gen.py -v

#!/bin/sh

# PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen_basic.py -v
# PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen.py -v

PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen* -v

#!/bin/sh

PYTHONPATH=. python3 -m pytest -n 12 --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/compiler/* -v

# PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen_basic.py -v
# PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen.py -v

# PYTHONPATH=. pytest --cov=chromatron --cov-config=tests/.coveragerc --cov-report=html -s tests/test_code_gen_opt.py -v
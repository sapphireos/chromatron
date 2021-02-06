#!/bin/sh
PYTHONPATH=. pytest --cov=. --cov-report=html -s tests/test_catbus.py -v -x
# PYTHONPATH=. pytest --cov=. --cov-report=html -s tests/test_services.py -v -x
# PYTHONPATH=. pytest -s -x
# PYTHONPATH=. pytest --cov=. --cov-report=html -s -x
#!/bin/sh
# PYTHONPATH=. pytest --cov=. --cov-report=html -s tests/test_catbus.py -v
PYTHONPATH=. pytest --cov=. --cov-report=html -s tests/test_services.py -v
# PYTHONPATH=. pytest -s
# PYTHONPATH=. pytest --cov=. --cov-report=html -s
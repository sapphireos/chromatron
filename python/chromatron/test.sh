#!/bin/sh
# PYTHONPATH=. pytest --cov=. --cov-config=tests/.coveragerc --cov-report=html -s tests/test_catbus.py -v -x
PYTHONPATH=. pytest --cov=. --cov-config=tests/.coveragerc --cov-report=html -s tests/test_services.py -v -x
# PYTHONPATH=. pytest --cov=. --cov-config=tests/.coveragerc --cov-report=html -s tests/test_link.py -v -x
# PYTHONPATH=. pytest -s -x
# PYTHONPATH=. pytest --cov=. --cov-config=tests/.coveragerc --cov-report=html -s -x
# PYTHONPATH=. pytest --cov=. --cov-config=tests/.coveragerc --cov-report=html tests/test_fields.py -s -x 
#!/bin/bash

set -e

./fx4 tests -r

exit $?

# make clean
# ./build.sh

# for fname in tests; do
# 	echo "$fname"
# 	# ./fx4 "$fname" -r 
# done

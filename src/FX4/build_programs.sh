#!/bin/bash
# Compile all .fx4 files under programs/ with the FX4 compiler.
# Exits with the first non-zero compiler status, 0 if all succeed.

cd "$(dirname "$0")" || exit 1

if [ ! -x ./fx4 ]; then
    echo "fx4 not found or not executable. Run ./build.sh first." >&2
    exit 1
fi

fail=0
count=0

while IFS= read -r -d '' file; do
    count=$((count + 1))
    ./fx4 "$file" > /dev/null
    code=$?
    if [ $code -ne 0 ]; then
        echo "FAIL: $file (exit $code)" >&2
        fail=1
    fi
done < <(find programs -name '*.fx4' -print0)

echo "Compiled $count files"

exit $fail

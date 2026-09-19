#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
bin=${1:-$root/build/linux-x86_64/bin}
include_paused_js=${KRYON_INCLUDE_PAUSED_JS:-0}
work=$(mktemp -d /tmp/kryon-record-values.XXXXXX)
echo "Record test output: $work"
cd "$root"

"$bin/k2c" --strict --no-main --root . -o "$work/c" tests/fixtures/record_values.kry tests/fixtures/record_types.kry
cc -std=c99 -Wall -Werror -I"$work/c" -I"$root/include" tests/record_values_main.c \
    "$work/c/tests/fixtures/record_values.c" -lm -o "$work/check-c"
"$work/check-c"

"$bin/k2cpp" --strict --no-main --root . -o "$work/cpp" tests/fixtures/record_values.kry tests/fixtures/record_types.kry
c++ -std=c++11 -Wall -I"$work/cpp" -I"$root/include" tests/record_values_main.cpp \
    "$work/cpp/tests/fixtures/record_values.cpp" -o "$work/check-cpp"
"$work/check-cpp"

"$bin/k2go" --strict --no-main --pkg records --root . -o "$work/go" tests/fixtures/record_values.kry tests/fixtures/record_types.kry
cp tests/record_values_test.go "$work/go/record_values_test.go"
(cd "$work/go" && GO111MODULE=off go test)

if [ "$include_paused_js" = 1 ]; then
    "$bin/k2js" --strict --root . -o "$work/js" tests/fixtures/record_values.kry tests/fixtures/record_types.kry
    node tests/record_values_test.mjs "$work/js/tests/fixtures/record_values.js"
    echo "Record value semantics pass in C, C++, Go, and JavaScript"
else
    echo "Record value semantics pass in C, C++, and Go; JS paused"
fi
python3 tests/string_diagnostics_test.py "$bin"
python3 tests/record_initializer_diagnostics_test.py "$bin"
python3 tests/aggregate_types_test.py "$bin"

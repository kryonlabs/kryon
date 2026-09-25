#!/bin/sh
set -eu

root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$root/build/typeface-source-test
mkdir -p "$work/c"
"$root/../ziran/build/bin/zi2c" --no-main --root "$root/tests" \
    --module-path "$root/src/ui" -o "$work/c" \
    "$root/tests/typeface_source_behavior.zi"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$root/../ziran/include" -I"$work/c" \
    "$root/tests/typeface_source_link_test.c" "$work/c"/*.c \
    -o "$work/test"
env -u DISPLAY -u WAYLAND_DISPLAY "$work/test"
echo "Kryon typeface host contract test passed"

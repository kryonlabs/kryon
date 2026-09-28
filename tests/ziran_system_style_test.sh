#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

# Parsing the whole classic pack is more than the portable runner's step
# budget, so this runs as native code.
source=$repo/tests/system_style_behavior.zi
"$ziran" build --target=c --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/src/kss" \
    --module-path "$repo/../ziran/std" -o "$work/c" "$source"
"${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$work/c" \
    "$work/c"/*.c -o "$work/style-test"
"$work/style-test"

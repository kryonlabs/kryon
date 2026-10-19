#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$repo/build/tile-map-test
rm -rf "$work"
mkdir -p "$work"
"$ziran_root/build/bin/zi2c" --no-main --root "$repo/tests" --module-path "$ziran_root/std" \
    --module-path "$repo/src/ui" -o "$work/c" "$repo/tests/tile_map_behavior.zi"
includes="-I$ziran_root/include -I$work/c"
for header in $(find "$work/c" -name '*.h' | xargs -n1 dirname | sort -u); do
    includes="$includes -I$header"
done
# shellcheck disable=SC2086
"${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections $includes $(find "$work/c" -name '*.c') \
    -Wl,--gc-sections -lm -o "$work/test"
"$work/test"
printf '%s\n' 'TileMap fitting and cell lookup passed'

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
"$ziran" bundle --root "$repo/tests" --module-path "$ziran_root/std" --module-path "$repo/src/ui" \
    --entry tile_map_behavior:main -o "$work/geometry.zib" "$repo/tests/tile_map_behavior.zi"
test "$("$ziran" run "$work/geometry.zib" | tail -n 1)" = 0
printf '%s\n' 'TileMap pixel fitting, cell lookup and transformed image mesh passed natively and portably'

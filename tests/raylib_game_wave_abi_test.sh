#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

"${CC:-cc}" -std=c11 -DGENERATED -ffunction-sections \
    -Wl,--gc-sections \
    -I"$repo/../ziran/include" -I"$repo/build/ziran/game/c" \
    "$repo/tests/raylib_game_wave_abi.c" -o "$work/generated"
"${CC:-cc}" -std=c11 -I"$repo/vendor/raylib/src" \
    "$repo/tests/raylib_game_wave_abi.c" -o "$work/raylib"

test "$("$work/generated")" = "$("$work/raylib")"
echo "raylib Wave and Sound Ziran ABI matches raylib.h"

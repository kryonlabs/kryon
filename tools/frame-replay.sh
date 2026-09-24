#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if test "$#" -ne 4; then
    echo "usage: $0 app.zib pointer-module input.trace output.json" >&2
    exit 2
fi

make -C "$repo" build/ziran/libkryon_host.a
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$repo/build/ziran/c" -I"$repo/../ziran/include" \
    "$repo/tools/frame_replay.c" "$repo/build/ziran/libkryon_host.a" \
    "$repo/../ziran/build/libziran.a" \
    -o "$repo/build/ziran/frame-replay"
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$repo/build/ziran/frame-replay" "$@"

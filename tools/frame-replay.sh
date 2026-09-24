#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
if test "$#" -ne 4; then
    echo "usage: $0 app.zib pointer-module input.trace output.json" >&2
    exit 2
fi

make -C "$repo" ZIRAN_DIR="$(dirname "$ziran_include")" \
    ZIRAN_BUILD_DIR="$(dirname "$ziran_lib")" \
    build/ziran/libkryon_host.a
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror \
    -I"$repo/build/ziran/c" -I"$ziran_include" \
    "$repo/tools/frame_replay.c" "$repo/build/ziran/libkryon_host.a" \
    "$ziran_lib" \
    -o "$repo/build/ziran/frame-replay"
env -u DISPLAY -u WAYLAND_DISPLAY \
    "$repo/build/ziran/frame-replay" "$@"

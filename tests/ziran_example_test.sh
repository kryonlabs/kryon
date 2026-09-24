#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
if [ -n "${ZIRAN_BIN:-}" ]; then
    ziran_tools=$(dirname "$ZIRAN_BIN")
    env -u DISPLAY -u WAYLAND_DISPLAY make -C "$repo/examples" \
        ZI2ZIR_BIN="$ziran_tools/zi2zir" \
        ZI2ZIB_BIN="$ziran_tools/zi2zib" \
        ZIRAN_LIB="${ZIRAN_LIB:?ZIRAN_LIB is required with ZIRAN_BIN}" \
        ZIRAN_INCLUDE="${ZIRAN_INCLUDE:?ZIRAN_INCLUDE is required with ZIRAN_BIN}" test
else
    env -u DISPLAY -u WAYLAND_DISPLAY make -C "$repo/examples" test
fi

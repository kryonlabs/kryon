#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/event-wake-sdl"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
for target in c; do
    executable=--exe
    LDLIBS='-lSDL2 -lpthread' "$ziran" build --project --root "$repo/tests" \
        --target="$target" --entry event_wake_sdl_test:main $executable \
        -o "$work/$target" "$repo/tests/event_wake_sdl_test.zi"
    xvfb-run -a "$work/$target/event_wake_sdl_test" > "$work/$target.log"
    tail -1 "$work/$target.log"
done

#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/event-wake-sdl"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
for target in c cpp; do
    executable=--exe; if test "$target" = cpp; then executable=; fi
    LDLIBS='-lSDL2 -lpthread' "$ziran" build --project --root "$repo/tests" \
        --target="$target" --entry event_wake_sdl_test:main $executable \
        -o "$work/$target" "$repo/tests/event_wake_sdl_test.zi"
    if test "$target" = cpp; then
        printf '#include "event_wake_sdl_test.hpp"\nint main() { return event_wake_sdl_test_main(); }\n' > "$work/$target/entry.cpp"
        "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$work/$target"/*.cpp -lSDL2 -lpthread -o "$work/$target/event_wake_sdl_test"
    fi
    xvfb-run -a "$work/$target/event_wake_sdl_test" > "$work/$target.log"
    tail -1 "$work/$target.log"
done

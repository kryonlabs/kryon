#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-wait"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread"
for target in c; do
    executable=--exe
    LDLIBS="$libs" "$ziran" build --project --root "$repo/tests" \
        --target="$target" --entry desktop_wait_test:main $executable \
        -o "$work/$target" "$repo/tests/desktop_wait_test.zi"
    status=0
    xvfb-run -a "$work/$target/desktop_wait_test" > "$work/$target.log" 2>&1 || status=$?
    if test "$status" != 0; then
        cat "$work/$target.log"
        echo "desktop wait test ($target) exited with status $status" >&2
        exit "$status"
    fi
    tail -1 "$work/$target.log"
done

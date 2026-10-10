#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-wait"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread"
for target in c cpp; do
    executable=--exe; if test "$target" = cpp; then executable=; fi
    LDLIBS="$libs" "$ziran" build --project --root "$repo/tests" \
        --target="$target" --entry desktop_wait_test:main $executable \
        -o "$work/$target" "$repo/tests/desktop_wait_test.zi"
    if test "$target" = cpp; then
        printf '#include "desktop_wait_test.hpp"\nint main() { return desktop_wait_test_main(); }\n' > "$work/$target/entry.cpp"
        "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$work/$target"/*.cpp $libs \
            -o "$work/$target/desktop_wait_test"
    fi
    xvfb-run -a "$work/$target/desktop_wait_test" > "$work/$target.log"
    tail -1 "$work/$target.log"
done

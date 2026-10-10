#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/desktop-raster-retained"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
libs="$(pkg-config --libs sdl2 cairo freetype2 pangocairo fontconfig glib-2.0 x11) -lm -lpthread"
source=$repo/tests/desktop_raster_retained_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$repo/src/ui" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    if test "$form" = saved; then
        input=$work/ir/desktop_raster_retained_test.zir
        root=$work/ir
    fi
    for target in c cpp; do
        executable=--exe; if test "$target" = cpp; then executable=; fi
        output=$work/$target-$form
        LDLIBS="$libs" "$ziran" build --root "$root" --module-path "$root" \
            --module-path "$repo/src/backend" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
            --target="$target" --entry desktop_raster_retained_test:main $executable \
            -o "$output" "$input"
        if test "$target" = cpp; then
            printf '#include "desktop_raster_retained_test.hpp"\nint main() { return desktop_raster_retained_test_main(); }\n' > "$output/entry.cpp"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$output"/*.cpp $libs \
                -o "$output/desktop_raster_retained_test"
        fi
        xvfb-run -a "$output/desktop_raster_retained_test" > "$work/$target-$form.log"
        tail -1 "$work/$target-$form.log"
    done
done

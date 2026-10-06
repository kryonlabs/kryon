#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-sdl"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SDL_RENDER_DRIVER SDL_FRAMEBUFFER_ACCELERATION
export YUE_DESKTOP_RECOVERY=0
export SDL_VIDEODRIVER=x11
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$repo/tests/pixmap_sdl_test.zi"
for input in source saved; do
    source="$repo/tests/pixmap_sdl_test.zi"
    set -- --project --root "$repo/tests"
    if test "$input" = saved; then
        source="$work/ir/pixmap_sdl_test.zir"
        set -- --root "$work/ir" --module-path "$work/ir"
    fi
    for target in c cpp; do
        executable=--exe; if test "$target" = cpp; then executable=; fi
        LDLIBS='-lSDL2' "$ziran" build "$@" \
            --target="$target" --entry pixmap_sdl_test:main $executable \
            -o "$work/$input-$target" "$source"
        if test "$target" = cpp; then
            printf '#include "pixmap_sdl_test.hpp"\nint main() { return pixmap_sdl_test_main(); }\n' > "$work/$input-$target/entry.cpp"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$work/$input-$target"/*.cpp -lSDL2 -o "$work/$input-$target/pixmap_sdl_test"
        fi
        for mode in default empty-driver env-software env-driver hint-driver env-surface hint-surface; do
            driver=; surface=0
            case "$mode" in env-driver) driver=opengl;; env-software) driver=software;; env-surface) surface=1;; esac
            if test "$mode" = default || test "$mode" = hint-surface || test "$mode" = hint-driver; then
                env PIXMAP_SDL_TEST="$mode" xvfb-run -a "$work/$input-$target/pixmap_sdl_test"
            else
                env PIXMAP_SDL_TEST="$mode" SDL_RENDER_DRIVER="$driver" SDL_FRAMEBUFFER_ACCELERATION="$surface" \
                    xvfb-run -a "$work/$input-$target/pixmap_sdl_test"
            fi
        done
    done
done

#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
out="$repo/build/scratch/bitmap-raylib"
mkdir -p "$out"
for folder in home config data cache state runtime; do
    mkdir -p "$out/$folder"
    chmod 700 "$out/$folder"
done
unset DISPLAY WAYLAND_DISPLAY WAYLAND_SOCKET XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER ICEAUTHORITY
export YUE_DESKTOP_RECOVERY=0 LIBGL_ALWAYS_SOFTWARE=1 TMPDIR="$out"
for mode in serial threaded; do
    defines=
    if test "$mode" = threaded; then defines='--define KRYON_FRAME_THREADS'; fi
    # shellcheck disable=SC2086
    "$ziran" ir --project --root "$repo/tests" $defines -o "$out/$mode-ir" "$repo/tests/bitmap_raylib_test.zi"
    for form in source saved; do
        source="$repo/tests/bitmap_raylib_test.zi"
        if test "$form" = saved; then source="$out/$mode-ir/bitmap_raylib_test.zir"; fi
        # shellcheck disable=SC2086
        "$ziran" build --project --target=c --root "$repo/tests" $defines -o "$out/$mode-$form" "$source"
        # shellcheck disable=SC2046
        "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -I"$ziran_root/include" \
            -I"$out/$mode-$form" "$out/$mode-$form"/*.c \
            "$repo/build/raylib-ziran/libraylib.a" "$repo/build/webp-ziran/libkryonwebp.a" \
            "$repo/build/avif-ziran/avif/libavif.a" "$repo/build/avif-ziran/aom/libaom.a" \
            -Wl,--gc-sections $(pkg-config --libs sdl2 gl) -ldl -lpthread -lm -o "$out/$mode-$form/test"
        env HOME="$out/home" XDG_CONFIG_HOME="$out/config" XDG_DATA_HOME="$out/data" \
            XDG_CACHE_HOME="$out/cache" XDG_STATE_HOME="$out/state" XDG_RUNTIME_DIR="$out/runtime" \
            XDG_CONFIG_DIRS="$out/config" XDG_DATA_DIRS="$out/data" \
            timeout 45s xvfb-run -a -s '-screen 0 128x128x24 -nolisten tcp' "$out/$mode-$form/test"
        printf 'Managed bitmap native Raylib: %s %s passed\n' "$mode" "$form"
    done
done

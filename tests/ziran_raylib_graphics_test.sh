#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_raylib_graphics_test.zi

# All graphics effects are injected Zi providers; no display or GPU is used.
"$ziran" ir --project --root "$repo/tests" --module-path "$repo/tests" \
    --define RAYLIB_GRAPHICS_HOST -o "$work/ir" "$source"
for input in source saved; do
    module=$source
    root=$repo/tests
    if test "$input" = saved; then
        root=$work/ir
        module=$root/ziran_raylib_graphics_test.zir
    fi
    for target in c; do
        output=$work/$input-$target
        "$ziran" build --project --target="$target" --root "$root" --module-path "$root" \
            --define RAYLIB_GRAPHICS_HOST -o "$output" "$module"
            compiler=${CC:-cc}
            standard=c11
            extension=c
        "$compiler" -std="$standard" -O2 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$output" "$output"/*."$extension" \
            "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" \
            "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" \
            -ldl -lpthread -lm -o "$output/test"
        "$output/test"
    done
done
echo 'Raylib graphics recovery, texture identity, retained frames and eviction passed C source and saved IR'

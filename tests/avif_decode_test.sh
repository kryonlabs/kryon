#!/bin/sh
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$repo/tests/toolchain.sh"
. "$repo/tests/image_packages.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
out="$repo/build/scratch/avif-decode"
mkdir -p "$out/lib"
cp "$repo/build/avif-ziran/libkryonavif.so" "$out/lib/"
image_ziran ir --define AVIF_RAYLIB --root "$repo/tests" --module-path "$repo/src/backend" -o "$out/ir" "$repo/tests/avif_decode_test.zi"
for form in source saved; do
    input="$repo/tests/avif_decode_test.zi"
    if test "$form" = saved; then input="$out/ir/avif_decode_test.zir"; fi
    image_ziran build --target=c --define AVIF_RAYLIB --root "$repo/tests" --module-path "$repo/src/backend" -o "$out/$form" "$input"
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$ziran_root/include" -I"$out/$form" "$out/$form"/*.c \
        "$repo/build/raylib-ziran/libraylib.a" "$repo/build/webp-ziran/libkryonwebp.a" \
        "$repo/build/avif-ziran/avif/libavif.a" "$repo/build/avif-ziran/aom/libaom.a" \
        $(pkg-config --libs sdl2) -ldl -lpthread -lm -o "$out/$form/test"
    # Executable-relative discovery, no codec environment or cwd dependency.
    cp "$out/$form/test" "$out/test"
    (cd /; timeout 30s "$out/test" "$repo/tests/fixtures/avif")
done

#!/bin/sh
# WebP lossy, alpha, lossless and animated fixtures through the raylib/Android
# image decoder, from source and saved IR, linked with the kryonwebp archive.
# No display or GL context is used.
set -eu
repo=$(CDPATH='' cd -- "$(dirname -- "$0")/.." && pwd -P)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
raylib=${RAYLIB_A:-$repo/build/raylib-ziran/libraylib.a}
webp=${WEBP_A:-$repo/build/webp-ziran/libkryonwebp.a}
test -f "$raylib" || { echo "Set RAYLIB_A to the desktop raylib archive (make raylib-webp-decode-test)" >&2; exit 1; }
test -f "$webp" || { echo "Set WEBP_A to the kryonwebp archive (make raylib-webp-decode-test)" >&2; exit 1; }
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2) -lGL -ldl -lpthread -lm}
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/raylib-webp-decode.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/raylib_webp_decode_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/raylib_webp_decode_test.zir; fi
    output=$work/$form
    "$ziran" build --target=c --root "$repo/tests" --module-path "$repo/src/backend" \
        --module-path "$ziran_root/std" -o "$output" "$input"
    "${CC:-cc}" -std=c11 -O1 -ffunction-sections -fdata-sections -Wl,--gc-sections \
        -I"$ziran_root/include" -I"$output" "$output"/*.c "$raylib" "$webp" \
        $raylib_libs -lm -o "$output/test"
    timeout --kill-after=2s 20s "$output/test" "$repo/tests/fixtures/webp"
done

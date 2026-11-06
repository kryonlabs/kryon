#!/bin/sh
# Real glyph decoding, fallback, advances and disposal; no desktop or GL.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/raylib-typeface.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
if test -n "${RAYLIB_A:-}"; then archive=$RAYLIB_A
else
    raylib_source=${RAYLIB_SOURCE:-$("$ziran" pkg path raylib)}
    archive=$raylib_source/src/libraylib.a
fi
test -f "$archive"
raylib_libs=${RAYLIB_LIBS:-$(pkg-config --libs sdl2) -lGL -ldl -lpthread -lm}
source=$repo/tests/raylib_typeface_test.zi
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
cd "$repo"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/raylib_typeface_test.zir; fi
    for target in c; do
        output=$work/$form-$target
        "$ziran" build --project --target="$target" --root "$repo/tests" \
            -o "$output" "$input"
            compiler=${CC:-cc}; standard=c11; extension=c
        "$compiler" -std="$standard" -O1 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -Wl,--wrap=LoadFontEx -Wl,--wrap=LoadFontFromMemory \
            -Wl,--wrap=UnloadFont -Wl,--wrap=DrawTextEx -Wl,--wrap=rlDrawRenderBatchActive \
            -Wl,--wrap=SetTextureFilter -Wl,--wrap=rlGetMatrixModelview \
            -I"$output" "$output"/*."$extension" "$archive" \
            "${WEBP_A:-$repo/build/webp-ziran/libkryonwebp.a}" \
            "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" \
            "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" \
            $raylib_libs -o "$output/test"
        XDG_CACHE_HOME="$work/cache" timeout --kill-after=2s 20s "$output/test"
    done
done
echo 'Raylib real font coverage, CJK fallback, supplementary seeds, measurements and disposal passed C source and saved IR'

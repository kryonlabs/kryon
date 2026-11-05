#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/raylib-window.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source=$repo/tests/raylib_window_test.zi
"$ziran" ir --project --root "$repo/tests" --module-path "$repo/tests" \
    -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    root=$repo/tests
    if test "$form" = saved; then root=$work/ir; input=$root/raylib_window_test.zir; fi
    for target in c; do
        output=$work/$form-$target
        "$ziran" build --project --target="$target" --root "$root" \
            --module-path "$root" -o "$output" "$input"
            compiler=${CC:-cc}; standard=c11; extension=c
        "$compiler" -std="$standard" -O1 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$output" "$output"/*."$extension" \
            "${WEBP_A:-$repo/build/webp-ziran/libkryonwebp.a}" \
            "${AVIF_A:-$repo/build/avif-ziran/avif/libavif.a}" \
            "${AOM_A:-$repo/build/avif-ziran/aom/libaom.a}" \
            -ldl -lpthread -lm -o "$output/test"
        # The default title is the one kept beside the program.
        printf '%s\n' 'Probe Window' > "$output/test.title"
        timeout --kill-after=2s 10s "$output/test"
    done
done
echo 'Raylib windows: launch properties, titles, focus, middle clicks and repeats passed C source and saved IR'

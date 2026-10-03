#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch"
work=$(mktemp -d "$repo/build/scratch/raylib-window.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY
source=$repo/tests/raylib_window_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    input=$source
    if test "$form" = saved; then input=$work/ir/raylib_window_test.zir; fi
    for target in c cpp; do
        output=$work/$form-$target
        "$ziran" build --target="$target" --root "$repo/tests" \
            --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
            --module-path "$ziran_root/std" -o "$output" "$input"
        if test "$target" = c; then compiler=${CC:-cc}; standard=c11; extension=c
        else compiler=${CXX:-c++}; standard=c++17; extension=cpp; fi
        "$compiler" -std="$standard" -O1 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$output" "$output"/*."$extension" -lm -o "$output/test"
        timeout --kill-after=2s 10s "$output/test"
    done
done
echo 'Raylib windows: launch properties, titles, focus, middle clicks and repeats passed C/C++ source and saved IR'

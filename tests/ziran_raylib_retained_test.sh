#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_raylib_retained_test.zi

# Ziran providers export the native Raylib symbols. No graphics library or
# display is used: these tests inject allocation failures at the FFI boundary.
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$repo/src/backend" --module-path "$ziran_root/std" \
    -o "$work/ir" "$source"
for input in source saved; do
    module=$source
    root=$repo/tests
    if test "$input" = saved; then
        root=$work/ir
        module=$root/ziran_raylib_retained_test.zir
    fi
    for target in c cpp; do
        output=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" \
            --module-path "$repo/src/ui" --module-path "$repo/src/backend" \
            --module-path "$ziran_root/std" -o "$output" "$module"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -O2 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$output" "$output"/*.c -lm -o "$output/test"
        else
            "${CXX:-c++}" -std=c++17 -O2 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$output" "$output"/*.cpp -lm -o "$output/test"
        fi
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/test"
    done
done
echo 'Retained frames: failed allocations, retries, reuse and resize passed'

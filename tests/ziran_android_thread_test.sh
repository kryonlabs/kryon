#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_android_thread_test.zi
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/backend" \
    -o "$work/ir" "$source"
for input in source saved; do
    root=$repo/tests
    module=$source
    if test "$input" = saved; then
        root=$work/ir
        module=$root/ziran_android_thread_test.zir
    fi
    for target in c cpp; do
        output=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" \
            --module-path "$repo/src/backend" -o "$output" "$module"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -O2 -pthread -Wl,--export-dynamic \
                -I"$output" "$output"/*.c -ldl -o "$output/test"
        else
            "${CXX:-c++}" -std=c++17 -O2 -pthread -Wl,--export-dynamic \
                -I"$output" "$output"/*.cpp -ldl -o "$output/test"
        fi
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/test"
    done
done
echo 'Android native thread: default stack and explicit attributes passed'

#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
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
    for target in c; do
        output=$work/$input-$target
        "$ziran" build --target="$target" --root "$root" \
            --module-path "$repo/src/backend" -o "$output" "$module"
            "${CC:-cc}" -std=c11 -O2 -pthread -Wl,--export-dynamic \
                -I"$output" "$output"/*.c -ldl -o "$output/test"
        env -u DISPLAY -u WAYLAND_DISPLAY "$output/test"
    done
done
echo 'Android native thread: default stack and explicit attributes passed'

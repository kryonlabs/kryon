#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/ziran_accessibility_behavior.zi
entry=ziran_accessibility_behavior:main

"$ziran" check --root "$repo/tests" $kryon_ui_args "$source"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" $kryon_ui_args \
    --entry "$entry" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry "$entry" -o "$work/saved.zx" \
    "$work/ir/ziran_accessibility_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir="$kryon_ui_args"
    else
        module=$work/ir/ziran_accessibility_behavior.zir
        module_root=$work/ir
        module_dir="--module-path $work/ir"
    fi
    for target in c go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry "$entry" \
                --root "$module_root" $module_dir \
                -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off \
                go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry "$entry" \
                --root "$module_root" $module_dir \
                -o "$output" "$module"
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done

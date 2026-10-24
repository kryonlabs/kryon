#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/dpi_test.zi

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry dpi_test:Answer \
    -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry dpi_test:Answer -o "$work/saved.zx" "$work/ir/dpi_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 42
test "$("$ziran" run "$work/saved.zx")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/dpi_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe \
                --entry dpi_test:main --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
                "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                    "$output"/*.c -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done

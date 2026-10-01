#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/canvas_props_test.zi

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --entry canvas_props_test:Answer \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry canvas_props_test:Answer -o "$work/saved.zib" \
    "$work/ir/canvas_props_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/canvas_props_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe \
                --entry canvas_props_test:main --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$module_root" \
                --module-path "$module_dir" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            if test "$target" = c; then
                "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                    "$output"/*.c -o "$output/app"
            else
                "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" \
                    -I"$output" "$output"/*.cpp -o "$output/app"
            fi
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done

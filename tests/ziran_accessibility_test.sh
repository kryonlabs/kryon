#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_accessibility_behavior.zi
entry=ziran_accessibility_behavior:main

"$ziran" check --root "$repo/tests" --module-path "$repo/src/ui" "$source"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --entry "$entry" -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry "$entry" -o "$work/saved.zib" \
    "$work/ir/ziran_accessibility_behavior.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_accessibility_behavior.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry "$entry" \
                --root "$module_root" --module-path "$module_dir" \
                -o "$output" "$module"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off \
                go run "$output"/*.go
        elif test "$target" = c; then
            "$ziran" build --target=c --entry "$entry" \
                --root "$module_root" --module-path "$module_dir" \
                -o "$output" "$module"
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        else
            "$ziran" build --target=cpp --entry "$entry" \
                --root "$module_root" --module-path "$module_dir" \
                -o "$output" "$module"
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done

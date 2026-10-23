#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/color_picker_widget_behavior.zi
portable=$repo/tests/color_picker_widget_portable_test.zi
widget_host=color_picker_widget_host
widget_mesh_host=ziran_mesh_noop_host
set --
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
    "$@" --entry color_picker_widget_portable_test:main \
    -o "$work/source.zx" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry color_picker_widget_portable_test:main \
    -o "$work/saved.zx" "$work/ir/color_picker_widget_portable_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for target in c cpp; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
        -o "$output" "$portable"
    if test "$target" = c; then
        "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$ziran_root/include" -I"$output" \
            "$output"/*.c -o "$output/app"
    else
        "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$ziran_root/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
done

for input in source saved; do
    output=$work/go-$input
    if test "$input" = source; then
        module=$portable
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/color_picker_widget_portable_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    "$ziran" build --target=go --pkg main --exe \
        --entry color_picker_widget_portable_test:main --root "$module_root" \
        --module-path "$module_dir" --module-path "$ziran_root/std" \
        "$@" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done

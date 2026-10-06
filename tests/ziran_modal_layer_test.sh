#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_modal_layer_test.zi
set --
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    module=$source; root=$repo/tests; modules=$repo/src/ui
    if test "$form" = saved; then module=$work/ir/ziran_modal_layer_test.zir; root=$work/ir; modules=$work/ir; fi
    for target in c cpp go py rust; do
        output=$work/$form-$target
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe "$@" \
                --entry ziran_modal_layer_test:main --root "$root" --module-path "$modules" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            GO111MODULE=off go run "$output"/*.go
        elif test "$target" = py || test "$target" = rust; then
            "$ziran" build --target="$target" --exe "$@" \
                --entry ziran_modal_layer_test:main --root "$root" --module-path "$modules" \
                --module-path "$ziran_root/std" -o "$output" "$module"
            if test "$target" = py; then python3 "$output"
            else cargo run --offline --quiet --manifest-path "$output/Cargo.toml"; fi
        else
            "$ziran" build --target="$target" --no-main --entry ziran_modal_layer_test:main \
                --root "$root" --module-path "$modules" --module-path "$ziran_root/std" -o "$output" "$module"
            if test "$target" = cpp; then compiler=${CXX:-c++}; standard=c++17; suffix=cpp
            else compiler=${CC:-cc}; standard=c11; suffix=c; fi
            "$compiler" -O2 -std="$standard" -ffunction-sections -fdata-sections -I"$output" \
                "$output"/*."$suffix" -Wl,--gc-sections -o "$output/run"
            "$output/run"
        fi
    done
    "$ziran" bundle --root "$root" --module-path "$modules" --module-path "$ziran_root/std" "$@" \
        --entry ziran_modal_layer_test:main -o "$work/$form.zib" "$module"
    test "$("$ziran" run "$work/$form.zib")" = 0
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Modal input transactions passed native and portable execution'

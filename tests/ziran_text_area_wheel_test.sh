#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_text_area_wheel_test.zi
widget_host=ziran_composition_widget_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind image_raster:ImageWidth=ziran_text_area_wheel_test:ImageWidth \
    --bind image_raster:ImageHeight=ziran_text_area_wheel_test:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" --entry ziran_text_area_wheel_test:main \
    -o "$work/source.zib" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_text_area_wheel_test:main -o "$work/saved.zib" \
    "$work/ir/ziran_text_area_wheel_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
for bundle in source saved; do
    result=$("$ziran" run "$work/$bundle.zib")
    printf '%s bundle: %s\n' "$bundle" "$result"
    test "$result" = 0
done
for target in c cpp go; do
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --exe \
            --entry ziran_text_area_wheel_test:main "$@" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
    else
        "$ziran" build --target="$target" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$work/$target" \
                "$work/$target"/*.c -o "$work/$target/ziran_text_area_wheel_test"
        else
            "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$work/$target" \
                "$work/$target"/*.cpp -o "$work/$target/ziran_text_area_wheel_test"
        fi
    fi
    if test "$target" = go; then
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            GO111MODULE=off go run "$work/$target"/*.go
    else
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            "$work/$target/ziran_text_area_wheel_test"
    fi
    printf '%s text area wheel: passed\n' "$target"
done

#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
source=$repo/tests/text_touch_behavior.zi
widget_host=ziran_context_menu_widget_host
widget_mesh_host=ziran_context_menu_widget_host
set -- \
    --bind image_raster:ImageWidth=text_touch_behavior:ImageWidth \
    --bind image_raster:ImageHeight=text_touch_behavior:ImageHeight \
    --bind system_clipboard:SystemClipboardOpenHost=ziran_system_clipboard_host:SystemClipboardOpenHost \
    --bind system_clipboard:SystemClipboardByteHost=ziran_system_clipboard_host:SystemClipboardByteHost \
    --bind system_clipboard:SystemClipboardWriteHost=ziran_system_clipboard_host:SystemClipboardWriteHost
. "$repo/tests/widget_raster_bindings.sh"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" --entry text_touch_behavior:main \
    -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry text_touch_behavior:main -o "$work/saved.zx" \
    "$work/ir/text_touch_behavior.zir"
cmp "$work/source.zx" "$work/saved.zx"
for bundle in source saved; do
    result=$("$ziran" run "$work/$bundle.zx")
    printf '%s bundle: %s\n' "$bundle" "$result"
    test "$result" = 0
done
for target in c go; do
    if test "$target" = go; then
        "$ziran" build --target=go --pkg main --exe \
            --entry text_touch_behavior:main "$@" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
    else
        "$ziran" build --target="$target" \
            --root "$repo/tests" --module-path "$repo/src/ui" \
            --module-path "$ziran_root/std" -o "$work/$target" "$source"
            "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
                -Wl,--gc-sections -I"$ziran_root/include" -I"$work/$target" \
                "$work/$target"/*.c -o "$work/$target/text_touch_behavior"
    fi
    if test "$target" = go; then
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            GO111MODULE=off go run "$work/$target"/*.go
    else
        env -u DISPLAY -u WAYLAND_DISPLAY -u XAUTHORITY -u DBUS_SESSION_BUS_ADDRESS \
            "$work/$target/text_touch_behavior"
    fi
    printf '%s touch selection: passed\n' "$target"
done

#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
set -- --bind text_widget:TextHostedHost=hosted_ui_provider:Text \
    --bind image_widget:ImageHostedHost=hosted_ui_provider:Image \
    --bind button_widget:ButtonHostedHost=hosted_ui_provider:Button \
    --bind text_area_widget:TextAreaHostedHost=hosted_ui_provider:TextArea \
    --bind scroll_widget:ScrollHostedHost=hosted_ui_provider:Scroll \
    --bind tree:EndHostedHost=hosted_ui_provider:End \
    --bind checkbox_widget:CheckboxHostedHost=hosted_ui_provider:Checkbox \
    --bind toggle_widget:ToggleHostedHost=hosted_ui_provider:Toggle
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" --define KRYON_HOSTED_UI "$@" \
    --entry hosted_ui:main -o "$work/ui.zib" \
    "$repo/tests/hosted_ui.zi" "$repo/tests/hosted_ui_provider.zi"
test "$("$ziran" run "$work/ui.zib")" = 42
printf '%s\n' 'Hosted portable Text, Image, Button, TextArea and Scroll contracts passed'
for target in c cpp; do
    executable=
    if test "$target" = c; then executable=--exe; fi
    "$ziran" build --target="$target" $executable --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
        --define KRYON_HOSTED_UI --entry hosted_ui:main \
        -o "$work/$target" "$repo/tests/hosted_ui.zi" "$repo/tests/hosted_ui_provider.zi"
    binary=$work/$target/hosted_ui
    if test "$target" = cpp; then
        binary=$work/cpp/test
        "${CXX:-c++}" -std=c++17 -I"$work/cpp" "$work/cpp"/*.cpp -lm -o "$binary"
    fi
    status=0
    "$binary" || status=$?
    test "$status" = 42
done
printf '%s\n' 'Hosted widget contracts also passed native C and C++'

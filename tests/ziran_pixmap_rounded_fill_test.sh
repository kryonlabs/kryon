#!/bin/sh
. "$(dirname -- "$0")/test_context.sh"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$repo/tests/pixmap_rounded_fill_test.zi"
for form in source saved; do
    if test "$form" = source; then
        "$ziran" build --project --root "$repo/tests" --target=c --exe --entry pixmap_rounded_fill_test:main -o "$work/$form" "$repo/tests/pixmap_rounded_fill_test.zi"
    else
        "$ziran" build --root "$work/ir" --target=c --exe --entry pixmap_rounded_fill_test:main -o "$work/$form" "$work/ir/pixmap_rounded_fill_test.zir"
    fi
    "$work/$form/pixmap_rounded_fill_test"
done

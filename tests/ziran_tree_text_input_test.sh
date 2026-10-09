#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/tree-text-input"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source="$repo/tests/tree_text_input_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    module=$source; root="$repo/tests"; modules="$repo/src/ui"
    if test "$form" = saved; then
        module="$work/ir/tree_text_input_test.zir"; root="$work/ir"; modules="$work/ir"
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" \
        --module-path "$ziran_root/std" --entry tree_text_input_test:main \
        -o "$work/$form.zib" "$module"
    test "$("$ziran" run "$work/$form.zib")" = 0
    "$ziran" build --root "$root" --module-path "$modules" \
        --module-path "$ziran_root/std" --entry tree_text_input_test:main \
        --target=c --exe -o "$work/$form-c" "$module"
    "$work/$form-c/tree_text_input_test"
done

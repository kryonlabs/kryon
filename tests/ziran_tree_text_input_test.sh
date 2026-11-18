#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/tree-text-input"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
source="$repo/tests/tree_text_input_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for form in source saved; do
    module=$source; root="$repo/tests"; modules="$kryon_ui_args"
    if test "$form" = saved; then
        module="$work/ir/tree_text_input_test.zir"; root="$work/ir"; modules="--module-path $work/ir"
    fi
    "$ziran" bundle --root "$root" $modules \
        --module-path "$ziran_root/std" --entry tree_text_input_test:main \
        -o "$work/$form.zx" "$module"
    result=$("$ziran" run "$work/$form.zx")
    if test "$result" != 0; then
        echo "Text input failure in $form ZX: $result" >&2
        exit 1
    fi
    "$ziran" build --root "$root" $modules \
        --module-path "$ziran_root/std" --entry tree_text_input_test:main \
        --target=c --exe -o "$work/$form-c" "$module"
    "$work/$form-c/tree_text_input_test"
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Text input: UTF-8 queue, editing keys and committed focus passed'

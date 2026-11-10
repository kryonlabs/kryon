set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${TREE_STYLE_METADATA_BUILD:-"$repo/build/scratch/tree-style-metadata"}
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/tree_style_metadata_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module=$source; module_root=$repo/tests; module_dir="$kryon_ui_args"
    if test "$input" = saved; then
        module=$work/ir/tree_style_metadata_test.zir
        module_root=$work/ir; module_dir="--module-path $work/ir"
    fi
    "$ziran" bundle --root "$module_root" $module_dir \
    --module-path "$ziran_root/std" --entry tree_style_metadata_test:TreeStyleMetadataAnswer \
        -o "$work/$input.zx" "$module"
    result=$("$ziran" run "$work/$input.zx")
    if test "$(printf '%s\n' "$result" | tail -1)" != 0; then
        printf '%s portable tree style metadata check returned %s\n' \
            "$input" "$result" >&2
        exit 1
    fi
    "$ziran" build --root "$module_root" $module_dir \
        --module-path "$ziran_root/std" --entry tree_style_metadata_test:TreeStyleMetadataAnswer \
        --target=c --exe -o "$work/$input-c" "$module"
    "$work/$input-c/tree_style_metadata_test"
done
cmp "$work/source.zx" "$work/saved.zx"
printf 'Tree style metadata source, saved IR, ZX, and C checks passed\n'
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${TREE_STYLE_METADATA_BUILD:-"$repo/build/scratch/tree-style-metadata"}
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0
source="$repo/tests/tree_style_metadata_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module=$source; module_root=$repo/tests; module_dir="$kryon_ui_args"
    if test "$input" = saved; then
        module=$work/ir/tree_style_metadata_test.zir
        module_root=$work/ir; module_dir="--module-path $work/ir"
    fi
    "$ziran" bundle --root "$module_root" $module_dir \
        --module-path "$ziran_root/std" --entry tree_style_metadata_test:TreeStyleMetadataAnswer \
        -o "$work/$input.zx" "$module"
    result=$("$ziran" run "$work/$input.zx")
    if test "$(printf '%s\n' "$result" | tail -1)" != 0; then
        printf '%s portable tree style metadata check returned %s\n' \
            "$input" "$result" >&2
        exit 1
    fi
    "$ziran" build --root "$module_root" $module_dir \
        --module-path "$ziran_root/std" --entry tree_style_metadata_test:main \
        --target=c --exe -o "$work/$input-c" "$module"
    "$work/$input-c/tree_style_metadata_test"
done
cmp "$work/source.zx" "$work/saved.zx"
printf 'Tree style metadata source, saved IR, ZX, and C checks passed\n'

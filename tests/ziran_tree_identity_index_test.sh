#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${TREE_IDENTITY_INDEX_BUILD:-"$repo/build/scratch/tree-index/checks"}
mkdir -p "$work"
exec 9>"$work/check.lock"
flock -n 9 || { echo 'Tree identity checks are already running' >&2; exit 1; }
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export YUE_DESKTOP_RECOVERY=0 GOCACHE="$work/go-cache"
source="$repo/tests/tree_identity_index_test.zi"
"$ziran" ir --root "$repo/tests" $kryon_ui_args \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module=$source
    root=$repo/tests
    modules="$kryon_ui_args"
    if test "$input" = saved; then
        module=$work/ir/tree_identity_index_test.zir
        root=$work/ir
        modules="--module-path $work/ir"
    fi
    "$ziran" bundle --root "$root" $modules \
        --module-path "$ziran_root/std" --entry tree_identity_index_test:main \
        -o "$work/$input.zx" "$module"
    "$ziran" run "$work/$input.zx" > "$work/$input-vm.log"
    test "$(tail -1 "$work/$input-vm.log")" = 0
    for target in ${TREE_IDENTITY_INDEX_TARGETS:-c go}; do
        output="$work/$input-$target"
        executable=--exe
        package=
        if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --root "$root" $modules \
            --module-path "$ziran_root/std" --entry tree_identity_index_test:main \
            --target="$target" $executable $package -o "$output" "$module"
        case "$target" in
            go) GO111MODULE=off go run "$output"/*.go;;
            *) "$output/tree_identity_index_test";;
        esac
    done
done
cmp "$work/source.zx" "$work/saved.zx"
echo 'Tree identity source and saved checks passed on all selected targets'

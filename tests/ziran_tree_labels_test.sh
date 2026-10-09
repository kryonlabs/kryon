#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${TREE_LABELS_BUILD:-"$repo/build/scratch/tree-labels/maintained"}
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/tree_labels_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module=$source; module_root=$repo/tests; module_dir=$repo/src/ui
    if test "$input" = saved; then
        module=$work/ir/tree_labels_test.zir; module_root=$work/ir; module_dir=$work/ir
    fi
    "$ziran" bundle --root "$module_root" --module-path "$module_dir" \
        --module-path "$ziran_root/std" --entry tree_labels_test:main \
        -o "$work/$input.zib" "$module"
    result=$("$ziran" run "$work/$input.zib")
    if test "$(printf '%s\n' "$result" | tail -1)" != 0; then
        printf '%s portable tree label check returned %s\n' "$input" "$result" >&2
        exit 1
    fi
    for target in ${TREE_LABELS_TARGETS:-c cpp go rust py}; do
        output=$work/$input-$target
        executable=--exe; package=
        if test "$target" = cpp; then executable=; fi
        if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --root "$module_root" --module-path "$module_dir" \
            --module-path "$ziran_root/std" --entry tree_labels_test:main \
            --target="$target" $executable $package -o "$output" "$module"
        if test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$output"/*.cpp -lm \
                -o "$output/tree_labels_test"
        fi
        case "$target" in
            go) GO111MODULE=off go run "$output"/*.go;;
            py) python3 "$output/__main__.py";;
            rust) CARGO_TARGET_DIR="$work/rust-$input" cargo run --offline --quiet \
                --manifest-path "$output/Cargo.toml";;
            *) "$output/tree_labels_test";;
        esac
    done
done
cmp "$work/source.zib" "$work/saved.zib"
printf 'Tree label buffer source and saved checks passed\n'

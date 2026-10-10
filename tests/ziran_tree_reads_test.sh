#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=${TREE_READS_BUILD:-"$repo/build/scratch/tree-reads/checks"}
mkdir -p "$work"
exec 9>"$work/check.lock"
flock -n 9 || { echo 'Tree read checks are already running' >&2; exit 1; }
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0 GOCACHE="$work/go-cache"
source="$repo/tests/tree_reads_test.zi"
"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
for input in source saved; do
    module=$source
    root=$repo/tests
    modules=$repo/src/ui
    if test "$input" = saved; then
        module=$work/ir/tree_reads_test.zir
        root=$work/ir
        modules=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$modules" \
        --module-path "$ziran_root/std" --entry tree_reads_test:main \
        -o "$work/$input.zib" "$module"
    test "$("$ziran" run "$work/$input.zib")" = 0
    for target in ${TREE_READS_TARGETS:-c cpp go rust py}; do
        output="$work/$input-$target"
        executable=--exe
        package=
        if test "$target" = cpp; then executable=; fi
        if test "$target" = go; then package='--pkg main'; fi
        "$ziran" build --root "$root" --module-path "$modules" \
            --module-path "$ziran_root/std" --entry tree_reads_test:main \
            --target="$target" $executable $package -o "$output" "$module"
        if test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" "$output"/*.cpp \
                -lm -o "$output/tree_reads_test"
        fi
        case "$target" in
            go) GO111MODULE=off go run "$output"/*.go;;
            py) python3 "$output/__main__.py";;
            rust) CARGO_TARGET_DIR="$work/rust-$input" cargo run --offline --quiet \
                --manifest-path "$output/Cargo.toml";;
            *) "$output/tree_reads_test";;
        esac
    done
done
cmp "$work/source.zib" "$work/saved.zib"
echo 'Tree snapshots, modal layers and lifecycle passed on source and saved targets'

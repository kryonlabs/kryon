#!/bin/sh
set -eu
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
std="${include%/include}/std"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_frame_retained_test.zi

set -- \
    --bind raster:RasterLine=ziran_frame_retained_host:RasterLine \
    --bind raster_text:RasterText=ziran_frame_retained_host:RasterText \
    --bind raster_text:RasterTextClipped=ziran_frame_retained_host:RasterTextClipped \
    --bind paint_queue:RasterImage=ziran_frame_retained_host:RasterImage \
    --bind paint_queue:RasterImageMesh=ziran_mesh_noop_host:RasterImageMesh

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$std" "$@" \
    --entry ziran_frame_retained_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_frame_retained_test:main -o "$work/saved.zx" \
    "$work/ir/ziran_frame_retained_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_frame_retained_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    for target in c cpp rust py; do
        output=$work/$target-$input
        executable=--exe
        if test "$target" = cpp; then executable=; fi
        build_target() {
            "$ziran" build --target="$target" \
                --entry ziran_frame_retained_test:main $executable "$@" \
                --root "$module_root" --module-path "$module_dir" \
                --module-path "$std" -o "$output" "$module"
        }
        case "$target" in
            c|cpp) build_target;;
            *) build_target "$@";;
        esac
        if test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$include" "$output"/*.cpp \
                -o "$output/ziran_frame_retained_test"
        fi
        case "$target" in
            rust) CARGO_TARGET_DIR="$work/rust-$input" cargo run --offline --quiet \
                --manifest-path "$output/Cargo.toml";;
            py) python3 "$output/__main__.py";;
            *) "$output/ziran_frame_retained_test";;
        esac
    done
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_frame_retained_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$std" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done

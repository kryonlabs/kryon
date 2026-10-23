#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM
source=$repo/tests/ziran_frame_transition_test.zi

widget_host=ziran_frame_transition_host
widget_mesh_host=ziran_mesh_noop_host
set -- \
    --bind frame_transition:DisposeFrameTexture=ziran_frame_transition_host:DisposeFrameTexture \
    --bind image_raster:ImageWidth=ziran_frame_transition_host:ImageWidth \
    --bind image_raster:ImageHeight=ziran_frame_transition_host:ImageHeight
. "$repo/tests/widget_raster_bindings.sh"

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" -o "$work/ir" "$source"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" \
    --module-path "$ziran_root/std" "$@" \
    --entry ziran_frame_transition_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" "$@" \
    --entry ziran_frame_transition_test:main -o "$work/saved.zx" \
    "$work/ir/ziran_frame_transition_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        module=$source
        module_root=$repo/tests
        module_dir=$repo/src/ui
    else
        module=$work/ir/ziran_frame_transition_test.zir
        module_root=$work/ir
        module_dir=$work/ir
    fi
    output=$work/go-$input
    "$ziran" build --target=go --pkg main --exe \
        --entry ziran_frame_transition_test:main "$@" \
        --root "$module_root" --module-path "$module_dir" \
        --module-path "$ziran_root/std" -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done

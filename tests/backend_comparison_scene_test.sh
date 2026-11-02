#!/bin/sh
# CPU correctness only. This does not measure GPU throughput or presentation.
# Invoke through build-queue, as with the other compiler/build tests.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/backend-comparison-scene"
mkdir -p "$work"
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0 GOCACHE="$work/go-cache"
source="$repo/tests/backend_comparison_scene_test.zi"
set -- --bind raster:RasterLine=pixmap:RasterLine \
    --bind raster_shape:RasterRoundedRectangle=pixmap:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=pixmap:RasterRoundedRectangleOutline \
    --bind raster_text:RasterText=pixmap:RasterText \
    --bind raster_text:RasterTextClipped=pixmap:RasterTextClipped \
    --bind raster_triangles:RasterTriangles=pixmap:RasterTriangles
"$ziran" ir --project --root "$repo/tests" --module-path "$repo/tests" -o "$work/ir" "$source"
for target in c go; do
    package=; if test "$target" = go; then package='--pkg main'; fi
    "$ziran" build --project --root "$repo/tests" --module-path "$repo/tests" --target="$target" \
        --entry backend_comparison_scene_test:main "$@" $package --exe \
        -o "$work/$target" "$source"
    if test "$target" = go; then
        GO111MODULE=off go run "$work/go"/*.go > "$work/go.log" || { status=$?; cat "$work/go.log"; exit "$status"; }
        cmp "$work/c.log" "$work/go.log"
    else
        "$work/c/backend_comparison_scene_test" > "$work/c.log" || { status=$?; cat "$work/c.log"; exit "$status"; }
        cat "$work/c.log"
    fi
done
"$ziran" build --root "$work/ir" --module-path "$work/ir" --target=c \
    --entry backend_comparison_scene_test:main --exe -o "$work/c" \
    "$work/ir/backend_comparison_scene_test.zir"
"$work/c/backend_comparison_scene_test" > "$work/saved.log" || { status=$?; cat "$work/saved.log"; exit "$status"; }
cmp "$work/c.log" "$work/saved.log"
for input in source saved; do
    if test "$input" = source; then
        "$ziran" bundle --project --root "$repo/tests" --module-path "$repo/tests" --entry backend_comparison_scene_test:main \
            "$@" -o "$work/$input.zx" "$source"
    else
        "$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry backend_comparison_scene_test:main \
            "$@" -o "$work/$input.zx" "$work/ir/backend_comparison_scene_test.zir"
    fi
    "$ziran" run "$work/$input.zx" > "$work/$input.log"
    test "$(tail -1 "$work/$input.log")" = 0
    # The portable runner appends the entry result after program output.
    sed '$d' "$work/$input.log" > "$work/portable.log"
    cmp "$work/c.log" "$work/portable.log"
done
printf 'Backend comparison scenes match C, Go, saved IR and portable bundles.\n'

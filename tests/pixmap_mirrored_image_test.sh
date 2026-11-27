#!/bin/sh
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work="$repo/build/scratch/pixmap-mirrored-image"
mkdir -p "$work"
source="$repo/tests/pixmap_mirrored_image_test.zi"
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in c go; do
    for input in source saved; do
        executable=--exe
        package=; if test "$target" = go; then package='--pkg main'; fi
        if test "$input" = source; then
            "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_mirrored_image_test:main $package $executable -o "$work/$target-$input" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" --entry pixmap_mirrored_image_test:main $package $executable -o "$work/$target-$input" "$work/ir/pixmap_mirrored_image_test.zir"
        fi
        if test "$target" = go; then
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$work/$target-$input"/*.go
        else
            env -u DISPLAY -u WAYLAND_DISPLAY "$work/$target-$input/pixmap_mirrored_image_test"
        fi
    done
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_mirrored_image_test:main -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_mirrored_image_test:main -o "$work/saved.zx" "$work/ir/pixmap_mirrored_image_test.zir"
cmp "$work/source.zx" "$work/saved.zx"
test "$(env -u DISPLAY -u WAYLAND_DISPLAY "$ziran_root/build/bin/zi2zx" run --backend engine "$work/source.zx" | tail -1)" = 0

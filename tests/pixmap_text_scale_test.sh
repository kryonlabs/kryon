#!/bin/sh
# Scaled pixmap text: drawn ink, caret edges and pointer hits agree at 100%,
# 125% and 150%, on every native target and in portable bundles.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
mkdir -p "$repo/build/scratch/pixmap-text-scale"
work=$(mktemp -d "$repo/build/scratch/pixmap-text-scale/run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS
export GOCACHE="$work/go-cache"
source="$repo/tests/pixmap_text_scale_test.zi"
set -- --bind font_metrics:MeasureGlyphWidth=pixmap:MeasureGlyphWidth \
    --bind font_metrics:MeasureGlyphLineHeight=pixmap:MeasureGlyphLineHeight
passed='Scaled text ink, carets and pointer agree'
"$ziran" ir --project --root "$repo/tests" -o "$work/ir" "$source"
for target in c go; do
    for input in source saved; do
        package=; binds=; if test "$target" = go; then package='--pkg main'; binds="$*"; fi
        if test "$input" = source; then
            "$ziran" build --project --root "$repo/tests" --target="$target" --entry pixmap_text_scale_test:main $binds $package --exe -o "$work/$target-$input" "$source"
        else
            "$ziran" build --root "$work/ir" --module-path "$work/ir" --target="$target" --entry pixmap_text_scale_test:main $binds $package --exe -o "$work/$target-$input" "$work/ir/pixmap_text_scale_test.zir"
        fi
        if test "$target" = go; then
            test "$(GO111MODULE=off go run "$work/$target-$input"/*.go | tail -1)" = "$passed"
        else
            test "$("$work/$target-$input/pixmap_text_scale_test" | tail -1)" = "$passed"
        fi
    done
done
"$ziran" bundle --project --root "$repo/tests" --entry pixmap_text_scale_test:main "$@" -o "$work/source.zx" "$source"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" --entry pixmap_text_scale_test:main "$@" -o "$work/saved.zx" "$work/ir/pixmap_text_scale_test.zir"
test "$("$ziran" run "$work/source.zx" | tail -1)" = 0
test "$("$ziran" run "$work/saved.zx" | tail -1)" = 0
printf '%s on every target and in bundles.\n' "$passed"

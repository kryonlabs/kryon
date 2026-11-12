set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"

unset DISPLAY WAYLAND_DISPLAY XAUTHORITY DBUS_SESSION_BUS_ADDRESS SESSION_MANAGER
export YUE_DESKTOP_RECOVERY=0

work=$(mktemp -d "$repo/build/scratch/cui-overlays-run.XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM

source="$repo/tests/cui_overlays_test.zi"
widget_test=cui_overlays_test
widget_std="$ziran_root/std"
module_args="$kryon_project_args"

binds='
--bind font_metrics:MeasureGlyphWidth=cui_overlays_raster_host:CuiOverlaysMeasureGlyphWidth
--bind font_metrics:MeasureGlyphLineHeight=cui_overlays_raster_host:CuiOverlaysMeasureGlyphLineHeight
--bind raster_text:RasterText=cui_overlays_raster_host:CuiOverlaysRasterText
--bind raster_text:RasterTextClipped=cui_overlays_raster_host:CuiOverlaysRasterTextClipped
--bind raster_text:RasterTextRotated=cui_overlays_raster_host:CuiOverlaysRasterTextRotated
--bind raster_shape:RasterRoundedRectangle=cui_overlays_raster_host:CuiOverlaysRasterRoundedRectangle
--bind raster_shape:RasterRoundedRectangleOutline=cui_overlays_raster_host:CuiOverlaysRasterRoundedRectangleOutline
--bind raster:RasterLine=cui_overlays_raster_host:CuiOverlaysRasterLine
--bind paint_queue:RasterImage=cui_overlays_raster_host:CuiOverlaysRasterImage
--bind paint_queue:RasterImageMesh=cui_overlays_raster_host:CuiOverlaysRasterImageMesh'

# shellcheck disable=SC2086
"$ziran" ir --root "$repo/tests" $module_args \
    -o "$work/ir" "$source"
# shellcheck disable=SC2086
"$ziran" bundle --root "$repo/tests" $module_args $binds \
    --entry "$widget_test:main" -o "$work/source.zx" "$source"
# shellcheck disable=SC2086
"$ziran" bundle --root "$work/ir" --project --module-path "$work/ir" $binds \
    --entry "$widget_test:main" -o "$work/saved.zx" \
    "$work/ir/$widget_test.zir"

cmp "$work/source.zx" "$work/saved.zx"
test "$("$ziran" run "$work/source.zx")" = 0
test "$("$ziran" run "$work/saved.zx")" = 0

for input in source saved; do
    if test "$input" = source; then
        root=$repo/tests
        module=$source
        paths=$module_args
    else
        root=$work/ir
        module=$work/ir/$widget_test.zir
        paths="--project --module-path $work/ir"
    fi

    output=$work/c-$input
    # shellcheck disable=SC2086
    "$ziran" build --target=c --entry "$widget_test:main" \
        --root "$root" $paths $binds -o "$output" "$module"
    # shellcheck disable=SC2086
    "${CC:-cc}" -O2 -std=c11 -ffunction-sections -fdata-sections \
        -Wl,--gc-sections -I"$output" -I"$ziran_root/include" \
        "$output"/*.c "${ZIRAN_LIB:-$ziran_root/build/libziran.a}" \
        -lm -lpthread -o "$output/app"
    "$output/app"

    output=$work/go-$input
    # shellcheck disable=SC2086
    "$ziran" build --target=go --pkg main --exe \
        --entry "$widget_test:main" --root "$root" $paths $binds \
        -o "$output" "$module"
    env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
done

printf 'CUI overlays: source/saved IR ZX bytes match; ZX/C/Go behavior and light/dark raster checks passed\n'
printf 'source_zx_bytes=%s saved_zx_bytes=%s source_zx_sha256=%s\n' \
    "$(wc -c < "$work/source.zx")" "$(wc -c < "$work/saved.zx")" \
    "$(sha256sum "$work/source.zx" | cut -d' ' -f1)"

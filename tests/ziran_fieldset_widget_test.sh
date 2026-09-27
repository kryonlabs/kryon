#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/fieldset_widget_behavior.zi
portable=$repo/tests/fieldset_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphWidth=fieldset_widget_host:MeasureGlyphWidth \
    --bind raster_shape:RasterRoundedRectangle=fieldset_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=fieldset_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=fieldset_widget_host:RasterLine \
    --bind raster_text:RasterText=fieldset_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=fieldset_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=fieldset_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry fieldset_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry fieldset_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/fieldset_widget_portable_test.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 0
test "$("$ziran" run "$work/saved.zib")" = 0

for target in c cpp; do
    output=$work/native-$target
    "$ziran" build --target="$target" --root "$repo/tests" \
        --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
        -o "$output" "$portable"
    if test "$target" = c; then
        "${CC:-cc}" -std=c11 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c -o "$output/app"
    else
        "${CXX:-c++}" -std=c++17 -ffunction-sections -fdata-sections \
            -Wl,--gc-sections -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
    fi
    env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
done

output=$work/native-go
"$ziran" build --target=go --root "$repo/tests" \
    --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$output" "$source"
cat > "$output/fieldset_widget_test.go" <<'GO'
package ziran
import "testing"
type fieldsetHost struct { t *testing.T; fills, outlines, labels int }
func (h *fieldsetHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 {
    if value != "Group" || font != 18 || typeface != "" {
        h.t.Fatal("fieldset title measurement")
    }
    return 35
}
func (h *fieldsetHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 18 }
func (h *fieldsetHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if h.fills < 2 &&
       (color.R != 0x12 || color.G != 0x34 ||
        color.B != 0x56 || color.A != 127) { h.t.Fatal("fieldset KSS") }
    if h.fills == 1 &&
       (bounds.X != 18 || bounds.Y != 22 || bounds.Width != 51 ||
        bounds.Height != 18 || radius != 0 || segments != 4) {
        h.t.Fatal("fieldset title cover")
    }
    h.fills++
}
func (h *fieldsetHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    if radius != 6 || segments != 12 || width != 1 {
        h.t.Fatal("fieldset border")
    }
    h.outlines++
}
func (h *fieldsetHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *fieldsetHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped title") }
func (h *fieldsetHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Group" || x != 26 || y != 21 || font != 18 ||
       clip.X != 26 || clip.Y != 21 || clip.Width != 35 ||
       clip.Height != 18 || color.R != 0xaa || color.G != 0xbb ||
       color.B != 0xcc || color.A != 127 { h.t.Fatal("fieldset title") }
    h.labels++
}
func (h *fieldsetHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestFieldsetWidget(t *testing.T) {
    h := &fieldsetHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    if FieldsetWidgetBehavior_Frame() != 7 || h.fills != 3 || h.outlines != 2 || h.labels != 1 {
        t.Fatal("fieldset frame")
    }
}
GO
env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go test "$output"/*.go

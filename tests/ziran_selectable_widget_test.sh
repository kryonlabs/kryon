#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/selectable_widget_behavior.zi
portable=$repo/tests/selectable_widget_portable_test.zi
set -- \
    --bind font_metrics:MeasureGlyphLineHeight=selectable_widget_host:MeasureGlyphLineHeight \
    --bind raster_shape:RasterRoundedRectangle=selectable_widget_host:RasterRoundedRectangle \
    --bind raster_shape:RasterRoundedRectangleOutline=selectable_widget_host:RasterRoundedRectangleOutline \
    --bind raster:RasterLine=selectable_widget_host:RasterLine \
    --bind raster_text:RasterText=selectable_widget_host:RasterText \
    --bind raster_text:RasterTextClipped=selectable_widget_host:RasterTextClipped \
    --bind paint_queue:RasterImage=selectable_widget_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    "$@" --entry selectable_widget_portable_test:main \
    -o "$work/saved.zib" "$work/ir/selectable_widget_portable_test.zir"
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
cat > "$output/selectable_widget_test.go" <<'GO'
package ziran
import "testing"
type selectableHost struct { t *testing.T; fills, labels int }
func (h *selectableHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *selectableHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 {
    if font != 14 || typeface != "" { h.t.Fatal("glyph height") }
    return 12
}
func (h *selectableHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X != 10 || bounds.Y != 20 || bounds.Width != 100 ||
       bounds.Height != 36 || radius != 4 || segments != 12 ||
       color.R != 0x12 || color.G != 0x34 || color.B != 0x56 {
        h.t.Fatal("selectable fill")
    }
    h.fills++
}
func (h *selectableHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *selectableHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *selectableHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unclipped text") }
func (h *selectableHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) {
    if value != "Alpha" || x != 18 || y != 32 || font != 14 ||
       clip.X != 10 || clip.Y != 20 || clip.Width != 100 ||
       clip.Height != 36 || color.R != 0xaa ||
       color.G != 0xbb || color.B != 0xcc { h.t.Fatal("selectable label") }
    h.labels++
}
func (h *selectableHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestSelectableWidget(t *testing.T) {
    h := &selectableHost{t: t}
    SetFontMetricsHost(h)
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    expectedFills := []int{0, 1, 2, 2}
    for phase := 0; phase < 4; phase++ {
        if SelectableWidgetBehavior_Frame() != int32(phase) || h.fills != expectedFills[phase] ||
           h.labels != phase+1 { t.Fatal("selectable phase", phase) }
    }
}
GO
env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go test "$output"/*.go

#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

source=$repo/tests/paned_view_behavior.zi
portable=$repo/tests/paned_view_portable_test.zi
bind_fill=raster_shape:RasterRoundedRectangle=paned_view_host:RasterRoundedRectangle
bind_outline=raster_shape:RasterRoundedRectangleOutline=paned_view_host:RasterRoundedRectangleOutline
bind_line=raster:RasterLine=paned_view_host:RasterLine
bind_text=raster_text:RasterText=paned_view_host:RasterText
bind_clipped=raster_text:RasterTextClipped=paned_view_host:RasterTextClipped
bind_image=paint_queue:RasterImage=paned_view_host:RasterImage

"$ziran" ir --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$portable"
"$ziran" bundle --root "$repo/tests" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --entry paned_view_portable_test:main \
    -o "$work/source.zib" "$portable"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --bind "$bind_fill" --bind "$bind_outline" \
    --bind "$bind_line" --bind "$bind_text" --bind "$bind_clipped" \
    --bind "$bind_image" --entry paned_view_portable_test:main \
    -o "$work/saved.zib" "$work/ir/paned_view_portable_test.zir"
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
cat > "$output/paned_view_test.go" <<'GO'
package ziran
import "testing"
type panedHost struct { t *testing.T; fills int }
func (h *panedHost) MeasureGlyphWidth(value string, font int32,
    typeface string) int32 { return 0 }
func (h *panedHost) MeasureGlyphLineHeight(font int32,
    typeface string) int32 { return 12 }
func (h *panedHost) RasterRoundedRectangle(bounds Rectangle,
    radius float32, segments int32, color Color) {
    if bounds.X < 10 || bounds.Y < 20 || bounds.X+bounds.Width > 130 ||
       bounds.Y+bounds.Height > 100 { h.t.Fatal("fill outside pane") }
    h.fills++
}
func (h *panedHost) RasterRoundedRectangleOutline(bounds Rectangle,
    radius float32, segments int32, width float32, color Color) {
    h.t.Fatal("unexpected outline")
}
func (h *panedHost) RasterLine(bounds Rectangle, color Color) {
    h.t.Fatal("unexpected line")
}
func (h *panedHost) RasterText(value string, x, y, font int32,
    color Color) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterTextClipped(value string, x, y, font int32,
    color Color, clip Rectangle) { h.t.Fatal("unexpected text") }
func (h *panedHost) RasterImage(path string, id uint32,
    source, destination, clip Rectangle, origin Vector2,
    rotation, radius float32, tint Color) { h.t.Fatal("unexpected image") }
func TestPanedView(t *testing.T) {
    h := &panedHost{t: t}
    SetRasterShapeHost(h)
    SetRasterTextHost(h)
    SetRasterHost(h)
    SetPaintQueueHost(h)
    for phase := 0; phase < 7; phase++ {
        if PanedViewBehavior_Frame() != int32(phase) { t.Fatal("pane phase", phase) }
    }
    if h.fills != 7 { t.Fatal("pane fill count", h.fills) }
}
GO
env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go test "$output"/*.go

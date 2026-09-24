#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'EOF'
#module "app"
#import "bevel"
#import "drawing_props"
#import "geometry"
#import "separator"
#import "style"
Answer :: () -> i32 #export {
    light: Color = (Color){(u8)10, (u8)20, (u8)30, (u8)40}
    dark: Color = (Color){(u8)50, (u8)60, (u8)70, (u8)80}
    RenderBevel(10, 20, 5, 4, light, dark)
    bounds: Rectangle = (Rectangle){2.0, 3.0, 10.0, 6.0}
    frame: StyleFrame
    frame.value.background = (u32)0x11223344
    RenderSeparatorLine(bounds, true, frame)
    frame.value.background = (u32)0x55667788
    RenderSeparatorLine(bounds, false, frame)
    return 42
}
EOF

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" -I"$repo/../ziran/include" \
    "$repo/tests/ziran_raster_host_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$repo/../ziran/build/libziran.a" \
    -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict --root "$work" \
            --module-path "$module_dir" -o "$output" "$module"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
#include <assert.h>
static int calls;
static const float expected[6][4] = {
    {10, 20, 4, 0}, {10, 20, 0, 3},
    {10, 23, 4, 0}, {14, 20, 0, 3},
    {7, 3, 0, 6}, {2, 6, 10, 0},
};
static const unsigned char colors[6][4] = {
    {10, 20, 30, 40}, {10, 20, 30, 40},
    {50, 60, 70, 80}, {50, 60, 70, 80},
    {0x11, 0x22, 0x33, 0x44}, {0x55, 0x66, 0x77, 0x88},
};
void RasterLine(Rectangle line, Color color) {
    assert(calls < 6);
    assert(line.x == expected[calls][0] && line.y == expected[calls][1]);
    assert(line.width == expected[calls][2] &&
           line.height == expected[calls][3]);
    assert(color.r == colors[calls][0] && color.g == colors[calls][1]);
    assert(color.b == colors[calls][2] && color.a == colors[calls][3]);
    calls++;
}
int main(void) { return Answer() == 42 && calls == 6 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
#include <cassert>
static int calls;
static const float expected[6][4] = {
    {10, 20, 4, 0}, {10, 20, 0, 3},
    {10, 23, 4, 0}, {14, 20, 0, 3},
    {7, 3, 0, 6}, {2, 6, 10, 0},
};
static const unsigned char colors[6][4] = {
    {10, 20, 30, 40}, {10, 20, 30, 40},
    {50, 60, 70, 80}, {50, 60, 70, 80},
    {0x11, 0x22, 0x33, 0x44}, {0x55, 0x66, 0x77, 0x88},
};
extern "C" void RasterLine(Rectangle line, Color color) {
    assert(calls < 6);
    assert(line.x == expected[calls][0] && line.y == expected[calls][1]);
    assert(line.width == expected[calls][2] &&
           line.height == expected[calls][3]);
    assert(color.r == colors[calls][0] && color.g == colors[calls][1]);
    assert(color.b == colors[calls][2] && color.a == colors[calls][3]);
    calls++;
}
int main() { return Answer() == 42 && calls == 6 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
                "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/raster_test.go" <<'GO'
package ziran
import "testing"
type lineHost struct { t *testing.T; calls int }
func (host *lineHost) RasterLine(line Rectangle, color Color) {
    expected := [6][4]float32{
        {10, 20, 4, 0}, {10, 20, 0, 3},
        {10, 23, 4, 0}, {14, 20, 0, 3},
        {7, 3, 0, 6}, {2, 6, 10, 0},
    }
    if host.calls >= 6 { host.t.Fatal("too many lines") }
    want := expected[host.calls]
    if line.X != want[0] || line.Y != want[1] ||
       line.Width != want[2] || line.Height != want[3] {
        host.t.Fatal("wrong raster line", line)
    }
    colors := [6][4]uint8{
        {10, 20, 30, 40}, {10, 20, 30, 40},
        {50, 60, 70, 80}, {50, 60, 70, 80},
        {0x11, 0x22, 0x33, 0x44}, {0x55, 0x66, 0x77, 0x88},
    }
    wantColor := colors[host.calls]
    if color.R != wantColor[0] || color.G != wantColor[1] ||
       color.B != wantColor[2] || color.A != wantColor[3] {
        host.t.Fatal("wrong raster color", color)
    }
    host.calls++
}
func TestRaster(t *testing.T) {
    host := &lineHost{t: t}
    SetRasterHost(host)
    if App_Answer() != 42 || host.calls != 6 { t.Fatal("line draw order") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done

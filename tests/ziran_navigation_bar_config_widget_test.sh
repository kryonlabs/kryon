#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "dropdown"
#import "geometry"
#import "navigation_bar_config_props"
#import "navigation_bar_config_widget"
#import "tree"
#import "tree_input"
#import "widget_kind"

phase :: i32 #global
route_count :: i32 #global
routes :: [4]i32 #global
open :: bool #global
open_row :: i32 #global
highlight :: i32 #global
menu_scroll :: i32 #global

FindLabel :: (label: string) -> i32 {
    index: i32 = 0
    while index < TreeCount() {
        if TreeNodeAt(index).semantic_label == label {
            return index
        }
        index += 1
    }
    return -1
}

Click :: (index: i32) -> bool {
    if index < 0 { return false }
    bounds: Rectangle = TreeNodeAt(index).bounds
    x: float = bounds.x + bounds.width * 0.5
    y: float = bounds.y + bounds.height * 0.5
    TreePointerUpdate((PointerFrame){x, y, true, true, false})
    TreePointerUpdate((PointerFrame){x, y, false, false, true})
    return true
}

Frame :: () -> i32 #export {
    if phase == 0 {
        route_count = 2
        routes[0] = 11
        routes[1] = 22
    }
    options: [2]NavigationBarOption
    options[0].key = (u64)101
    options[0].route = 11
    options[0].label = "Home"
    options[1].key = (u64)102
    options[1].route = 22
    options[1].label = "Search"
    labels: [4]string
    labels[0] = "First"
    labels[1] = "Second"
    labels[2] = "Third"
    props: NavigationBarConfigProps
    props.key = (u64)40
    props.id = 40
    props.title = "Navigation"
    props.route_count = route_count
    props.max_route_count = 4
    props.dropdown_open = open
    props.dropdown_row = open_row
    props.dropdown_highlight_index = highlight
    props.dropdown_scroll_offset = menu_scroll
    props.focused_row = 0
    if phase == 6 { props.trigger_input.enter = true }
    if phase == 7 {
        props.menu_input.navigating = true
        props.menu_input.up = true
    }
    if phase == 8 { props.menu_input.commit = true }
    props.close_label = "Close"
    props.remove_label = "Remove"
    props.add_label = "Add"
    props.reset_label = "Reset"
    props.cancel_label = "Cancel"
    props.save_label = "Save"
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 640.0, 480.0})
    result: NavigationBarConfigResult = NavigationBarConfig(
        props, routes[0:4], labels[0:4], options[0:2])
    if !TreeFinish() || result.node != 1 ||
        FindLabel("Add") < 0 || FindLabel("Save") < 0 {
        return -20
    }
    route_count = result.route_count
    open = result.dropdown_open
    open_row = result.dropdown_row
    highlight = result.dropdown_highlight_index
    menu_scroll = result.dropdown_scroll_offset
    if phase == 0 {
        if result.changed || open || FindLabel("Home") < 0 ||
            !Click(FindLabel("Home")) { return -1 }
    } else if phase == 1 {
        if !open || open_row != 0 ||
            FindLabel("Search") < 0 ||
            !Click(FindLabel("Search")) { return -2 }
    } else if phase == 2 {
        if !result.changed || open || routes[0] != 22 ||
            !Click(FindLabel("Add")) { return -3 }
    } else if phase == 3 {
        if !result.changed || route_count != 3 ||
            routes[2] != 11 ||
            !Click(FindLabel("Remove")) { return -4 }
    } else if phase == 4 {
        if !result.changed || route_count != 2 ||
            routes[0] != 22 ||
            !Click(FindLabel("Save")) { return -5 }
    } else if phase == 5 {
        if result.action != 2 || route_count != 2 {
            return -6
        }
    } else if phase == 6 {
        if !open || open_row != 0 || highlight != 1 {
            return -7
        }
    } else if phase == 7 {
        if !open || highlight != 0 { return -8 }
    } else if phase == 8 {
        if open || !result.changed || routes[0] != 11 {
            return -9
        }
    }
    old: i32 = phase
    phase += 1
    return old
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Frame -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Frame -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_navigation_bar_config_widget_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

cat > "$work/native_main.h" <<'C'
#ifdef __cplusplus
#include "app.hpp"
#include <cassert>
#define HOST extern "C"
#else
#include "app.h"
#include <assert.h>
#define HOST
#endif
HOST int32_t MeasureGlyphWidth(String value, int32_t font,
    String face) { (void)face; return (int32_t)value.length * font / 2; }
HOST int32_t MeasureGlyphLineHeight(int32_t font,
    String face) { (void)face; return font; }
HOST int32_t ImageWidth(String path) { (void)path; return 0; }
HOST int32_t ImageHeight(String path) { (void)path; return 0; }
HOST void RasterImage(String path, uint32_t id, Rectangle source,
    Rectangle destination, Rectangle clip, Vector2 origin,
    float rotation, float radius, Color tint) {
    (void)path; (void)id; (void)source; (void)destination;
    (void)clip; (void)origin; (void)rotation; (void)radius;
    (void)tint;
}
HOST void RasterRoundedRectangle(Rectangle bounds, float radius,
    int32_t segments, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)color;
}
HOST void RasterRoundedRectangleOutline(Rectangle bounds,
    float radius, int32_t segments, float width, Color color) {
    (void)bounds; (void)radius; (void)segments; (void)width;
    (void)color;
}
HOST void RasterLine(Rectangle line, Color color) {
    (void)line; (void)color;
}
HOST void RasterText(String value, int32_t x, int32_t y,
    int32_t font, Color color) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
}
HOST void RasterTextClipped(String value, int32_t x, int32_t y,
    int32_t font, Color color, Rectangle clip) {
    (void)value; (void)x; (void)y; (void)font; (void)color;
    (void)clip;
}
int main(void) {
    for (int phase = 0; phase < 9; phase++)
        assert(Frame() == phase);
    return 0;
}
C

for target in c cpp go; do
    output=$work/native-$target
    "$ziran" build --target="$target" --strict --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cp "$work/native_main.h" "$output/main.c"
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cp "$work/native_main.h" "$output/main.cpp"
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
            -I"$output" "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/navigation_bar_config_widget_test.go" <<'GO'
package ziran
import "testing"
type configHost struct{}
func (configHost) MeasureGlyphWidth(value string, font int32,
    face string) int32 { return int32(len(value)) * font / 2 }
func (configHost) MeasureGlyphLineHeight(font int32,
    face string) int32 { return font }
func TestNavigationBarConfig(t *testing.T) {
    SetFontMetricsHost(configHost{})
    for phase := int32(0); phase < 9; phase++ {
        if got := App_Frame(); got != phase {
            t.Fatalf("phase %d returned %d", phase, got)
        }
    }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done

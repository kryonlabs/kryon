#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/use_moved.zi" <<'EOF'
#module "use_moved"
#import "canvas"
#import "color_picker"
#import "control_props"
#import "drawing_props"
#import "focus"
#import "geometry"
#import "list_box"
#import "menu"
#import "page"
#import "paned_view"
#import "primitive"
#import "radio"
#import "scroll"
#import "segmented_control"
#import "style"
#import "style_sheet"
#import "table_view"
#import "text_input"
#import "toast"
#import "tree"
#import "widget_kind"

Answer :: () -> i32 #export {
    if !StyleMatchesOptional(-1, 8) { return 0 }
    if StyleMatchesOptional(3, 8) { return 0 }
    if StyleSpecificity(1, 2, 3, 4) <= 0 { return 0 }
    style: StyleData
    style.fields = (u32)StyleGap | (u32)StylePaddingX
    style.gap = 8.0
    style.padding_x = 12.0
    page: PageLayoutMetrics = PageLayoutMetricsFor(style)
    if page.gap != 8 || page.padding != 12 { return 0 }
    selection: SegmentedSelectionResult = SegmentedSelectionFor(1, 2, true)
    if !selection.changed || selection.selected_index != 2 { return 0 }
    if SegmentedNextRowWidth(20, 30, 5) != 55 { return 0 }
    if !TreeSameIdentity(1, 2, 3, 1, 2, 3) { return 0 }
    if TreeSameIdentity(1, 2, 3, 1, 2, 4) { return 0 }
    if !TreeInteractiveButtonLike(WidgetKindButton, 0, false, false) {
        return 0
    }
    if FocusTabDirectionFor(true, true) != -1 { return 0 }
    if FocusTabDirectionFor(false, true) != 0 { return 0 }
    bounds: Rectangle
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.width = 100.0
    bounds.height = 80.0
    point: Vector2
    point.x = 30.0
    point.y = 40.0
    screen: Vector2 = CanvasPointToScreen(bounds, point, 4, 6, 2.0)
    world: Vector2 = CanvasPointFromScreen(bounds, screen, 4, 6, 2.0)
    if world.x != point.x || world.y != point.y { return 0 }
    if ScrollClamp(-5, 20) != 0 || ScrollClamp(40, 20) != 20 {
        return 0
    }
    if ScrollMax(80, 100) != 0 || ScrollMax(180, 100) != 80 {
        return 0
    }
    if MenuWrappedItemIndex(2, 1, 3) != 0 { return 0 }
    if RadioActivationFor(7, true, false) != 7 { return 0 }
    if ColorPickerChannelByte(0.5) != (u8)128 { return 0 }
    line: LinePrimitive = PrimitiveLineFor(5, 9, 1, 3)
    if line.bounds.x != 1.0 || line.bounds.y != 3.0 ||
        line.bounds.width != 4.0 || line.bounds.height != 6.0 {
        return 0
    }
    color: Color
    color.r = (u8)20
    color.a = (u8)255
    if PrimitiveAppBackgroundColor(color, color).r != (u8)20 {
        return 0
    }
    if ListBoxClampScroll(99, 15) != 15 { return 0 }
    if PanedViewClampSplit(5, 10, 90) != 10 { return 0 }
    if TableViewSelectedRowFor(8, 3) != 2 { return 0 }
    if !TextNativeEditShouldRun(false, false) { return 0 }
    if TextNativeEditShouldRun(true, false) { return 0 }
    if ToastDeadlineFor(10.0, 2.0) != 12.0 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" --module-path "$repo/src/ui" "$work/use_moved.zi"
"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/use_moved.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry use_moved:Answer -o "$work/source.zib" "$work/use_moved.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry use_moved:Answer -o "$work/ir.zib" "$work/ir/use_moved.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

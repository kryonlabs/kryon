#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/geometry.zi" "$work/geometry.zi"
cp "$ziran_root/std/math.zi" "$work/math.zi"
cp "$repo/src/ui/layout/layout_behavior.zi" "$work/layout_behavior.zi"
cp "$repo/src/ui/layout/layout_props.zi" "$work/layout_props.zi"
cp "$repo/src/ui/group.zi" "$work/group.zi"
cat > "$work/use_layout.zi" <<'EOF'
#import "geometry"
#import "layout_behavior"
LayoutProps :: #import "layout_props";
#import "group"

using FlexDirection;
using JustifyContent;
using AlignItems;

CheckBatch :: () -> bool {
    props: FlexProps = .{.bounds = .{10.0, 20.0, 110.0, 0.0}, .gap = 10,
        .cross_gap = 5, .wrap = true, .align_items = .AlignEnd}
    items: [3]FlexItem = .[.{.width = 40.0, .height = 20.0, .grow = 1.0},
        .{.width = 40.0, .height = 30.0, .grow = 1.0}, .{.width = 60.0, .height = 12.0}]
    boxes: [3]FlexPlacement
    cross := PlaceFlexItems(props, items[:], boxes[:])
    if cross != 47.0 || boxes[0].bounds.x != 10.0 || boxes[0].bounds.y != 30.0 || boxes[0].bounds.width != 50.0 ||
        boxes[1].bounds.x != 70.0 || boxes[1].bounds.y != 20.0 || boxes[2].bounds.x != 10.0 || boxes[2].bounds.y != 55.0 { return false }
    if PlaceFlexItems(props, items[:], boxes[:2]) != -1.0 || boxes[2].bounds.y != 55.0 { return false }

    props.wrap = false
    props.bounds.width = 70.0
    items[0] = .{.basis = 40.0, .has_basis = true, .height = 15.0, .shrink = 1.0}
    items[1] = .{.width = 80.0, .height = 20.0, .shrink = 1.0}
    cross = PlaceFlexItems(props, items[:2], boxes[:])
    if cross != 20.0 || boxes[0].bounds.width != 20.0 || boxes[1].bounds.width != 40.0 || boxes[1].bounds.x != 40.0 { return false }
    items[0].shrink = 10.0
    props.bounds.width = 50.0
    cross = PlaceFlexItems(props, items[:2], boxes[:])
    if cross != 20.0 || boxes[0].bounds.width != 0.0 || boxes[1].bounds.width != 40.0 { return false }

    props = .{.bounds = .{10.0, 20.0, 70.0, 100.0}, .direction = .FlexColumn, .reverse = true,
        .padding = 5, .gap = 4, .align_items = .AlignStretch, .justify_content = .JustifyEnd}
    items[0] = .{.height = 20.0}
    items[1] = .{.width = 10.0, .height = 12.0, .align_self = .AlignCenter, .has_align_self = true}
    cross = PlaceFlexItems(props, items[:2], boxes[:])
    return cross == 20.0 && boxes[0].bounds.x == 15.0 && boxes[0].bounds.y == 41.0 && boxes[0].bounds.width == 60.0 &&
        boxes[1].bounds.x == 40.0 && boxes[1].bounds.y == 25.0
}

#program_export
Answer :: () -> s32 {
    if KeyHash("") != cast(u64)1469598103934665603 { return 0 }
    if KeyHash("results/root") != cast(u64)5627422651130798188 { return 0 }
    if KeyHash("meditation/root") != cast(u64)4594846862641403136 { return 0 }
    if KeyHash("settings/root") != cast(u64)4237429609958274031 { return 0 }
    if KeyHash("☯") != cast(u64)9292100647144557370 { return 0 }
    centered: CenteredColumnLayout = CenteredColumnFor(800, 600, 50)
    compact: CenteredColumnLayout = CenteredColumnFor(200, 500, 20)
    if centered.x != 100 || centered.width != 600 { return 0 }
    if compact.x != 20 || compact.width != 160 { return 0 }
    if PageSidePaddingFor(300) != 12 { return 0 }
    if PageSidePaddingFor(2000) != 24 { return 0 }
    if DesktopWidthThresholdFor(0.0) != 500 { return 0 }
    if DesktopWidthThresholdFor(1.5) != 750 { return 0 }
    if IsDesktopWidth(749, 1.5) { return 0 }
    if !IsDesktopWidth(750, 1.5) { return 0 }
    bounds: Rectangle
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.width = 100.0
    bounds.height = 60.0
    column: LayoutProps.Props
    column.bounds = bounds
    column.gap = 7
    column.key = cast(u64)1 << 63
    if column.key != (cast(u64)1 << 63) || column.bounds.width != 100.0 ||
       column.gap != 7 { return 0 }
    metrics: LayoutMetrics = LayoutMetricsFor(bounds, 7, 5)
    if metrics.content.x != 15.0 || metrics.content.y != 25.0 { return 0 }
    if metrics.content.width != 90.0 || metrics.content.height != 50.0 { return 0 }
    if metrics.gap != 7 || metrics.padding != 5 { return 0 }
    unclamped: LayoutMetrics = LayoutMetricsFor(bounds, -7, -5)
    if unclamped.gap != 0 || unclamped.padding != 0 { return 0 }
    if unclamped.content.x != 10.0 || unclamped.content.width != 100.0 { return 0 }
    scoped: Rectangle = bounds
    scoped.width = 0.0
    scoped.height = -1.0
    scoped = LayoutScopeBounds(scoped, 80, 40)
    if scoped.width != 80.0 || scoped.height != 40.0 { return 0 }
    if LayoutCursorForChild(metrics, true, 2, 30.0) != 59.0 { return 0 }
    current: Rectangle
    current.width = 20.0
    placed: Rectangle = LayoutChildBounds(current, current, metrics, true, 59.0)
    if placed.x != 59.0 || placed.y != 25.0 { return 0 }
    if placed.width != 20.0 || placed.height != 50.0 { return 0 }
    placed = LayoutChildBounds(current, current, metrics, false, 33.0)
    if placed.x != 15.0 || placed.y != 33.0 { return 0 }
    declared: Rectangle = current
    declared.x = 1.0
    placed = LayoutChildBounds(declared, current, metrics, true, 59.0)
    if placed.x != 0.0 || placed.y != 0.0 { return 0 }
    placed = StackChildBounds(current, current, metrics)
    if placed.x != 15.0 || placed.y != 25.0 { return 0 }
    if placed.width != 20.0 || placed.height != 50.0 { return 0 }
    props: FlexProps
    props.bounds.width = 200.0
    props.bounds.height = 100.0
    props.direction = cast(FlexDirection)FlexRow
    props.justify_content = cast(JustifyContent)JustifySpaceBetween
    props.align_items = cast(AlignItems)AlignCenter
    props.gap = 5
    props.padding = 10
    cursor: FlexCursor = BeginFlexCursor(props, 2, 80.0)
    if cursor.position != 10.0 || cursor.gap != 100.0 { return 0 }
    cursor = FlexStep(cursor, 30.0, 20.0)
    if cursor.item.x != 10.0 || cursor.item.y != 40.0 { return 0 }
    if cursor.item.width != 30.0 || cursor.item.height != 20.0 { return 0 }
    cursor = FlexStep(cursor, 50.0, 40.0)
    if cursor.item.x != 140.0 || cursor.item.y != 30.0 { return 0 }
    if cursor.remaining != 0 { return 0 }
    props.direction = cast(FlexDirection)FlexColumn
    props.justify_content = cast(JustifyContent)JustifyStart
    props.align_items = cast(AlignItems)AlignStretch
    cursor = BeginFlexCursor(props, 2, 30.0)
    cursor = FlexStep(cursor, 0.0, 10.0)
    if cursor.item.x != 10.0 || cursor.item.y != 10.0 { return 0 }
    if cursor.item.width != 180.0 || cursor.item.height != 10.0 { return 0 }
    cursor = FlexStep(cursor, 0.0, 20.0)
    if cursor.item.y != 25.0 || cursor.item.width != 180.0 { return 0 }
    props.direction = cast(FlexDirection)FlexRow
    props.justify_content = cast(JustifyContent)JustifyEnd
    props.align_items = cast(AlignItems)AlignEnd
    cursor = BeginFlexCursor(props, 1, 20.0)
    if cursor.position != 170.0 { return 0 }
    cursor = FlexStep(cursor, 20.0, 20.0)
    if cursor.item.x != 170.0 || cursor.item.y != 70.0 { return 0 }
    cursor = BeginFlexCursor(props, 0, 0.0)
    cursor = FlexStep(cursor, 20.0, 20.0)
    if cursor.item.width != 0.0 || cursor.remaining != 0 { return 0 }
    if !CheckBatch() { return 0 }
    policy: GroupPolicy = GroupPolicyFor(bounds, 7, 5)
    if policy.bounds.x != 10.0 || policy.content.x != 15.0 { return 0 }
    if policy.content.width != 90.0 || policy.gap != 7 { return 0 }
    screen: Rectangle
    policy = ScreenGroupPolicyFor(screen, 200, 100, 3, 10)
    if policy.bounds.width != 200.0 || policy.bounds.height != 100.0 { return 0 }
    if policy.content.width != 180.0 || policy.content.height != 80.0 { return 0 }
    return 42
}
EOF
cat > "$work/scalar_layout.zi" <<'EOF'
#import "layout_behavior"
#program_export
Answer :: () -> s32 {
    if PageSidePaddingFor(300) == 12 && IsDesktopWidth(750, 1.5) {
        return 42
    }
    return 0
}
EOF
cat > "$work/portable_layout.zi" <<'EOF'
#import "geometry"
#import "layout_behavior"
#import "group"
#program_export
Answer :: () -> s32 {
    centered: CenteredColumnLayout = CenteredColumnFor(800, 600, 50)
    if centered.x != 100 || centered.width != 600 { return 0 }
    bounds: Rectangle
    bounds.x = 10.0
    bounds.y = 20.0
    bounds.width = 100.0
    bounds.height = 60.0
    metrics: LayoutMetrics = LayoutMetricsFor(bounds, 7, 5)
    if metrics.content.x != 15.0 || metrics.content.y != 25.0 { return 0 }
    if metrics.content.width != 90.0 || metrics.content.height != 50.0 { return 0 }
    scoped: Rectangle = bounds
    scoped.width = 0.0
    scoped = LayoutScopeBounds(scoped, 80, 40)
    if scoped.width != 80.0 || bounds.width != 100.0 { return 0 }
    policy: GroupPolicy = GroupPolicyFor(bounds, 7, 5)
    if policy.content.width != 90.0 || policy.gap != 7 { return 0 }
    screen: Rectangle
    policy = ScreenGroupPolicyFor(screen, 200, 100, 3, 10)
    if policy.bounds.width != 200.0 || policy.content.height != 80.0 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/geometry.zi" "$work/layout_behavior.zi" "$work/layout_props.zi" \
    "$work/group.zi" "$work/use_layout.zi" "$work/scalar_layout.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/geometry.zi" "$work/layout_behavior.zi" "$work/layout_props.zi" "$work/group.zi" \
    "$work/use_layout.zi" "$work/scalar_layout.zi"
"$ziran" bundle --root "$work" --entry scalar_layout:Answer \
    -o "$work/scalar-layout.zx" "$work/geometry.zi" "$work/layout_behavior.zi" "$work/layout_props.zi" \
    "$work/group.zi" "$work/scalar_layout.zi"
test "$("$ziran" run "$work/scalar-layout.zx")" = 42
"$ziran" bundle --root "$work" --entry scalar_layout:Answer \
    -o "$work/scalar-layout-ir.zx" "$work/ir/geometry.zir" \
    "$work/ir/layout_behavior.zir" "$work/ir/group.zir" \
    "$work/ir/scalar_layout.zir"
cmp "$work/scalar-layout.zx" "$work/scalar-layout-ir.zx"
test "$("$ziran" run "$work/scalar-layout-ir.zx")" = 42
"$ziran" bundle --root "$work" --entry portable_layout:Answer \
    -o "$work/portable-layout.zx" "$work/geometry.zi" \
    "$work/layout_behavior.zi" "$work/layout_props.zi" "$work/group.zi" "$work/portable_layout.zi"
test "$("$ziran" run "$work/portable-layout.zx")" = 42
"$ziran" ir --root "$work" -o "$work/portable-ir" \
    "$work/geometry.zi" "$work/layout_behavior.zi" "$work/layout_props.zi" "$work/group.zi" \
    "$work/portable_layout.zi"
"$ziran" bundle --root "$work" --entry portable_layout:Answer \
    -o "$work/portable-layout-ir.zx" "$work/portable-ir/geometry.zir" \
    "$work/portable-ir/layout_behavior.zir" "$work/portable-ir/group.zir" \
    "$work/portable-ir/portable_layout.zir"
cmp "$work/portable-layout.zx" "$work/portable-layout-ir.zx"
test "$("$ziran" run "$work/portable-layout-ir.zx")" = 42
"$ziran" bundle --root "$work" --entry use_layout:Answer \
    -o "$work/full-layout.zx" "$work/geometry.zi" \
    "$work/layout_behavior.zi" "$work/layout_props.zi" "$work/group.zi" "$work/use_layout.zi"
test "$("$ziran" run "$work/full-layout.zx")" = 42
"$ziran" bundle --root "$work" --entry use_layout:Answer \
    -o "$work/full-layout-ir.zx" "$work/ir/geometry.zir" \
    "$work/ir/layout_behavior.zir" "$work/ir/group.zir" \
    "$work/ir/use_layout.zir"
cmp "$work/full-layout.zx" "$work/full-layout-ir.zx"
test "$("$ziran" run "$work/full-layout-ir.zx")" = 42
python3 - "$work/scalar-layout.zx" <<'PY'
from pathlib import Path
import sys
data = Path(sys.argv[1]).read_bytes()
assert b'geometry' not in data
assert b'GroupPolicyFor' not in data
assert b'BeginFlexCursor' not in data
PY

for input in source ir; do
    if test "$input" = source; then
        geometry="$work/geometry.zi"
        layout="$work/layout_behavior.zi"
        layout_props="$work/layout_props.zi"
        group="$work/group.zi"
        use_layout="$work/use_layout.zi"
    else
        geometry="$work/ir/geometry.zir"
        layout="$work/ir/layout_behavior.zir"
        layout_props="$work/ir/layout_props.zir"
        group="$work/ir/group.zir"
        use_layout="$work/ir/use_layout.zir"
    fi
    "$ziran" build --target=c --root "$work" \
        -o "$work/c-$input" "$geometry" "$layout" "$layout_props" "$group" "$use_layout"
    cat > "$work/c-$input/main.c" <<'EOF'
#include "use_layout.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -I"$ziran_include" -I"$work/c-$input" \
        "$work/c-$input/geometry.c" "$work/c-$input/math.c" "$work/c-$input/layout_behavior.c" \
        "$work/c-$input/group.c" \
        "$work/c-$input/use_layout.c" "$work/c-$input/main.c" \
        -o "$work/c-$input/app" -lm
    "$work/c-$input/app"


    "$ziran" build --target=go --pkg main --root "$work" \
        -o "$work/go-$input" "$geometry" "$layout" "$layout_props" "$group" "$use_layout"
    if grep -Fq 'github.com/waozixyz/kryon/go/kryon' \
        "$work/go-$input/geometry.go" "$work/go-$input/math.go" "$work/go-$input/layout_behavior.go" \
        "$work/go-$input/group.go" "$work/go-$input/use_layout.go"; then
        echo 'ordinary imported layout code pulled in the legacy Go runtime' >&2
        exit 1
    fi
    cat > "$work/go-$input/main.go" <<'EOF'
package main
func main() { if UseLayout_Answer() != 42 { panic("wrong layout result") } }
EOF
    # Run every generated file: std modules such as finite arrive with math.
    GO111MODULE=off go run "$work/go-$input"/*.go
done

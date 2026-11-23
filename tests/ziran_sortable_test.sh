#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
ziran_include=${ZIRAN_INCLUDE:-"$ziran_root/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/use_sortable.zi" <<'EOF'
#import "geometry"
#import "sortable_behavior"
#import "sortable_props"

// A, then group G holding B and C, then D: rows 40 high, 10 apart, and
// rows inside G indented by 10.
TestRows :: (rows: []Row) {
    depths: [6]s32 = .[0, 0, 1, 1, 0, 0]
    for index: cast(s32)0..5 {
        rows[index] = Row.{.bounds = Rectangle.{cast(float32)(depths[index] * 10), cast(float32)(index * 50),
            cast(float32)(200 - depths[index] * 10), 40.0}, .depth = depths[index], .end = -1}
    }
    rows[1].end = 4
    rows[4].closes = true
}

Same :: (drop: Drop, before: s32, depth: s32, into: s32, y: float32, x: float32) -> bool {
    return drop.valid && drop.before == before && drop.depth == depth && drop.into == into &&
        drop.line.y == y && drop.line.x == x
}

Targets :: () -> bool {
    rows: [6]Row
    TestRows(rows[:])
    // Between B and C, inside the group.
    if !Same(Target(rows[:], 0, 120.0, 10.0), 3, 1, -1, 145.0, 10.0) { return false }
    // The middle of the group's start row takes the row into the group.
    if !Same(Target(rows[:], 5, 70.0, 10.0), 4, 1, 1, 50.0, 0.0) { return false }
    // Above the group's end row is the group's end; below it the list's end.
    if !Same(Target(rows[:], 5, 195.0, 10.0), 4, 1, -1, 195.0, 10.0) { return false }
    if !Same(Target(rows[:], 5, 235.0, 10.0), 6, 0, -1, 240.0, 0.0) { return false }
    // A lifted group carries its rows and never lands inside itself.
    if !Same(Target(rows[:], 1, 10.0, 10.0), 0, 0, -1, 0.0, 0.0) { return false }
    if !Same(Target(rows[:], 1, 70.0, 10.0), 5, 0, -1, 145.0, 0.0) { return false }
    if Target(rows[:], 4, 10.0, 10.0).valid || Target(rows[:], 6, 10.0, 10.0).valid ||
        Target(rows[:], -1, 10.0, 10.0).valid {
        return false
    }
    rows[0].fixed = true
    return !Target(rows[:], 0, 120.0, 10.0).valid && Target(rows[:], 5, 10.0, 10.0).before == 0
}

Presses :: () -> bool {
    state: State
    row := ItemProps.{.row = 0, .bounds = Rectangle.{0.0, 0.0, 100.0, 40.0}}
    // A mouse lifts the row once it moves past the slop, and drops it on release.
    first := Track(*state, row, ItemPress.{.down = true, .x = 5.0, .y = 5.0})
    if first.lifted || !state.active || state.grab.y != 5.0 { return false }
    if Track(*state, row, ItemPress.{.down = true, .x = 5.0, .y = 9.0}).lifted { return false }
    if !Track(*state, row, ItemPress.{.down = true, .x = 5.0, .y = 20.0}).lifted { return false }
    if Lifted(state).y != 15.0 || AutoScroll(state, 0.0, 300.0, 10, 100, 25.0, 7) != 3 { return false }
    // Another row's press passes its click through while this one is held.
    other := row
    other.row = 1
    if !Track(*state, other, ItemPress.{.clicked = true}).clicked || state.row != 0 { return false }
    dropped := Track(*state, row, ItemPress.{.released = true, .clicked = true, .x = 5.0, .y = 60.0})
    if !dropped.dropped || dropped.clicked || dropped.pointer.y != 60.0 || state.active { return false }
    // A press that stays in place is a click.
    unused Track(*state, row, ItemPress.{.down = true, .x = 5.0, .y = 5.0})
    click := Track(*state, row, ItemPress.{.released = true, .clicked = true, .x = 5.0, .y = 5.0})
    if !click.clicked || click.dropped { return false }
    // On a touch screen a swipe before the hold is left to the scroll.
    touch := row
    touch.touch = true
    unused Track(*state, touch, ItemPress.{.down = true, .x = 5.0, .y = 5.0})
    if Track(*state, touch, ItemPress.{.down = true, .x = 5.0, .y = 30.0}).lifted || state.active { return false }
    // A hold lifts it; the row then follows the pointer anywhere.
    unused Track(*state, touch, ItemPress.{.down = true, .x = 5.0, .y = 5.0})
    if !Track(*state, touch, ItemPress.{.down = true, .held = true, .x = 6.0, .y = 6.0}).lifted { return false }
    if AutoScroll(state, 0.0, 300.0, 10, 100, 20.0, 7) != 3 { return false }
    if !Track(*state, touch, ItemPress.{.down = true, .x = 6.0, .y = 295.0}).lifted ||
        AutoScroll(state, 0.0, 300.0, 98, 100, 20.0, 7) != 100 {
        return false
    }
    held := Track(*state, touch, ItemPress.{.released = true, .x = 6.0, .y = 295.0})
    // A disabled row never takes a press.
    disabled := row
    disabled.disabled = true
    unused Track(*state, disabled, ItemPress.{.down = true, .x = 5.0, .y = 5.0})
    return held.dropped && !state.active
}

#program_export
Answer :: () -> s32 {
    if !Targets() { return 1 }
    if !Presses() { return 2 }
    return 42
}
EOF

for target in c go; do
    output="$work/$target"
    "$ziran" build --target="$target" --root "$work" \
        $kryon_ui_args --module-path "$ziran_root/std" -o "$output" "$work/use_sortable.zi"
    if test "$target" = go; then
        cat > "$output/sortable_test.go" <<'GO'
package ziran
import "testing"
func TestSortable(t *testing.T) { if r := UseSortable_Answer(); r != 42 { t.Fatal("sortable result", r) } }
GO
        GO111MODULE=off go test "$output"/*.go
    else
        cat > "$output/main.c" <<'C'
#include <stdio.h>
#include "use_sortable.h"
int main(void) { int r = Answer(); if (r != 42) fprintf(stderr, "sortable result %d\n", r); return r == 42 ? 0 : 1; }
C
        ${CC:-cc} -I"$ziran_include" -I"$output" "$output"/*.c -o "$output/app" -lm
        "$output/app"
    fi
done
"$ziran" bundle --root "$work" $kryon_ui_args --module-path "$ziran_root/std" \
    --entry use_sortable:Answer -o "$work/sortable.zx" "$work/use_sortable.zi"
test "$("$ziran" run "$work/sortable.zx")" = 42

# The drawing half also builds for hosted packages, where Press and Box
# reach the host.
cat > "$work/hosted_sortable.zi" <<'EOF'
Sortable :: #import "sortable";
#import "drawing_props"
#import "geometry"
#import "session"

#program_export
Answer :: (session: Session) -> s32 {
    state: Sortable.State
    rows: [1]Sortable.Row
    rows[0].bounds = Rectangle.{0.0, 0.0, 100.0, 40.0}
    rows[0].end = -1
    result := Sortable.Item(session, *state, Sortable.ItemProps.{.key = 9, .row = 0, .bounds = rows[0].bounds})
    drop := Sortable.Target(rows[:], 0, result.pointer.y, 8.0)
    Sortable.Indicator(session, 10, drop, Color.{.a = 255}, 3.0, 6.0)
    return ifx result.clicked then 1 else 0
}
EOF
"$ziran" check --root "$work" --define KRYON_HOSTED_UI $kryon_ui_args \
    --module-path "$ziran_root/std" "$work/hosted_sortable.zi"
echo "sortable: drop targets, presses and hosted build passed"

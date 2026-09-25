#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "session"
test_session: Session;
TestSession :: () -> Session {
    if !SessionValid(test_session) { test_session = SessionOpen() }
    return test_session
}

#import "geometry"
#import "tree"
#import "tree_input"
#import "widget_kind"

Build :: (reversed: bool, second_disabled: bool,
    first_present: bool) -> bool {
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    if reversed {
        second: s32 = TreeSubmit(TestSession(), cast(u64)12, 0, WidgetKindButton,
            Rectangle.{20.0, 20.0, 30.0, 30.0})
        TreeSetInteractive(TestSession(), second, second_disabled, false, 0)
    }
    if first_present {
        first: s32 = TreeSubmit(TestSession(), cast(u64)11, 0, WidgetKindButton,
            Rectangle.{10.0, 10.0, 30.0, 30.0})
        TreeSetInteractive(TestSession(), first, false, false, 0)
    }
    if !reversed {
        second: s32 = TreeSubmit(TestSession(), cast(u64)12, 0, WidgetKindButton,
            Rectangle.{20.0, 20.0, 30.0, 30.0})
        TreeSetInteractive(TestSession(), second, second_disabled, false, 0)
    }
    return TreeFinish(TestSession())
}

#program_export
Answer :: () -> s32 {
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    TreeSubmit(TestSession(), cast(u64)99, 0, WidgetKindButton,
        Rectangle.{10.0, 10.0, 30.0, 30.0})
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != -1 { return -20 }
    if !Build(false, false, true) || TreeCount(TestSession()) != 3 { return -1 }
    first: s32 = TreeChild(TestSession(), 0, cast(u64)11, WidgetKindButton)
    second: s32 = TreeChild(TestSession(), 0, cast(u64)12, WidgetKindButton)
    if first != 1 || second != 2 || TreeHitAt(TestSession(), 25.0, 25.0) != second ||
        TreeHitAt(TestSession(), 100.0, 25.0) != -1 { return -2 }
    identity: u64 = TreeNodeAt(TestSession(), second).identity_generation
    if TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, true, true, false}) != second ||
        !TreeHoveredAt(TestSession(), second) || !TreePressedAt(TestSession(), second) ||
        TreePressedAt(TestSession(), first) { return -3 }
    TreePointerUpdate(TestSession(), PointerFrame.{90.0, 90.0, true, false, false})
    if TreeHoveredAt(TestSession(), second) || TreePressedAt(TestSession(), second) { return -4 }
    TreePointerUpdate(TestSession(), PointerFrame.{90.0, 90.0, false, false, true})
    if TreeTakeActivationAt(TestSession(), second) { return -5 }

    TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(TestSession(), first) || !TreeTakeActivationAt(TestSession(), second) ||
        TreeTakeActivationAt(TestSession(), second) { return -6 }

    TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, true, true, false})
    if !Build(true, false, true) { return -7 }
    second = TreeChild(TestSession(), 0, cast(u64)12, WidgetKindButton)
    first = TreeChild(TestSession(), 0, cast(u64)11, WidgetKindButton)
    if second != 1 || first != 2 ||
        TreeNodeAt(TestSession(), second).identity_generation != identity { return -8 }
    TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(TestSession(), first) || TreeTakeActivationAt(TestSession(), second) {
        return -9
    }

    TreePointerUpdate(TestSession(), PointerFrame.{45.0, 25.0, true, true, false})
    if !Build(true, false, true) { return -10 }
    TreePointerUpdate(TestSession(), PointerFrame.{45.0, 25.0, false, false, true})
    if !TreeTakeActivationAt(TestSession(), second) { return -11 }

    if !Build(false, true, true) { return -12 }
    first = TreeChild(TestSession(), 0, cast(u64)11, WidgetKindButton)
    second = TreeChild(TestSession(), 0, cast(u64)12, WidgetKindButton)
    if TreeHitAt(TestSession(), 25.0, 25.0) != -1 ||
        TreeHitAt(TestSession(), 15.0, 15.0) != first { return -13 }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(TestSession(), first) || TreeTakeActivationAt(TestSession(), second) {
        return -14
    }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    if !Build(false, true, false) { return -15 }
    TreePointerUpdate(TestSession(), PointerFrame.{25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(TestSession(), TreeChild(TestSession(), 0, cast(u64)12,
        WidgetKindButton)) { return -16 }

    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    card: s32 = TreeSubmit(TestSession(), cast(u64)13, 0, WidgetKindCard,
        Rectangle.{10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), card, false, false, 0)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != -1 { return -17 }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    card = TreeSubmit(TestSession(), cast(u64)13, 0, WidgetKindCard,
        Rectangle.{10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), card, false, false, 5)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != 1 { return -18 }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(TestSession(), 1) { return -19 }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    checkbox: s32 = TreeSubmit(TestSession(), cast(u64)14, 0, WidgetKindCheckbox,
        Rectangle.{10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), checkbox, false, false, 0)
    toggle: s32 = TreeSubmit(TestSession(), cast(u64)15, 0, WidgetKindToggle,
        Rectangle.{45.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), toggle, false, false, 0)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != 1 ||
        TreeHitAt(TestSession(), 50.0, 15.0) != 2 { return -21 }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(TestSession(), checkbox) || TreeTakeActivationAt(TestSession(), toggle) {
        return -22
    }
    TreePointerUpdate(TestSession(), PointerFrame.{50.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{50.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(TestSession(), toggle) { return -23 }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    checkbox = TreeSubmit(TestSession(), cast(u64)14, 0, WidgetKindCheckbox,
        Rectangle.{10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), checkbox, true, false, 0)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != -1 {
        return -24
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    slider: s32 = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, false, false, 0)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != slider {
        return -25
    }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{150.0, 15.0, true, false, false})
    drag: PointerDrag = TreeTakeDragAt(TestSession(), slider)
    if !drag.active || !drag.started || drag.ended ||
        drag.x != 150.0 || drag.y != 15.0 ||
        drag.grab_x != 5.0 || drag.grab_y != 5.0 { return -26 }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    TreeSubmit(TestSession(), cast(u64)17, 0, WidgetKindText,
        Rectangle.{0.0, 0.0, 5.0, 5.0})
    slider = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, false, false, 0)
    if !TreeFinish(TestSession()) || slider != 2 { return -27 }
    drag = TreeTakeDragAt(TestSession(), slider)
    if !drag.active || drag.started || drag.ended ||
        drag.x != 150.0 || drag.grab_x != 5.0 ||
        drag.grab_y != 5.0 { return -28 }
    TreePointerUpdate(TestSession(), PointerFrame.{150.0, 15.0, false, false, true})
    drag = TreeTakeDragAt(TestSession(), slider)
    if drag.active || drag.started || !drag.ended ||
        drag.x != 150.0 || drag.y != 15.0 ||
        drag.grab_x != 5.0 || drag.grab_y != 5.0 { return -29 }
    if TreeTakeDragAt(TestSession(), slider).ended || TreeTakeActivationAt(TestSession(), slider) {
        return -30
    }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreePointerUpdate(TestSession(), PointerFrame.{30.0, 15.0, false, false, true})
    drag = TreeTakeDragAt(TestSession(), slider)
    if !drag.started || !drag.ended || drag.x != 30.0 ||
        TreeTakeActivationAt(TestSession(), slider) { return -31 }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, true, false, 0)
    if !TreeFinish(TestSession()) || TreeHitAt(TestSession(), 15.0, 15.0) != -1 {
        return -32
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, false, false, 0)
    if !TreeFinish(TestSession()) { return -33 }
    TreePointerUpdate(TestSession(), PointerFrame.{15.0, 15.0, true, true, false})
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, true, false, 0)
    if !TreeFinish(TestSession()) || TreeTakeDragAt(TestSession(), slider).active {
        return -34
    }
    TreePointerUpdate(TestSession(), PointerFrame.{30.0, 15.0, false, false, true})
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit(TestSession(), cast(u64)16, 0, WidgetKindSlider,
        Rectangle.{10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(TestSession(), slider, false, false, 0)
    if !TreeFinish(TestSession()) || TreeTakeDragAt(TestSession(), slider).ended {
        return -35
    }
    TreeStart(TestSession(), cast(u64)1, Rectangle.{0.0, 0.0, 100.0, 100.0})
    outer: s32 = TreeSubmit(TestSession(), cast(u64)20, 0, WidgetKindCard,
        Rectangle.{0.0, 0.0, 90.0, 90.0})
    TreeSetInteractive(TestSession(), outer, false, false, 1)
    TreeSetChildClip(TestSession(), outer, Rectangle.{20.0, 20.0, 40.0, 40.0})
    child: s32 = TreeSubmit(TestSession(), cast(u64)21, outer, WidgetKindButton,
        Rectangle.{10.0, 10.0, 70.0, 70.0})
    TreeSetInteractive(TestSession(), child, false, false, 0)
    nested: s32 = TreeSubmit(TestSession(), cast(u64)22, outer, WidgetKindStack,
        Rectangle.{15.0, 15.0, 50.0, 50.0})
    TreeSetChildClip(TestSession(), nested, Rectangle.{50.0, 50.0, 30.0, 30.0})
    grandchild: s32 = TreeSubmit(TestSession(), cast(u64)23, nested, WidgetKindButton,
        Rectangle.{45.0, 45.0, 30.0, 30.0})
    TreeSetInteractive(TestSession(), grandchild, false, false, 0)
    if !TreeFinish(TestSession()) ||
        TreeHitAt(TestSession(), 12.0, 12.0) != outer ||
        TreeHitAt(TestSession(), 25.0, 25.0) != child ||
        TreeHitAt(TestSession(), 35.0, 35.0) != child ||
        TreeHitAt(TestSession(), 45.0, 45.0) != child ||
        TreeHitAt(TestSession(), 55.0, 55.0) != grandchild ||
        TreeHitAt(TestSession(), 65.0, 65.0) != outer ||
        !TreeNodeAt(TestSession(), child).clipped ||
        TreeNodeAt(TestSession(), child).clip.width != 40.0 ||
        TreeNodeAt(TestSession(), grandchild).clip.width != 10.0 {
        return -36
    }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/ui" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"

cat > "$work/portable.c" <<'C'
#include "ziran_host.h"
#include <assert.h>
int main(int argc, char **argv) {
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL && BundleCapabilityCount(bundle) == 0);
    long long result = 0;
    int has_result = 0;
    assert(BundleRun(bundle, NULL, 0, &result, &has_result));
    assert(has_result && result == 42);
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/../ziran/include" \
    "$work/portable.c" "$ziran_lib" ${VM_LDFLAGS:-} -o "$work/portable"
"$work/portable" "$work/source.zib"
"$work/portable" "$work/saved.zib"

for target in c cpp go; do
    output=$work/$target
    "$ziran" build --target="$target" --root "$work" \
        --module-path "$repo/src/ui" -o "$output" "$work/app.zi"
    if test "$target" = c; then
        cat > "$output/main.c" <<'C'
#include "app.h"
#include <assert.h>
int main(void) { assert(Answer() == 42); return 0; }
C
        "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.c -o "$output/app"
        "$output/app"
    elif test "$target" = cpp; then
        cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
#include <cassert>
int main() { assert(Answer() == 42); return 0; }
CPP
        "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" -I"$output" \
            "$output"/*.cpp -o "$output/app"
        "$output/app"
    else
        cat > "$output/tree_input_test.go" <<'GO'
package ziran
import "testing"
func TestTreePointer(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("retained pointer routing") }
}
GO
        GO111MODULE=off go test "$output"/*.go
    fi
done

#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "geometry"
#import "tree"
#import "tree_input"
#import "widget_kind"

Build :: (reversed: bool, second_disabled: bool,
    first_present: bool) -> bool {
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    if reversed {
        second: i32 = TreeSubmit((u64)12, 0, WidgetKindButton,
            (Rectangle){20.0, 20.0, 30.0, 30.0})
        TreeSetInteractive(second, second_disabled, false, 0)
    }
    if first_present {
        first: i32 = TreeSubmit((u64)11, 0, WidgetKindButton,
            (Rectangle){10.0, 10.0, 30.0, 30.0})
        TreeSetInteractive(first, false, false, 0)
    }
    if !reversed {
        second: i32 = TreeSubmit((u64)12, 0, WidgetKindButton,
            (Rectangle){20.0, 20.0, 30.0, 30.0})
        TreeSetInteractive(second, second_disabled, false, 0)
    }
    return TreeFinish()
}

Answer :: () -> i32 #export {
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    TreeSubmit((u64)99, 0, WidgetKindButton,
        (Rectangle){10.0, 10.0, 30.0, 30.0})
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != -1 { return -20 }
    if !Build(false, false, true) || TreeCount() != 3 { return -1 }
    first: i32 = TreeFind((u64)11, (u64)1, WidgetKindButton)
    second: i32 = TreeFind((u64)12, (u64)1, WidgetKindButton)
    if first != 1 || second != 2 || TreeHitAt(25.0, 25.0) != second ||
        TreeHitAt(100.0, 25.0) != -1 { return -2 }
    identity: i32 = TreeNodeAt(second).identity_generation
    if TreePointerUpdate((PointerFrame){25.0, 25.0, true, true, false}) != second ||
        !TreeHoveredAt(second) || !TreePressedAt(second) ||
        TreePressedAt(first) { return -3 }
    TreePointerUpdate((PointerFrame){90.0, 90.0, true, false, false})
    if TreeHoveredAt(second) || TreePressedAt(second) { return -4 }
    TreePointerUpdate((PointerFrame){90.0, 90.0, false, false, true})
    if TreeTakeActivationAt(second) { return -5 }

    TreePointerUpdate((PointerFrame){25.0, 25.0, true, true, false})
    TreePointerUpdate((PointerFrame){25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(first) || !TreeTakeActivationAt(second) ||
        TreeTakeActivationAt(second) { return -6 }

    TreePointerUpdate((PointerFrame){25.0, 25.0, true, true, false})
    if !Build(true, false, true) { return -7 }
    second = TreeFind((u64)12, (u64)1, WidgetKindButton)
    first = TreeFind((u64)11, (u64)1, WidgetKindButton)
    if second != 1 || first != 2 ||
        TreeNodeAt(second).identity_generation != identity { return -8 }
    TreePointerUpdate((PointerFrame){25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(first) || TreeTakeActivationAt(second) {
        return -9
    }

    TreePointerUpdate((PointerFrame){45.0, 25.0, true, true, false})
    if !Build(true, false, true) { return -10 }
    TreePointerUpdate((PointerFrame){45.0, 25.0, false, false, true})
    if !TreeTakeActivationAt(second) { return -11 }

    if !Build(false, true, true) { return -12 }
    first = TreeFind((u64)11, (u64)1, WidgetKindButton)
    second = TreeFind((u64)12, (u64)1, WidgetKindButton)
    if TreeHitAt(25.0, 25.0) != -1 ||
        TreeHitAt(15.0, 15.0) != first { return -13 }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(first) || TreeTakeActivationAt(second) {
        return -14
    }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    if !Build(false, true, false) { return -15 }
    TreePointerUpdate((PointerFrame){25.0, 25.0, false, false, true})
    if TreeTakeActivationAt(TreeFind((u64)12, (u64)1,
        WidgetKindButton)) { return -16 }

    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    card: i32 = TreeSubmit((u64)13, 0, WidgetKindCard,
        (Rectangle){10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(card, false, false, 0)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != -1 { return -17 }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    card = TreeSubmit((u64)13, 0, WidgetKindCard,
        (Rectangle){10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(card, false, false, 5)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != 1 { return -18 }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(1) { return -19 }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    checkbox: i32 = TreeSubmit((u64)14, 0, WidgetKindCheckbox,
        (Rectangle){10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(checkbox, false, false, 0)
    toggle: i32 = TreeSubmit((u64)15, 0, WidgetKindToggle,
        (Rectangle){45.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(toggle, false, false, 0)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != 1 ||
        TreeHitAt(50.0, 15.0) != 2 { return -21 }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){15.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(checkbox) || TreeTakeActivationAt(toggle) {
        return -22
    }
    TreePointerUpdate((PointerFrame){50.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){50.0, 15.0, false, false, true})
    if !TreeTakeActivationAt(toggle) { return -23 }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    checkbox = TreeSubmit((u64)14, 0, WidgetKindCheckbox,
        (Rectangle){10.0, 10.0, 30.0, 30.0})
    TreeSetInteractive(checkbox, true, false, 0)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != -1 {
        return -24
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    slider: i32 = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, false, false, 0)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != slider {
        return -25
    }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){150.0, 15.0, true, false, false})
    drag: PointerDrag = TreeTakeDragAt(slider)
    if !drag.active || !drag.started || drag.ended ||
        drag.x != 150.0 || drag.y != 15.0 ||
        drag.grab_y != 5.0 { return -26 }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    TreeSubmit((u64)17, 0, WidgetKindText,
        (Rectangle){0.0, 0.0, 5.0, 5.0})
    slider = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, false, false, 0)
    if !TreeFinish() || slider != 2 { return -27 }
    drag = TreeTakeDragAt(slider)
    if !drag.active || drag.started || drag.ended ||
        drag.x != 150.0 || drag.grab_y != 5.0 { return -28 }
    TreePointerUpdate((PointerFrame){150.0, 15.0, false, false, true})
    drag = TreeTakeDragAt(slider)
    if drag.active || drag.started || !drag.ended ||
        drag.x != 150.0 || drag.y != 15.0 ||
        drag.grab_y != 5.0 { return -29 }
    if TreeTakeDragAt(slider).ended || TreeTakeActivationAt(slider) {
        return -30
    }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreePointerUpdate((PointerFrame){30.0, 15.0, false, false, true})
    drag = TreeTakeDragAt(slider)
    if !drag.started || !drag.ended || drag.x != 30.0 ||
        TreeTakeActivationAt(slider) { return -31 }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, true, false, 0)
    if !TreeFinish() || TreeHitAt(15.0, 15.0) != -1 {
        return -32
    }
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, false, false, 0)
    if !TreeFinish() { return -33 }
    TreePointerUpdate((PointerFrame){15.0, 15.0, true, true, false})
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, true, false, 0)
    if !TreeFinish() || TreeTakeDragAt(slider).active {
        return -34
    }
    TreePointerUpdate((PointerFrame){30.0, 15.0, false, false, true})
    TreeStart((u64)1, (Rectangle){0.0, 0.0, 100.0, 100.0})
    slider = TreeSubmit((u64)16, 0, WidgetKindSlider,
        (Rectangle){10.0, 10.0, 60.0, 20.0})
    TreeSetInteractive(slider, false, false, 0)
    if !TreeFinish() || TreeTakeDragAt(slider).ended {
        return -35
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
    "$ziran" build --target="$target" --strict --root "$work" \
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

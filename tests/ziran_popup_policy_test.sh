#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/geometry.zi" "$work/geometry.zi"
cp "$repo/src/ui/popup_policy.zi" "$work/popup_policy.zi"
cat > "$work/use_popup.zi" <<'EOF'
#module "use_popup"
#import "geometry"
#import "popup_policy"

Answer :: () -> i32 #export {
    invalid: PopupDecision = PopupDecisionFor((u32)8, false)
    if invalid.valid { return 0 }
    context: PopupDecision = PopupDecisionFor((u32)4, false)
    if !context.valid || !context.context || !context.requires_trigger ||
        context.tooltip { return 0 }
    input: PopupFrameInput
    input.id = 7
    input.bounds.x = 10.0
    input.bounds.y = 20.0
    input.bounds.width = 100.0
    input.bounds.height = 80.0
    input.trigger = input.bounds
    input.has_open = true
    input.mouse.x = 30.0
    input.mouse.y = 40.0
    input.right_released = true
    state: PopupLifecycle = PopupLifecycleBegin((u32)4, input)
    if !state.eligible || !state.context_opened || !state.visible ||
        state.input_bounds.width != 100.0 { return 0 }
    dismissed: PopupLifecycle = PopupLifecycleRelease(state, true, false, false)
    if dismissed.visible || !dismissed.consume_release ||
        !dismissed.close_input { return 0 }
    modal: PopupDecision = PopupDecisionFor((u32)2, false)
    if PopupBackdropAlpha(modal) != 180 { return 0 }
    bounds: Rectangle = PopupInputBounds(modal, input.bounds, 640.0, 480.0)
    if bounds.x != 0.0 || bounds.y != 0.0 ||
        bounds.width != 640.0 || bounds.height != 480.0 { return 0 }
    tooltip: PopupDecision = PopupDecisionFor((u32)1, false)
    if !PopupTooltipVisible(tooltip, false, true) ||
        PopupTooltipVisible(tooltip, true, true) { return 0 }
    opened: PopupOpenResult = PopupOpenFor(false, true, false, true)
    if !opened.open || !opened.changed { return 0 }
    origin: Vector2 = PopupMenuBarOrigin(input.bounds, bounds)
    if origin.x != 10.0 || origin.y != 480.0 { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/geometry.zi" \
    "$work/popup_policy.zi" "$work/use_popup.zi"
"$ziran" ir --root "$work" -o "$work/ir" "$work/geometry.zi" \
    "$work/popup_policy.zi" "$work/use_popup.zi"
"$ziran" bundle --root "$work" --entry use_popup:Answer \
    -o "$work/source.zib" "$work/geometry.zi" \
    "$work/popup_policy.zi" "$work/use_popup.zi"
"$ziran" bundle --root "$work" --entry use_popup:Answer \
    -o "$work/ir.zib" "$work/ir/geometry.zir" \
    "$work/ir/popup_policy.zir" "$work/ir/use_popup.zir"
cmp "$work/source.zib" "$work/ir.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/ir.zib")" = 42

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    for target in c cpp go; do
        output="$work/$target-$input"
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                -o "$output" "$input_dir/geometry.$extension" \
                "$input_dir/popup_policy.$extension" \
                "$input_dir/use_popup.$extension"
            if rg -q 'github.com/waozixyz/kryon' "$output"; then
                echo 'popup policy imported the old Kryon Go runtime' >&2
                exit 1
            fi
            cat > "$output/main.go" <<'GO'
package main
func main() { if UsePopup_Answer() != 42 { panic("wrong popup result") } }
GO
            GO111MODULE=off go run "$output/geometry.go" \
                "$output/popup_policy.go" "$output/use_popup.go" \
                "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --strict --root "$work" -o "$output" \
                "$input_dir/geometry.$extension" \
                "$input_dir/popup_policy.$extension" \
                "$input_dir/use_popup.$extension"
            cat > "$output/main.c" <<'C'
#include "use_popup.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/geometry.c" "$output/popup_policy.c" \
                "$output/use_popup.c" "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --strict --root "$work" -o "$output" \
                "$input_dir/geometry.$extension" \
                "$input_dir/popup_policy.$extension" \
                "$input_dir/use_popup.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_popup.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/geometry.cpp" "$output/popup_policy.cpp" \
                "$output/use_popup.cpp" "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done

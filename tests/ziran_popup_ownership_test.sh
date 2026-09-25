#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/src/ui/popup_ownership.zi" "$work/popup_ownership.zi"
cat > "$work/use_ownership.zi" <<'EOF'
#import "popup_ownership"

#program_export
Answer :: () -> s32 {
    high: u64 = cast(u64)0x8000000000000000
    latest: u64 = cast(u64)0xffffffffffffffff
    order: PopupOrder
    order.phase = 2
    above: PopupOrder = PopupOrderAdvance(order, true, true,
        false, true, latest, high)
    if !above.done || !above.above { return 0 }
    below: PopupOrder = PopupOrderAdvance(order, true, true,
        false, true, high, latest)
    if !below.done || below.above { return 0 }
    if PopupOwnerRetired(true, high, high) { return 0 }
    if !PopupOwnerRetired(true, high, latest) { return 0 }
    if !PopupInputCaptured(true, false, false, false) { return 0 }
    if !PopupOwnerAlive(true, true) { return 0 }
    ancestry: PopupAncestry = PopupAncestryAdvance(true, true, true)
    if !ancestry.done || !ancestry.contains { return 0 }
    focus: PopupFocusState
    focus = PopupFocusInitialize(focus, false, 5)
    if !focus.autofocus || focus.restore_focus != 5 { return 0 }
    acquired: PopupFocusDecision = PopupFocusRegister(focus, 7, 0,
        true, true, false)
    if !acquired.acquire || !acquired.state.has_last_focus ||
        acquired.state.last_focus != 7 { return 0 }
    if !PopupFocusRestore(acquired.state, 7, true, false) { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/popup_ownership.zi" \
    "$work/use_ownership.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/popup_ownership.zi" "$work/use_ownership.zi"
"$ziran" bundle --root "$work" --entry use_ownership:Answer \
    -o "$work/source.zib" "$work/popup_ownership.zi" \
    "$work/use_ownership.zi"
"$ziran" bundle --root "$work" --entry use_ownership:Answer \
    -o "$work/ir.zib" "$work/ir/popup_ownership.zir" \
    "$work/ir/use_ownership.zir"
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
            "$ziran" build --target=go --pkg main --root "$work" \
                -o "$output" "$input_dir/popup_ownership.$extension" \
                "$input_dir/use_ownership.$extension"
            if rg -q 'github.com/waozixyz/kryon' "$output"; then
                echo 'popup ownership imported the old Kryon Go runtime' >&2
                exit 1
            fi
            cat > "$output/main.go" <<'GO'
package main
func main() { if UseOwnership_Answer() != 42 { panic("wrong ownership result") } }
GO
            GO111MODULE=off go run "$output/popup_ownership.go" \
                "$output/use_ownership.go" "$output/main.go"
        elif test "$target" = c; then
            "$ziran" build --target=c --root "$work" -o "$output" \
                "$input_dir/popup_ownership.$extension" \
                "$input_dir/use_ownership.$extension"
            cat > "$output/main.c" <<'C'
#include "use_ownership.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            ${CC:-cc} -I"$ziran_include" -I"$output" \
                "$output/popup_ownership.c" "$output/use_ownership.c" \
                "$output/main.c" -o "$output/app"
            "$output/app"
        else
            "$ziran" build --target=cpp --root "$work" -o "$output" \
                "$input_dir/popup_ownership.$extension" \
                "$input_dir/use_ownership.$extension"
            cat > "$output/main.cpp" <<'CPP'
#include "use_ownership.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            ${CXX:-c++} -I"$ziran_include" -I"$output" \
                "$output/popup_ownership.cpp" "$output/use_ownership.cpp" \
                "$output/main.cpp" -o "$output/app"
            "$output/app"
        fi
    done
done

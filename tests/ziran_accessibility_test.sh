#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_include=${ZIRAN_INCLUDE:-"$repo/../ziran/include"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cp "$repo/zi/widget_kind.zi" "$work/widget_kind.zi"
cp "$repo/zi/accessibility_props.zi" "$work/accessibility_props.zi"
cp "$repo/zi/accessibility_policy.zi" "$work/accessibility_policy.zi"
cat > "$work/use_accessibility.zi" <<'EOF'
#module "use_accessibility"
#import "widget_kind"
#import "accessibility_props"
#import "accessibility_policy"

Answer :: () -> i32 #export {
    button: u32 = AccessibilityActionsFor((i32)WidgetKindButton, 7,
        false, false, false)
    if button != (u32)3 { return 0 }
    if !AccessibilityActionAllowed(button,
        (AccessibilityAction)AccessibilityActionActivate) { return 0 }
    if AccessibilityActionsFor((i32)WidgetKindButton, 0,
        false, false, false) != (u32)0 { return 0 }
    field: u32 = AccessibilityActionsFor((i32)WidgetKindTextField, 8,
        false, false, false)
    if field != (u32)13 { return 0 }
    if AccessibilityActionsFor((i32)WidgetKindListBox, 9,
        false, false, true) != (u32)1 { return 0 }
    if !AccessibilityItemSelectionFor(false, 3, 3,
        (AccessibilityAction)AccessibilityActionSelectItem) { return 0 }
    if AccessibilitySingleSelectionFor(3, 3,
        (AccessibilityAction)AccessibilityActionDeselectItem) != -1 { return 0 }
    if !AccessibilityValueCodepointAllowed(10, true) { return 0 }
    if AccessibilityValueCodepointAllowed(10, false) { return 0 }
    if !AccessibilityValueFits(5, 3, 6, 3) { return 0 }
    if AccessibilityValueFits(6, 3, 6, 3) { return 0 }
    return 42
}
EOF

"$ziran" check --root "$work" "$work/widget_kind.zi" \
    "$work/accessibility_props.zi" "$work/accessibility_policy.zi" \
    "$work/use_accessibility.zi"
"$ziran" ir --root "$work" -o "$work/ir" \
    "$work/widget_kind.zi" "$work/accessibility_props.zi" \
    "$work/accessibility_policy.zi" "$work/use_accessibility.zi"

for input in source ir; do
    if test "$input" = source; then
        extension=zi
        input_dir=$work
    else
        extension=zir
        input_dir=$work/ir
    fi
    "$ziran" build --target=c --strict --root "$work" -o "$work/c-$input" \
        "$input_dir/widget_kind.$extension" \
        "$input_dir/accessibility_props.$extension" \
        "$input_dir/accessibility_policy.$extension" \
        "$input_dir/use_accessibility.$extension"
    cat > "$work/c-$input/main.c" <<'EOF'
#include "use_accessibility.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
EOF
    ${CC:-cc} -I"$ziran_include" -I"$work/c-$input" \
        "$work/c-$input/widget_kind.c" \
        "$work/c-$input/accessibility_props.c" \
        "$work/c-$input/accessibility_policy.c" \
        "$work/c-$input/use_accessibility.c" \
        "$work/c-$input/main.c" -o "$work/c-$input/app"
    "$work/c-$input/app"

    "$ziran" build --target=cpp --strict --root "$work" -o "$work/cpp-$input" \
        "$input_dir/widget_kind.$extension" \
        "$input_dir/accessibility_props.$extension" \
        "$input_dir/accessibility_policy.$extension" \
        "$input_dir/use_accessibility.$extension"
    cat > "$work/cpp-$input/main.cpp" <<'EOF'
#include "use_accessibility.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
EOF
    ${CXX:-c++} -I"$ziran_include" -I"$work/cpp-$input" \
        "$work/cpp-$input/widget_kind.cpp" \
        "$work/cpp-$input/accessibility_props.cpp" \
        "$work/cpp-$input/accessibility_policy.cpp" \
        "$work/cpp-$input/use_accessibility.cpp" \
        "$work/cpp-$input/main.cpp" -o "$work/cpp-$input/app"
    "$work/cpp-$input/app"

    "$ziran" build --target=go --strict --pkg main --root "$work" \
        -o "$work/go-$input" \
        "$input_dir/widget_kind.$extension" \
        "$input_dir/accessibility_props.$extension" \
        "$input_dir/accessibility_policy.$extension" \
        "$input_dir/use_accessibility.$extension"
    if grep -Fq 'github.com/waozixyz/kryon' \
        "$work/go-$input/widget_kind.go" \
        "$work/go-$input/accessibility_props.go" \
        "$work/go-$input/accessibility_policy.go" \
        "$work/go-$input/use_accessibility.go"; then
        echo 'ordinary accessibility imports pulled in the old Kryon Go runtime' >&2
        exit 1
    fi
    cat > "$work/go-$input/main.go" <<'EOF'
package main
func main() { if UseAccessibility_Answer() != 42 { panic("wrong result") } }
EOF
    GO111MODULE=off go run "$work/go-$input/widget_kind.go" \
        "$work/go-$input/accessibility_props.go" \
        "$work/go-$input/accessibility_policy.go" \
        "$work/go-$input/use_accessibility.go" \
        "$work/go-$input/main.go"
done

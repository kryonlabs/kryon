#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "preference_policy"
#import "theme"

#program_export
Answer :: () -> s32 {
    theme: ThemePreference
    theme.theme_id = 7
    theme.source = cast(ThemeSource)ThemeSourceApp
    theme.mode = cast(ThemePolicy)ThemePolicyLight
    theme.system_dark = true
    choice: ThemeDecision = ThemeDecisionFor(theme)
    if choice.theme_id != 7 || choice.dark ||
        choice.source != cast(ThemeSource)ThemeSourceApp { return -1 }
    theme.mode = cast(ThemePolicy)ThemePolicySystem
    choice = ThemeDecisionFor(theme)
    if !choice.dark { return -2 }
    theme.mode = cast(ThemePolicy)ThemePolicyDark
    theme.system_dark = false
    choice = ThemeDecisionFor(theme)
    if !choice.dark { return -3 }
    theme.source = cast(ThemeSource)99
    theme.mode = cast(ThemePolicy)99
    choice = ThemeDecisionFor(theme)
    if choice.source != cast(ThemeSource)ThemeSourceSystem ||
        choice.mode != cast(ThemePolicy)ThemePolicySystem ||
        choice.dark { return -4 }

    orientation: OrientationPreference
    orientation.mode = 1
    orientation.portrait_mode = 1
    orientation.landscape_mode = 2
    plan: OrientationDecision = OrientationDecisionFor(orientation,
        800, 400, true, true)
    if plan.action != cast(OrientationAction)OrientationActionResize ||
        plan.width != 400 || plan.height != 800 || plan.mode != 1 {
        return -5
    }
    orientation.mode = 2
    plan = OrientationDecisionFor(orientation, 400, 800, true, true)
    if plan.action != cast(OrientationAction)OrientationActionResize ||
        plan.width != 800 || plan.height != 400 { return -6 }
    plan = OrientationDecisionFor(orientation, 800, 400, true, true)
    if plan.action != cast(OrientationAction)OrientationActionSetMode {
        return -7
    }
    plan = OrientationDecisionFor(orientation, 0, 400, true, true)
    if plan.action != cast(OrientationAction)OrientationActionSetMode {
        return -8
    }
    plan = OrientationDecisionFor(orientation, 400, 800, false, false)
    if plan.action != cast(OrientationAction)OrientationActionNone {
        return -9
    }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" \
    -o "$work/ir" "$work/app.zi"
for input in source saved; do
    if test "$input" = source; then
        source=$work/app.zi
        root=$work
        module_path=$repo/src/ui
    else
        source=$work/ir/app.zir
        root=$work/ir
        module_path=$work/ir
    fi
    "$ziran" bundle --root "$root" --module-path "$module_path" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        "$ziran" build --target="$target" --strict \
            --root "$root" --module-path "$module_path" \
            -o "$output" "$source"
        if test "$target" = c; then
            cat > "$output/main.c" <<'C'
#include "app.h"
int main(void) { return Answer() == 42 ? 0 : 1; }
C
            "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            "$output/app"
        elif test "$target" = cpp; then
            cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
int main() { return Answer() == 42 ? 0 : 1; }
CPP
            "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            "$output/app"
        else
            cat > "$output/preference_test.go" <<'GO'
package ziran
import "testing"
func TestPreference(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("preference") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

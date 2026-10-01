#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
. "$repo/tests/toolchain.sh"
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "preference_policy"
#import "theme"

using ThemeSource;
using OrientationAction;
using ThemePolicy;

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

#program_export
main :: () -> s32 {
    if Answer() != 42 { return 1 }
    return 0
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/ui" --module-path "$ziran_root/std" \
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
    "$ziran" bundle --root "$root" --module-path "$module_path" --module-path "$ziran_root/std" \
        --entry app:Answer -o "$work/$input.zib" "$source"
    test "$("$ziran" run "$work/$input.zib")" = 42
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --exe --entry app:main \
                --root "$root" --module-path "$module_path" \
                --module-path "$ziran_root/std" -o "$output" "$source"
            env -u DISPLAY -u WAYLAND_DISPLAY GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$root" \
                --module-path "$module_path" \
                --module-path "$ziran_root/std" -o "$output" "$source"
        fi
        if test "$target" = c; then
            "${CC:-cc}" -std=c11 -I"$ziran_root/include" \
                -I"$output" "$output"/*.c -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        elif test "$target" = cpp; then
            "${CXX:-c++}" -std=c++17 -I"$ziran_root/include" \
                -I"$output" "$output"/*.cpp -o "$output/app"
            env -u DISPLAY -u WAYLAND_DISPLAY "$output/app"
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

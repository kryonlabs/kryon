#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "drawing_props"
#import "kss_parser"
#import "progress"
#import "progress_style"
#import "style_parse"
#import "style"
#import "style_sheet"

using KssStatus;

#program_export
Answer :: () -> s32 {
    parsed: StyleRulesParse = BeginStyleRules(
        "@pack demo; @import <base>; Progress[role=Track] { background: #112233; border: #99aabb; border-width: 2; radius: 5; }",
        "demo.kss", KssDefaultEnvironment())
    parsed = ParseStyleRules(parsed)
    if parsed.parser.status != cast(s32)KssStatusNeedImport ||
        parsed.rules.count != 0 { return 0 }
    parsed = ProvideStyleRulesImport(parsed,
        "Progress[role=Fill] { background: #556677; }", "base")
    parsed = ParseStyleRules(parsed)
    if parsed.parser.status != cast(s32)KssStatusDone ||
        parsed.overflow || parsed.rules.count != 2 { return 0 }
    if !InstallParsedStyleRules(parsed) { return 0 }
    defaults: ProgressFaces
    faces: ProgressFaces = ProgressFacesFor(ActiveStyleRules(), 0, defaults)
    if faces.track.value.background != cast(u32)0x112233ff ||
        faces.track.value.border != cast(u32)0x99aabbff ||
        faces.track.value.border_width != 2.0 ||
        faces.track.value.radius != 5.0 ||
        faces.fill.value.background != cast(u32)0x556677ff {
        return 0
    }
    parsed = BeginStyleRules(
        "@pack colors; tokens { color { canvas: #112233; } }",
        "colors.kss", KssDefaultEnvironment())
    parsed.parser = KssAddColorOverride(parsed.parser, "canvas",
        cast(u32)0xaabbccdd)
    parsed = ParseStyleRules(parsed)
    if !InstallParsedStyleRules(parsed) { return 0 }
    color: Color = StyleTokenColor("canvas")
    if color.r != 0xaa || color.g != 0xbb || color.b != 0xcc ||
        color.a != 0xdd { return 0 }
    color = StyleTokenColorOr("missing", Color.{1, 2, 3, 4})
    if color.r != 1 || color.g != 2 || color.b != 3 || color.a != 4 {
        return 0
    }
    parsed = BeginStyleRules(
        "@pack replacement; tokens { color { text: #445566; } }",
        "replacement.kss", KssDefaultEnvironment())
    parsed = ParseStyleRules(parsed)
    if !InstallParsedStyleRules(parsed) { return 0 }
    color = StyleTokenColor("canvas")
    if color.r != 0 || color.g != 0 || color.b != 0 || color.a != 255 {
        return 0
    }
    parsed = BeginStyleRules("Progress { background: #112233; }",
        "full.kss", KssDefaultEnvironment())
    parsed.rules.count = 320
    parsed = StepStyleRules(parsed)
    if !parsed.overflow || parsed.rules.count != 320 { return 0 }
    parsed = BeginStyleRules("@import <missing>;", "missing.kss",
        KssDefaultEnvironment())
    parsed = ParseStyleRules(parsed)
    if parsed.parser.status != cast(s32)KssStatusNeedImport { return 0 }
    parsed = FailStyleRulesImport(parsed)
    if parsed.parser.status != cast(s32)KssStatusError { return 0 }

    bytes: [4]u8 = .[98, 111, 100, 121]
    owned_rules: StyleRules
    owned_rules.count = 1
    owned_rules.items[0].style.typeface = TextView(bytes[:])
    if !InstallStyleRules(owned_rules) { return 0 }
    bytes[0] = cast(u8)120
    installed: StyleRules = ActiveStyleRules()
    if installed.items[0].style.typeface != "body" { return 0 }
    owned_rules.count = 321
    if InstallStyleRules(owned_rules) ||
        ActiveStyleRules().items[0].style.typeface != "body" { return 0 }
    return 42
}
ZI

"$ziran" ir --root "$work" --module-path "$repo/src/kss" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    -o "$work/ir" "$work/app.zi"
"$ziran" bundle --root "$work" --module-path "$repo/src/kss" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" \
    --entry app:Answer -o "$work/source.zib" "$work/app.zi"
"$ziran" bundle --root "$work/ir" --module-path "$work/ir" \
    --entry app:Answer -o "$work/saved.zib" "$work/ir/app.zir"
cmp "$work/source.zib" "$work/saved.zib"
test "$("$ziran" run "$work/source.zib")" = 42
test "$("$ziran" run "$work/saved.zib")" = 42

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/kss
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --pkg main --root "$work" \
                --module-path "$module_dir" --module-path "$repo/../ziran/std" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
func main() {
    if App_Answer() != 42 { panic("parsed Progress style") }
}
GO
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --root "$work" \
                --module-path "$module_dir" --module-path "$repo/../ziran/std" --module-path "$repo/src/ui" --module-path "$repo/../ziran/std" -o "$output" "$module"
            if test "$target" = c; then
                cat > "$output/main.c" <<'C'
#include "app.h"
#include <assert.h>
int main(void) { assert(Answer() == 42); return 0; }
C
                "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                    -I"$output" "$output"/*.c -o "$output/app"
            else
                cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
#include <cassert>
int main() { assert(Answer() == 42); return 0; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                    -I"$output" "$output"/*.cpp -o "$output/app"
            fi
            "$output/app"
        fi
    done
done

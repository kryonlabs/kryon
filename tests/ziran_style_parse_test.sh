#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#module "app"
#import "kss_parser"
#import "progress"
#import "progress_style"
#import "style_parse"
#import "style"
#import "style_sheet"

Answer :: () -> i32 #export {
    parsed: StyleRulesParse = BeginStyleRules(
        "@pack demo; @import <base>; Progress[role=Track] { background: #112233; border: #99aabb; border-width: 2; radius: 5; }",
        "demo.kss", KssDefaultEnvironment())
    parsed = ParseStyleRules(parsed)
    if parsed.parser.status != (i32)KssStatusNeedImport ||
        parsed.rules.count != 0 { return 0 }
    parsed = ProvideStyleRulesImport(parsed,
        "Progress[role=Fill] { background: #556677; }", "base")
    parsed = ParseStyleRules(parsed)
    if parsed.parser.status != (i32)KssStatusDone ||
        parsed.overflow || parsed.rules.count != 2 { return 0 }
    InstallStyleRules(parsed.rules)
    defaults: ProgressFaces
    faces: ProgressFaces = ProgressFacesFor(ActiveStyleRules(), 0, defaults)
    if faces.track.value.background != (u32)0x112233ff ||
        faces.track.value.border != (u32)0x99aabbff ||
        faces.track.value.border_width != 2.0 ||
        faces.track.value.radius != 5.0 ||
        faces.fill.value.background != (u32)0x556677ff {
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
    if parsed.parser.status != (i32)KssStatusNeedImport { return 0 }
    parsed = FailStyleRulesImport(parsed)
    if parsed.parser.status != (i32)KssStatusError { return 0 }
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

cat > "$work/host.c" <<'C'
#include "kryon_portable_host.h"

#include <assert.h>

int main(int argc, char **argv)
{
    assert(argc == 2);
    Bundle *bundle = BundleOpen(argv[1]);
    assert(bundle != NULL);
    HostBinding binding = KssStringSliceBinding();
    long long value = 0;
    int has_value = 0;
    assert(BundleRun(bundle, &binding, 1, &value, &has_value));
    assert(has_value && value == 42);
    BundleClose(bundle);
    return 0;
}
C
"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" -I"$repo/../ziran/include" \
    "$work/host.c" "$repo/build/ziran/libkryon_host.a" \
    "$ziran_lib" ${VM_LDFLAGS:-} -o "$work/host"
"$work/host" "$work/source.zib"
"$work/host" "$work/saved.zib"

for input in source saved; do
    if test "$input" = source; then
        module=$work/app.zi
        module_dir=$repo/src/ui
    else
        module=$work/ir/app.zir
        module_dir=$work/ir
    fi
    for target in c cpp go; do
        output=$work/$target-$input
        if test "$target" = go; then
            "$ziran" build --target=go --strict --pkg main --root "$work" \
                --module-path "$module_dir" -o "$output" "$module"
            cat > "$output/main.go" <<'GO'
package main
type sliceHost struct{}
func (sliceHost) StringSlice(source string, start, length int32) string {
    if start < 0 || length < 0 ||
       int(start)+int(length) > len(source) { panic("slice bounds") }
    return source[start:start+length]
}
func main() {
    SetKssParserHost(sliceHost{})
    if App_Answer() != 42 { panic("parsed Progress style") }
}
GO
            GO111MODULE=off go run "$output"/*.go
        else
            "$ziran" build --target="$target" --strict --root "$work" \
                --module-path "$module_dir" -o "$output" "$module"
            if test "$target" = c; then
                cat > "$output/main.c" <<'C'
#include "app.h"
#include <assert.h>
String StringSlice(String source, int32_t start, int32_t length)
{
    assert(start >= 0 && length >= 0 &&
           (size_t)start + (size_t)length <= source.length);
    return (String){source.data + start, (size_t)length};
}
int main(void) { assert(Answer() == 42); return 0; }
C
                "${CC:-cc}" -std=c11 -I"$repo/../ziran/include" \
                    -I"$output" "$output"/*.c -o "$output/app"
            else
                cat > "$output/main.cpp" <<'CPP'
#include "app.hpp"
#include <cassert>
extern "C" String StringSlice(String source, int32_t start, int32_t length)
{
    assert(start >= 0 && length >= 0 &&
           static_cast<size_t>(start)+static_cast<size_t>(length) <=
               source.length);
    return String{source.data + start, static_cast<size_t>(length)};
}
int main() { assert(Answer() == 42); return 0; }
CPP
                "${CXX:-c++}" -std=c++17 -I"$repo/../ziran/include" \
                    -I"$output" "$output"/*.cpp -o "$output/app"
            fi
            "$output/app"
        fi
    done
done

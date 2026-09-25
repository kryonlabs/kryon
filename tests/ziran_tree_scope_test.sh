#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
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
#import "widget_kind"

using WidgetKind;

Frame :: (bounds: Rectangle) -> bool {
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreeCurrentParent(TestSession()) != 0 ||
        TreeCurrentParentKey(TestSession()) != cast(u64)9 { return false }
    outer: s32 = TreeSubmitCurrent(TestSession(), cast(u64)10, WidgetKindColumn, bounds)
    if outer != 1 || !TreePushScope(TestSession(), outer) ||
        TreeCurrentParent(TestSession()) != outer ||
        TreeCurrentParentKey(TestSession()) != cast(u64)10 { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)11, WidgetKindButton, bounds) != 2 {
        return false
    }
    inner: s32 = TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindGroup, bounds)
    if inner != 3 || !TreePushScope(TestSession(), inner) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)13, WidgetKindText, bounds) != 4 ||
        !TreePopScope(TestSession()) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)14, WidgetKindImage, bounds) != 5 ||
        !TreePopScope(TestSession()) { return false }
    if TreeSubmitCurrent(TestSession(), cast(u64)15, WidgetKindRouter, bounds) != 6 {
        return false
    }
    return TreeFinish(TestSession())
}

#program_export
Answer :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 100.0, 80.0}
    if !Frame(bounds) || TreeCount(TestSession()) != 7 ||
        TreeNodeAt(TestSession(), 1).parent != 0 ||
        TreeNodeAt(TestSession(), 2).parent != 1 ||
        TreeNodeAt(TestSession(), 3).parent != 1 ||
        TreeNodeAt(TestSession(), 4).parent != 3 ||
        TreeNodeAt(TestSession(), 5).parent != 1 ||
        TreeNodeAt(TestSession(), 6).parent != 0 ||
        TreeNodeAt(TestSession(), 4).parent_key != cast(u64)12 ||
        TreeNodeAt(TestSession(), 1).first_child != 2 ||
        TreeNodeAt(TestSession(), 2).next_sibling != 3 ||
        TreeNodeAt(TestSession(), 3).next_sibling != 5 { return -1 }
    generation: u64 = TreeNodeAt(TestSession(), 4).identity_generation
    if !Frame(bounds) || TreeNodeAt(TestSession(), 4).identity_generation != generation {
        return -2
    }
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreePopScope(TestSession()) || TreeFinish(TestSession()) ||
        TreeCount(TestSession()) != 7 { return -5 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    if TreePushScope(TestSession(), 99) || TreeFinish(TestSession()) ||
        TreeCount(TestSession()) != 7 { return -6 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    inner: s32 = TreeSubmitCurrent(TestSession(), cast(u64)12, WidgetKindGroup, bounds)
    if !TreePushScope(TestSession(), inner) || TreeFinish(TestSession()) || TreeCount(TestSession()) != 7 ||
        TreeNodeAt(TestSession(), 4).identity_generation != generation { return -3 }
    TreeStart(TestSession(), cast(u64)9, bounds)
    TreeCancel(TestSession())
    if TreeCurrentParent(TestSession()) != -1 || TreeSubmitCurrent(TestSession(), cast(u64)20,
        WidgetKindText, bounds) != -1 { return -4 }
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
        "$ziran" build --target="$target" \
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
            cat > "$output/scope_test.go" <<'GO'
package ziran
import "testing"
func TestScope(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("tree scope") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

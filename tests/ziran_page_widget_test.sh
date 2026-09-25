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

#import "control_props"
#import "geometry"
#import "layout"
#import "page"
#import "page_props"
#import "semantic"
#import "style"
#import "tree"
#import "widget_kind"

#program_export
Answer :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 320.0, 240.0}
    page_props: PageProps
    page_props.key = cast(u64)20
    page_props.title = "Welcome"
    page_props.description = "Start here"
    page_props.canonical_url = "https://example.test/"
    section_props: SectionProps
    section_props.key = cast(u64)30
    section_props.label = "Details"
    section_props.bounds = Rectangle.{8.0, 10.0, 100.0, 50.0}

    TreeStart(TestSession(), cast(u64)10, bounds)
    page: PageResult = Page(TestSession(), page_props)
    if !page.opened || page.node != 1 ||
        page.bounds.width != 320.0 || page.content.height != 240.0 ||
        page.gap != 0 || page.padding != 0 || page.has_theme_color ||
        page.title != "Welcome" || page.description != "Start here" ||
        page.canonical_url != "https://example.test/" ||
        TreeCurrentParentKey(TestSession()) != cast(u64)20 { return -1 }
    section: SectionResult = Section(TestSession(), section_props)
    if !section.opened || section.node != 2 ||
        section.bounds.width != 100.0 ||
        TreeCurrentParentKey(TestSession()) != cast(u64)30 { return -2 }
    if TreeSubmitCurrent(TestSession(), cast(u64)40, WidgetKindText,
        section.content) != 3 || !End(TestSession()) || !End(TestSession()) ||
        !TreeFinish(TestSession()) { return -3 }
    if TreeCount(TestSession()) != 4 || TreeNodeAt(TestSession(), 1).kind != WidgetKindPage ||
        TreeNodeAt(TestSession(), 1).semantic_kind != cast(SemanticKind)SemanticPage ||
        TreeNodeAt(TestSession(), 1).semantic_label != "Welcome" ||
        TreeNodeAt(TestSession(), 2).kind != WidgetKindSection ||
        TreeNodeAt(TestSession(), 2).semantic_kind != cast(SemanticKind)SemanticSection ||
        TreeNodeAt(TestSession(), 2).semantic_label != "Details" ||
        TreeNodeAt(TestSession(), 2).parent != 1 || TreeNodeAt(TestSession(), 3).parent != 2 {
        return -4
    }
    generation: u64 = TreeNodeAt(TestSession(), 2).identity_generation
    page_props.bounds = Rectangle.{0.0, 0.0, 200.0, 180.0}
    section_props.bounds = Rectangle.{0.0, 0.0, 0.0, 0.0}
    TreeStart(TestSession(), cast(u64)10, bounds)
    page = Page(TestSession(), page_props)
    section = Section(TestSession(), section_props)
    if section.bounds.width != 200.0 ||
        section.bounds.height != 180.0 ||
        !End(TestSession()) || !End(TestSession()) || !TreeFinish(TestSession()) ||
        TreeNodeAt(TestSession(), 2).identity_generation != generation { return -5 }

    style: StyleData
    style.gap = 3.6
    style.padding_x = 5.2
    metrics: PageSpacing = PageLayoutMetricsFor(style)
    if metrics.gap != 0 || metrics.padding != 0 { return -6 }
    style.fields = cast(u32)StyleGap | cast(u32)StylePaddingX
    metrics = PageLayoutMetricsFor(style)
    if metrics.gap != 4 || metrics.padding != 5 { return -7 }
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
            cat > "$output/page_test.go" <<'GO'
package ziran
import "testing"
func TestPage(t *testing.T) {
    if App_Answer() != 42 { t.Fatal("page") }
}
GO
            GO111MODULE=off go test "$output"/*.go
        fi
    done
done
cmp "$work/source.zib" "$work/saved.zib"

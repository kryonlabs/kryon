#!/bin/sh
set -eu

repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
ziran=${ZIRAN_BIN:-"$repo/../ziran/build/bin/ziran"}
ziran_lib=${ZIRAN_LIB:-"$repo/../ziran/build/libziran.a"}
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT HUP INT TERM

cat > "$work/app.zi" <<'ZI'
#import "drawing_props"
#import "geometry"
#import "layout"
#import "layout_widget"
#import "page"
#import "page_props"
#import "page_text"
#import "paint_queue"
#import "semantic"
#import "tree"
#import "text_props"
#import "text_widget"
#import "widget_kind"

#program_export
Answer :: () -> s32 {
    bounds: Rectangle = Rectangle.{0.0, 0.0, 200.0, 100.0}
    TreeStart(cast(u64)1, bounds)
    page_props: PageProps
    page_props.key = cast(u64)10
    page_props.title = "Document"
    page: PageResult = Page(page_props)
    heading: HeadingProps
    heading.key = cast(u64)11
    heading.bounds = Rectangle.{10.0, 20.0, 0.0, 0.0}
    heading.text = "Title"
    heading.level = 9
    Heading(heading)
    paragraph: ParagraphTextProps
    paragraph.key = cast(u64)12
    paragraph.bounds = Rectangle.{10.0, 40.0, 0.0, 0.0}
    paragraph.text = "Body"
    ParagraphText(paragraph)
    if !page.opened || !End() || !TreeFinish() ||
        TreeCount() != 4 ||
        TreeNodeAt(1).semantic_kind != cast(SemanticKind)SemanticPage ||
        TreeNodeAt(2).parent != 1 ||
        TreeNodeAt(2).kind != WidgetKindText ||
        TreeNodeAt(2).semantic_kind != cast(SemanticKind)SemanticHeading ||
        TreeNodeAt(2).heading_level != 6 ||
        TreeNodeAt(2).semantic_label != "Title" ||
        TreeNodeAt(2).bounds.width != 50.0 ||
        TreeNodeAt(3).parent != 1 ||
        TreeNodeAt(3).semantic_kind != cast(SemanticKind)SemanticParagraph ||
        TreeNodeAt(3).heading_level != 0 ||
        TreeNodeAt(3).bounds.width != 190.0 { return -1 }
    PaintFlush()
    TreeStart(cast(u64)20, bounds)
    column_props: ColumnProps
    column_props.key = cast(u64)21
    column_props.bounds = bounds
    column_props.padding = 5
    column_props.gap = 3
    column: LayoutContainer = Column(column_props)
    first: TextProps
    first.key = cast(u64)22
    first.text = "A"
    Text(first)
    first.key = cast(u64)23
    first.text = "BB"
    Text(first)
    if !column.opened || !End() || !TreeFinish() ||
        TreeNodeAt(2).bounds.x != 5.0 ||
        TreeNodeAt(2).bounds.y != 5.0 ||
        TreeNodeAt(3).bounds.x != 5.0 ||
        TreeNodeAt(3).bounds.y != 20.0 { return -2 }
    PaintFlush()
    TreeStart(cast(u64)30, bounds)
    viewport: s32 = TreeSubmit(cast(u64)31, 0, WidgetKindCard,
        Rectangle.{0.0, 0.0, 100.0, 100.0})
    TreeSetChildClip(viewport,
        Rectangle.{20.0, 20.0, 40.0, 40.0})
    child: s32 = TreeSubmit(cast(u64)32, viewport, WidgetKindText,
        Rectangle.{10.0, 10.0, 70.0, 70.0})
    ink: Color = ColorFromPacked(cast(u32)0x171717ff)
    PaintLabel(child, "Plain", 10, 10, 16, ink)
    PaintClippedLabel(child, "Clipped", 30, 30, 16, ink,
        Rectangle.{30.0, 30.0, 50.0, 50.0})
    PaintImage(child, "", cast(u32)7,
        Rectangle.{0.0, 0.0, 70.0, 70.0},
        Rectangle.{10.0, 10.0, 70.0, 70.0},
        Rectangle.{10.0, 10.0, 70.0, 70.0},
        Vector2.{0.0, 0.0}, 0.0, 0.0, ink)
    if !TreeFinish() { return -3 }
    PaintFlush()
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

"${CC:-cc}" ${VM_CFLAGS:-} -std=c11 -I"$repo/build/ziran/c" -I"$repo/include" \
    -I"$repo/../ziran/include" \
    "$repo/tests/ziran_page_text_test.c" \
    "$repo/build/ziran/libkryon_host.a" "$ziran_lib" \
    ${VM_LDFLAGS:-} -o "$work/host-test"
"$work/host-test" "$work/source.zib"
"$work/host-test" "$work/saved.zib"

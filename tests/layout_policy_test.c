#include <assert.h>
#include <math.h>

#include "runtime/layout.h"

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

static void
check_flex_alignment(void)
{
    const float starts[] = {14, 69, 124, 14, 41.5f, 50.666667f};
    const float seconds[] = {44, 99, 154, 154, 126.5f, 117.333333f};
    FlexProps props = {.bounds = {10, 20, 188, 88}, .gap = 10, .padding = 4};

    for(int direction = FlexRow; direction <= FlexColumn; direction++) {
        props.direction = (FlexDirection)direction;
        props.bounds = direction == FlexRow
            ? (Rectangle){10, 20, 188, 88} : (Rectangle){20, 10, 88, 188};
        for(int justify = JustifyStart; justify <= JustifySpaceEvenly; justify++) {
            props.justify_content = (JustifyContent)justify;
            for(int align = AlignStart; align <= AlignStretch; align++) {
                props.align_items = (AlignItems)align;
                float cross = align == AlignCenter ? 49 : align == AlignEnd ? 74 : 24;
                FlexCursor cursor = BeginFlexCursor(props, 2, 60);
                cursor = FlexStep(cursor, direction == FlexRow ? 20 : 30,
                                  direction == FlexRow ? 30 : 20);
                if(direction == FlexRow)
                    check_rect(cursor.item, starts[justify], cross, 20, 30);
                else
                    check_rect(cursor.item, cross, starts[justify], 30, 20);
                cursor = FlexStep(cursor, direction == FlexRow ? 40 : 30,
                                  direction == FlexRow ? 30 : 40);
                if(direction == FlexRow)
                    check_rect(cursor.item, seconds[justify], cross, 40, 30);
                else
                    check_rect(cursor.item, cross, seconds[justify], 30, 40);
                assert(cursor.remaining == 0);
                cursor = FlexStep(cursor, 10, 10);
                check_rect(cursor.item, 0, 0, 0, 0);
            }
        }
    }

    props = (FlexProps){.bounds = {10, 20, 180, 80},
                        .align_items = AlignStretch};
    FlexCursor cursor = BeginFlexCursor(props, 1, 20);
    cursor = FlexStep(cursor, 20, 0);
    check_rect(cursor.item, 10, 20, 20, 80);
    props.direction = FlexColumn;
    cursor = BeginFlexCursor(props, 1, 20);
    cursor = FlexStep(cursor, 0, 20);
    check_rect(cursor.item, 10, 20, 180, 20);

    props.direction = FlexRow;
    for(int justify = JustifyStart; justify <= JustifySpaceEvenly; justify++) {
        props.justify_content = (JustifyContent)justify;
        props.align_items = AlignStart;
        float x = justify == JustifyEnd ? 170 :
            justify == JustifyCenter || justify == JustifySpaceAround ||
            justify == JustifySpaceEvenly ? 90 : 10;
        cursor = FlexStep(BeginFlexCursor(props, 1, 20), 20, 30);
        check_rect(cursor.item, x, 20, 20, 30);
        cursor = FlexStep(BeginFlexCursor(props, 0, 0), 20, 30);
        check_rect(cursor.item, 0, 0, 0, 0);
        cursor = FlexStep(BeginFlexCursor(props, -1, 0), 20, 30);
        check_rect(cursor.item, 0, 0, 0, 0);

        props.align_items = AlignCenter;
        cursor = FlexStep(BeginFlexCursor(props, 1, 200), 200, 100);
        check_rect(cursor.item, 10, 20, 200, 100);
    }

    props = (FlexProps){.bounds = {10, 20, 1, 1}, .padding = 4,
                        .justify_content = JustifySpaceAround};
    cursor = FlexStep(BeginFlexCursor(props, 1, -20), -20, -30);
    check_rect(cursor.item, 14, 24, 0, 0);
    props = (FlexProps){.bounds = {10, 20, 180, 80}, .gap = -8, .padding = -4};
    cursor = FlexStep(BeginFlexCursor(props, 2, 40), 20, 30);
    cursor = FlexStep(cursor, 20, 30);
    check_rect(cursor.item, 30, 20, 20, 30);
}

int
main(void)
{
    check_flex_alignment();
    Rectangle bounds = {10, 20, 100, 80};
    Rectangle child = {0, 0, 0, 18};
    LayoutMetrics metrics = LayoutMetricsFor(bounds, 6, 4);
    CenteredColumnLayout column;
    float cursor;

    column = CenteredColumnFor(800, 480, 24);
    assert(column.x == 160);
    assert(column.width == 480);
    column = CenteredColumnFor(320, 480, 24);
    assert(column.x == 24);
    assert(column.width == 272);
    column = CenteredColumnFor(20, 480, 24);
    assert(column.x == 10);
    assert(column.width == 0);
    column = CenteredColumnFor(320, -1, -2);
    assert(column.x == 160);
    assert(column.width == 0);
    assert(PageSidePaddingFor(400) == 12);
    assert(PageSidePaddingFor(1000) == 20);
    assert(PageSidePaddingFor(3000) == 24);
    assert(DesktopWidthThresholdFor(0.0f) == 500);
    assert(DesktopWidthThresholdFor(1.5f) == 750);
    assert(!IsDesktopWidth(499, 1.0f));
    assert(IsDesktopWidth(500, 1.0f));
    assert(!IsDesktopWidth(749, 1.5f));
    assert(IsDesktopWidth(750, 1.5f));

    assert(metrics.gap == 6);
    assert(metrics.padding == 4);
    check_rect(metrics.content, 14, 24, 92, 72);
    cursor = LayoutCursorForChild(metrics, false, 2, 48);
    assert(fabsf(cursor - 84.0f) < 0.001f);
    check_rect(LayoutChildBounds(child, child, metrics, false, cursor),
               14, 84, 92, 18);

    child = (Rectangle){0, 0, 30, 0};
    cursor = LayoutCursorForChild(metrics, true, 1, 30);
    assert(fabsf(cursor - 50.0f) < 0.001f);
    check_rect(LayoutChildBounds(child, child, metrics, true, cursor),
               50, 24, 30, 72);

    child = (Rectangle){5, 0, 30, 10};
    check_rect(LayoutChildBounds(child, child, metrics, true, 50),
               5, 0, 30, 10);

    child = (Rectangle){0, 0, 0, 0};
    check_rect(StackChildBounds(child, child, metrics), 14, 24, 92, 72);

    metrics = LayoutMetricsFor(bounds, -1, -2);
    assert(metrics.gap == 0);
    assert(metrics.padding == 0);
    check_rect(metrics.content, 10, 20, 100, 80);

    check_rect(LayoutScopeBounds((Rectangle){2, 3, 0, -1}, 320, 180),
               2, 3, 320, 180);
    check_rect(LayoutScopeBounds((Rectangle){2, 3, 40, 50}, 320, 180),
               2, 3, 40, 50);
    return 0;
}

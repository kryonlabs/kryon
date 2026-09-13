#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>

#include "runtime/dropdown.h"

Activation
ReadActivation(Rectangle bounds, int32_t id, bool enabled)
{
    (void)bounds;
    (void)id;
    (void)enabled;
    return (Activation){0};
}

int32_t
MeasureTextWidth(const char *text, int32_t font, const char *typeface)
{
    (void)text;
    (void)font;
    (void)typeface;
    return 0;
}

void *
InstanceState(const char *type, uint64_t key, size_t size)
{
    static unsigned char storage[4096];

    (void)type;
    (void)key;
    assert(size <= sizeof(storage));
    return storage;
}

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

static void
test_indicator(void)
{
    DropdownIndicator closed = DropdownIndicatorFor(100, 50, 10, false);
    DropdownIndicator open = DropdownIndicatorFor(100, 50, 10, true);
    DropdownIndicator zero = DropdownIndicatorFor(8, 9, -1, false);

    assert(closed.x1 == 95);
    assert(closed.y1 == 48);
    assert(closed.x2 == 100);
    assert(closed.y2 == 52);
    assert(closed.x3 == 100);
    assert(closed.y3 == 52);
    assert(closed.x4 == 105);
    assert(closed.y4 == 48);

    assert(open.x1 == 95);
    assert(open.y1 == 52);
    assert(open.x2 == 100);
    assert(open.y2 == 48);
    assert(open.x3 == 100);
    assert(open.y3 == 48);
    assert(open.x4 == 105);
    assert(open.y4 == 52);

    assert(zero.x1 == 8);
    assert(zero.y1 == 9);
    assert(zero.x4 == 8);
    assert(zero.y4 == 9);
}

static void
test_existing_policy(void)
{
    StyleFrame frame = {0};
    VisibleRows rows = Rows(10, 15, 35.0f, 10.0f);
    MenuLayout layout = MenuLayoutFor((Rectangle){10, 20, 100, 64},
                                      5, 20, 4, 4, 8, 2);
    Rectangle track;
    OptionPaint option;

    assert(ClampIndex(-2, 3) == 0);
    assert(ClampIndex(5, 3) == 2);
    assert(ClampIndex(0, 0) == -1);
    assert(ContentHeight(3, 20.0f, 8.0f) == 68);
    assert(WheelOffset(20, 1.0f, 10.0f, 100) == 10);
    assert(rows.first == 1);
    assert(rows.end == 5);
    assert(layout.content_height == 108);
    assert(layout.max_scroll == 44);
    assert(layout.option_width == 90);
    check_rect(layout.content_bounds, 10, 24, 90, 56);
    check_rect(layout.scrollbar_bounds, 102, 20, 8, 64);
    track = ScrollbarTrackBounds(layout.scrollbar_bounds, 2);
    check_rect(track, 102, 22, 8, 60);
    option = OptionPaintFor((Rectangle){10, 20, 100, 64}, layout.option_width,
                            2, 20, 18, 4, 4, 4, 2);
    assert(option.option_y == 46);
    check_rect(option.visible_bounds, 10, 46, 90, 20);
    check_rect(option.highlight_bounds, 14, 48, 82, 16);
    frame.value.fields = StylePaddingY | StyleGap | StyleContentOffset;
    frame.value.padding_y = 10.0f;
    frame.value.gap = 6.0f;
    frame.value.offset_y = 20.0f;
    check_rect(PopupBounds((Rectangle){20, 30, 80, 24},
                           (Rectangle){0, 0, 200, 200}, 2, 1.0f, frame),
               20, 60, 80, 58);
}

int
main(void)
{
    test_indicator();
    test_existing_policy();
    return 0;
}

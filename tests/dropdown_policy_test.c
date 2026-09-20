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
    StyleFrame trigger = {0};
    DropdownIndicator closed = DropdownIndicatorFor(100, 50, 10, false);
    DropdownIndicator open = DropdownIndicatorFor(100, 50, 10, true);
    DropdownIndicator zero = DropdownIndicatorFor(8, 9, -1, false);
    DropdownTriggerMetrics metrics = DropdownTriggerMetricsFor(1.0f, trigger);
    ContentMetrics content = {.padding = 8.0f, .gap = 4.0f,
                              .icon = 16.0f, .font = 14.0f};
    DropdownTriggerContent layout;

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

    assert(metrics.min_width == 32);
    assert(metrics.min_height == 24);
    assert(metrics.indicator_padding == 24);
    assert(metrics.indicator_size == 10);
    assert(metrics.text_indicator_gap == 8);

    trigger.value.fields = StyleContentOffset | StyleIconSize | StyleGap;
    trigger.value.offset_x = 18.0f;
    trigger.value.icon_size = 12.0f;
    trigger.value.gap = 5.0f;
    metrics = DropdownTriggerMetricsFor(2.0f, trigger);
    assert(metrics.min_width == 64);
    assert(metrics.min_height == 48);
    assert(metrics.indicator_padding == 36);
    assert(metrics.indicator_size == 24);
    assert(metrics.text_indicator_gap == 10);

    trigger.value.offset_x = 0.0f;
    trigger.value.icon_size = 0.0f;
    trigger.value.gap = 0.0f;
    metrics = DropdownTriggerMetricsFor(1.0f, trigger);
    assert(metrics.indicator_padding == 0);
    assert(metrics.indicator_size == 0);
    assert(metrics.text_indicator_gap == 0);

    metrics = DropdownTriggerMetricsFor(1.0f, (StyleFrame){0});
    layout = DropdownTriggerContentFor((Rectangle){10, 20, 120, 30},
                                       content, metrics, false);
    assert(layout.indicator_center_x == 106);
    assert(layout.indicator_center_y == 35);
    check_rect(layout.clip_bounds, 18, 20, 70, 30);
    check_rect(layout.text_bounds, 18, 20, 70, 30);

    layout = DropdownTriggerContentFor((Rectangle){10, 20, 120, 30},
                                       content, metrics, true);
    check_rect(layout.icon_bounds, 18, 27, 16, 16);
    check_rect(layout.clip_bounds, 38, 20, 50, 30);
    check_rect(layout.text_bounds, 38, 20, 50, 30);
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
    ContentMetrics content = {.padding = 8.0f, .gap = 4.0f,
                              .icon = 16.0f, .font = 14.0f};
    DropdownMenuMetrics metrics = {.separator_inset = 16};
    DropdownOptionContent option_content;
    DropdownTriggerInput trigger_input;
    DropdownMenuInput menu_input;
    DropdownOpenDecision open_decision;

    assert(ClampIndex(-2, 3) == 0);
    assert(ClampIndex(5, 3) == 2);
    assert(ClampIndex(0, 0) == -1);
    assert(DropdownOptionCountFor(-4) == 0);
    assert(DropdownOptionCountFor(5) == 5);
    assert(DropdownCurrentIndexFor(-2, 3) == 0);
    assert(DropdownCurrentIndexFor(5, 3) == 2);
    assert(DropdownCurrentIndexFor(0, 0) == 0);
    trigger_input = DropdownTriggerInputFor(true, false, false, false);
    assert(trigger_input.enter && !trigger_input.space && !trigger_input.down);
    trigger_input = DropdownTriggerInputFor(false, true, true, true);
    assert(trigger_input.enter && trigger_input.space && trigger_input.down);
    open_decision = DropdownOpenDecisionFor(false, false, 3, true, true,
                                            false, trigger_input, 24.0f,
                                            false, false, false);
    assert(open_decision.open && open_decision.changed &&
           open_decision.opened && !open_decision.closed);
    open_decision = DropdownOpenDecisionFor(true, false, 3, true, true,
                                            true, trigger_input, 24.0f,
                                            false, false, false);
    assert(!open_decision.open && open_decision.changed &&
           !open_decision.opened && open_decision.closed);
    open_decision = DropdownOpenDecisionFor(true, true, 3, true, true,
                                            false, trigger_input, 24.0f,
                                            false, false, false);
    assert(!open_decision.open && open_decision.changed &&
           open_decision.closed);
    open_decision = DropdownOpenDecisionFor(true, false, 3, false, true,
                                            false, trigger_input, 24.0f,
                                            false, false, true);
    assert(!open_decision.open && open_decision.changed &&
           open_decision.closed);
    open_decision = DropdownOpenAfterCommit(true, true);
    assert(!open_decision.open && open_decision.changed &&
           !open_decision.opened && open_decision.closed);
    open_decision = DropdownOpenAfterCommit(true, false);
    assert(open_decision.open && !open_decision.changed &&
           !open_decision.opened && !open_decision.closed);
    menu_input = DropdownMenuInputFor(true, false, true, false, false, false,
                                      false, false, true);
    assert(menu_input.navigating && menu_input.up && !menu_input.commit &&
           menu_input.escape);
    menu_input = DropdownMenuInputFor(true, false, false, true, false, false,
                                      true, false, false);
    assert(menu_input.navigating && menu_input.down && menu_input.commit);
    menu_input = DropdownMenuInputFor(true, false, false, false, true, false,
                                      false, false, false);
    assert(menu_input.navigating && menu_input.home);
    menu_input = DropdownMenuInputFor(true, false, false, false, false, true,
                                      false, true, false);
    assert(menu_input.navigating && menu_input.end && menu_input.commit);
    menu_input = DropdownMenuInputFor(false, false, true, true, true, true,
                                      true, true, true);
    assert(!menu_input.navigating && !menu_input.commit && !menu_input.escape);
    menu_input = DropdownMenuInputFor(true, true, true, true, true, true,
                                      true, true, true);
    assert(!menu_input.navigating && !menu_input.commit && menu_input.escape);
    assert(ContentHeight(3, 20.0f, 8.0f) == 68);
    assert(WheelOffset(20, 1.0f, 10.0f, 100) == 10);
    assert(DropdownJustOpenedNext(true, true));
    assert(!DropdownJustOpenedNext(true, false));
    assert(!DropdownJustOpenedNext(false, true));
    assert(DropdownScrollbarPressedNext(false, true, true, 20, true));
    assert(!DropdownScrollbarPressedNext(false, true, true, 0, true));
    assert(!DropdownScrollbarPressedNext(false, true, true, 20, false));
    assert(DropdownScrollbarPressedNext(true, true, false, 0, false));
    assert(!DropdownScrollbarPressedNext(true, false, false, 20, true));
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
    option_content = DropdownOptionContentFor(
        (Rectangle){10, 46, 90, 20}, option.visible_bounds, content,
        metrics, false);
    check_rect(option_content.separator_bounds, 26, 46, 58, 0);
    check_rect(option_content.clip_bounds, 18, 46, 54, 20);
    check_rect(option_content.text_bounds, 18, 46, 54, 20);
    check_rect(option_content.check_bounds, 76, 48, 16, 16);
    option_content = DropdownOptionContentFor(
        (Rectangle){10, 46, 90, 20}, option.visible_bounds, content,
        metrics, true);
    check_rect(option_content.icon_bounds, 18, 48, 16, 16);
    check_rect(option_content.clip_bounds, 38, 46, 34, 20);
    check_rect(option_content.text_bounds, 38, 46, 34, 20);
    frame.value.padding_y = 10.0f;
    frame.value.gap = 6.0f;
    frame.value.offset_y = 20.0f;
    check_rect(PopupBounds((Rectangle){20, 30, 80, 24},
                           (Rectangle){0, 0, 200, 200}, 2, 1.0f, frame),
               20, 58, 80, 56);

    frame.value.fields = StylePaddingY | StyleGap | StyleContentOffset;
    frame.value.padding_y = 10.0f;
    frame.value.gap = 6.0f;
    frame.value.offset_y = 20.0f;
    check_rect(PopupBounds((Rectangle){20, 30, 80, 24},
                           (Rectangle){0, 0, 200, 200}, 2, 1.0f, frame),
               20, 60, 80, 58);
    frame.value.padding_y = 0.0f;
    frame.value.gap = 0.0f;
    frame.value.offset_y = 0.0f;
    check_rect(PopupBounds((Rectangle){20, 30, 80, 24},
                           (Rectangle){0, 0, 200, 200}, 2, 1.0f, frame),
               20, 54, 80, 48);
}

static void
test_menu_metrics(void)
{
    StyleFrame panel = {0};
    StyleFrame option = {0};
    StyleFrame scrollbar = {0};
    DropdownMenuMetrics metrics =
        DropdownMenuMetricsFor(1.0f, panel, option, scrollbar);

    assert(metrics.padding_top == 4);
    assert(metrics.padding_bottom == 4);
    assert(metrics.scrollbar_width == 8);
    assert(metrics.scrollbar_gap == 2);
    assert(metrics.scrollbar_track_inset == 2);
    assert(metrics.highlight_inset_x == 4);
    assert(metrics.highlight_inset_y == 2);
    assert(metrics.separator_inset == 16);
    assert(metrics.drag_threshold == 8);

    panel.value.fields = StylePaddingY;
    panel.value.padding_y = 6.0f;
    option.value.fields = StylePaddingX | StylePaddingY | StyleContentOffset;
    option.value.padding_x = 5.0f;
    option.value.padding_y = 3.0f;
    option.value.offset_x = 12.0f;
    scrollbar.value.fields = StyleIconSize | StylePaddingY | StyleGap |
        StyleContentOffset;
    scrollbar.value.icon_size = 11.0f;
    scrollbar.value.padding_y = 4.0f;
    scrollbar.value.gap = 7.0f;
    scrollbar.value.offset_x = 9.0f;
    metrics = DropdownMenuMetricsFor(2.0f, panel, option, scrollbar);

    assert(metrics.padding_top == 12);
    assert(metrics.padding_bottom == 12);
    assert(metrics.scrollbar_width == 22);
    assert(metrics.scrollbar_gap == 14);
    assert(metrics.scrollbar_track_inset == 8);
    assert(metrics.highlight_inset_x == 10);
    assert(metrics.highlight_inset_y == 6);
    assert(metrics.separator_inset == 24);
    assert(metrics.drag_threshold == 18);

    panel.value.padding_y = 0.0f;
    option.value.padding_x = 0.0f;
    option.value.padding_y = 0.0f;
    option.value.offset_x = 0.0f;
    scrollbar.value.icon_size = 0.0f;
    scrollbar.value.padding_y = 0.0f;
    scrollbar.value.gap = 0.0f;
    scrollbar.value.offset_x = 0.0f;
    metrics = DropdownMenuMetricsFor(1.0f, panel, option, scrollbar);

    assert(metrics.padding_top == 0);
    assert(metrics.padding_bottom == 0);
    assert(metrics.scrollbar_width == 0);
    assert(metrics.scrollbar_gap == 0);
    assert(metrics.scrollbar_track_inset == 0);
    assert(metrics.highlight_inset_x == 0);
    assert(metrics.highlight_inset_y == 0);
    assert(metrics.separator_inset == 0);
    assert(metrics.drag_threshold == 8);
}

int
main(void)
{
    test_indicator();
    test_existing_policy();
    test_menu_metrics();
    return 0;
}

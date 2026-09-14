#include <assert.h>

#include "runtime/toolbar.h"

static void
test_toolbar_action_bounds(void)
{
    ToolbarLayout layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 0, .y = 0, .width = 320, .height = 48,
        .action_count = 2, .scale = 2.0f
    });
    ToolbarDividerLine divider = ToolbarDividerLineFor(5, 7, 120, 44);
    Rectangle first = ToolbarActionBoundsFor(layout, 0, 2);
    Rectangle second = ToolbarActionBoundsFor(layout, 1, 2);

    assert(layout.action_icon_size == 40);
    assert(layout.action_icon_padding == 16);
    assert(layout.action_width == 72);
    assert(layout.action_gap == 12);
    assert(layout.side_padding == 24);
    assert(ToolbarBarRole() == 1);
    assert(ToolbarDividerRole() == 18);
    assert(ToolbarActionRole() == 17);
    assert(ToolbarBottomBarRole() == 28);
    assert(ToolbarBottomActionRole() == 29);
    assert(ToolbarActionIdFor(0, 1) == 0);
    assert(ToolbarActionIdFor(7, -1) == 0);
    assert(ToolbarActionIdFor(7, 0) == 701);
    assert(ToolbarActionIdFor(7, 1) == 702);
    assert(divider.x1 == 5);
    assert(divider.y1 == 50);
    assert(divider.x2 == 125);
    assert(divider.y2 == 50);
    assert(first.x == 140.0f);
    assert(first.y == -12.0f);
    assert(second.x == 224.0f);
    assert(ToolbarActionStyleIconSize(40, 2.0f) == 20.0f);
    assert(ToolbarActionStyleIconSize(-40, 2.0f) == 0.0f);
    assert(ToolbarActionStyleIconSize(20, 0.0f) == 20.0f);

    layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 0, .y = 0, .width = 120, .height = 32,
        .action_count = 1, .action_fields = StyleIconSize |
            StylePaddingX | StyleGap, .bar_fields = StylePaddingX,
        .action_icon_size = 18,
        .action_icon_padding = 5,
        .action_gap = 3,
        .side_padding = 7,
        .scale = 1.0f
    });
    first = ToolbarActionBoundsFor(layout, 0, 1);
    assert(layout.action_icon_size == 18);
    assert(layout.action_icon_padding == 5);
    assert(layout.action_width == 28);
    assert(layout.action_gap == 3);
    assert(layout.side_padding == 7);
    assert(first.x == 85.0f);
    assert(first.y == 2.0f);

    StyleFrame bar = {.value = {.fields = StylePaddingX,
                                .padding_x = 9.0f}};
    StyleFrame action = {.value = {.fields = StyleIconSize |
                                             StylePaddingX | StyleGap,
                                   .icon_size = 22.0f,
                                   .padding_x = 6.0f,
                                   .gap = 5.0f}};
    layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 4, .y = 10, .width = 140, .height = 40,
        .action_count = 1,
        .action_icon_size = -1,
        .action_icon_padding = -1,
        .action_gap = -1,
        .side_padding = -1,
        .bar = bar,
        .action = action,
        .scale = 1.0f
    });
    first = ToolbarActionBoundsFor(layout, 0, 1);
    assert(layout.action_icon_size == 22);
    assert(layout.action_icon_padding == 6);
    assert(layout.action_width == 34);
    assert(layout.action_gap == 5);
    assert(layout.side_padding == 9);
    assert(first.x == 101.0f);
    assert(first.y == 13.0f);

    bar.value.padding_x = 0.0f;
    action.value.icon_size = 0.0f;
    action.value.padding_x = 0.0f;
    action.value.gap = 0.0f;
    layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 0, .y = 0, .width = 80, .height = 12,
        .action_count = 1,
        .action_icon_size = -1,
        .action_icon_padding = -1,
        .action_gap = -1,
        .side_padding = -1,
        .bar = bar,
        .action = action,
        .scale = 1.0f
    });
    assert(layout.action_icon_size == 0);
    assert(layout.action_icon_padding == 0);
    assert(layout.action_width == 0);
    assert(layout.action_gap == 0);
    assert(layout.side_padding == 0);
}

static void
test_bottom_icon_row_layout(void)
{
    BottomIconRowProps props = {
        .center_x = 160, .view_width = 320, .view_height = 240, .count = 3
    };
    BottomIconRowLayout layout = BottomIconRowLayoutFor(props, 1.0f);
    StyleFrame bar = {.value = {.fields = StylePaddingX | StylePaddingY |
                                StyleGap | StyleContentOffset,
                                .padding_x = 20.0f,
                                .padding_y = 4.0f,
                                .gap = 10.0f,
                                .offset_x = 140.0f}};
    StyleFrame action = {.value = {.fields = StylePaddingX | StyleGap |
                                   StyleIconSize | StyleContentOffset,
                                   .padding_x = 8.0f,
                                   .gap = 14.0f,
                                   .icon_size = 22.0f,
                                   .offset_x = 12.0f,
                                   .offset_y = 5.0f}};
    Rectangle second = BottomIconRowButtonBoundsFor(layout, 1);

    assert(layout.button_width == 44);
    assert(layout.icon_size == 24);
    assert(layout.icon_padding == 10);
    assert(layout.gap == 11);
    assert(layout.start_x == 83);
    assert(layout.y == 190);
    assert(second.x == 138.0f);
    assert(second.y == 190.0f);
    assert(second.width == 44.0f);

    layout = BottomIconRowLayoutForStyle(props, 1.0f, bar, action);
    second = BottomIconRowButtonBoundsFor(layout, 1);
    assert(layout.button_width == 38);
    assert(layout.icon_size == 22);
    assert(layout.icon_padding == 8);
    assert(layout.gap == 10);
    assert(layout.start_x == 93);
    assert(layout.y == 198);
    assert(second.x == 141.0f);

    bar.value.padding_y = 0.0f;
    bar.value.gap = 0.0f;
    action.value.padding_x = 0.0f;
    action.value.icon_size = 0.0f;
    action.value.offset_x = 0.0f;
    action.value.offset_y = 0.0f;
    layout = BottomIconRowLayoutForStyle(props, 1.0f, bar, action);
    second = BottomIconRowButtonBoundsFor(layout, 1);
    assert(layout.button_width == 0);
    assert(layout.icon_size == 0);
    assert(layout.icon_padding == 0);
    assert(layout.gap == 0);
    assert(layout.y == 240);
    assert(second.width == 0.0f);
}

static void
test_icon_slider_popup_layout(void)
{
    IconSliderPopupLayout layout =
        IconSliderPopupLayoutFor(20, 30, 24, 10, 0, 0, 1.0f);
    IconSliderPopupLayout wide =
        IconSliderPopupLayoutFor(20, 30, 24, 10, 30, 120, 2.0f);
    IconSliderPopupCloseDecision close_decision;

    assert(layout.button_bounds.x == 20.0f);
    assert(layout.button_bounds.y == 30.0f);
    assert(layout.button_bounds.width == 44.0f);
    assert(layout.popup_bounds.x == 20.0f);
    assert(layout.popup_bounds.y == 78.0f);
    assert(layout.popup_bounds.width == 44.0f);
    assert(layout.popup_bounds.height == 200.0f);
    assert(layout.slider_x == 42);
    assert(layout.slider_y == 92);
    assert(layout.slider_height == 176);

    assert(wide.popup_bounds.x == 20.0f);
    assert(wide.popup_bounds.y == 82.0f);
    assert(wide.popup_bounds.width == 44.0f);
    assert(wide.popup_bounds.height == 120.0f);
    assert(wide.slider_y == 110);
    assert(wide.slider_height == 72);

    close_decision = IconSliderPopupCloseDecisionFor(false, true, false);
    assert(close_decision.close);
    close_decision = IconSliderPopupCloseDecisionFor(true, true, false);
    assert(!close_decision.close);
    close_decision = IconSliderPopupCloseDecisionFor(false, false, false);
    assert(!close_decision.close);
    close_decision = IconSliderPopupCloseDecisionFor(false, true, true);
    assert(!close_decision.close);
}

int
main(void)
{
    test_toolbar_action_bounds();
    test_bottom_icon_row_layout();
    test_icon_slider_popup_layout();
    return 0;
}

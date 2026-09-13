#include <assert.h>

#include "runtime/toolbar.h"

static void
test_toolbar_action_bounds(void)
{
    ToolbarLayout layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 0, .y = 0, .width = 320, .height = 48,
        .action_count = 2, .scale = 2.0f
    });
    Rectangle first = ToolbarActionBoundsFor(layout, 0, 2);
    Rectangle second = ToolbarActionBoundsFor(layout, 1, 2);

    assert(layout.action_icon_size == 40);
    assert(layout.action_icon_padding == 16);
    assert(layout.action_width == 72);
    assert(layout.action_gap == 12);
    assert(layout.side_padding == 24);
    assert(first.x == 140.0f);
    assert(first.y == -12.0f);
    assert(second.x == 224.0f);

    layout = ToolbarLayoutFor((ToolbarSpec){
        .x = 0, .y = 0, .width = 120, .height = 32,
        .action_count = 1, .action_fields = StyleIconSize |
            StylePaddingX | StyleGap, .bar_fields = StylePaddingX,
        .scale = 1.0f
    });
    first = ToolbarActionBoundsFor(layout, 0, 1);
    assert(layout.action_icon_size == 0);
    assert(layout.action_icon_padding == 0);
    assert(layout.action_width == 0);
    assert(layout.action_gap == 0);
    assert(layout.side_padding == 0);
    assert(first.x == 120.0f);
    assert(first.y == 16.0f);
}

static void
test_bottom_icon_row_layout(void)
{
    BottomIconRowProps props = {
        .center_x = 160, .view_width = 320, .view_height = 240, .count = 3
    };
    BottomIconRowLayout layout = BottomIconRowLayoutFor(props, 1.0f);
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
}

static void
test_icon_slider_popup_layout(void)
{
    IconSliderPopupLayout layout =
        IconSliderPopupLayoutFor(20, 30, 24, 10, 0, 0, 1.0f);
    IconSliderPopupLayout wide =
        IconSliderPopupLayoutFor(20, 30, 24, 10, 30, 120, 2.0f);

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
}

int
main(void)
{
    test_toolbar_action_bounds();
    test_bottom_icon_row_layout();
    test_icon_slider_popup_layout();
    return 0;
}

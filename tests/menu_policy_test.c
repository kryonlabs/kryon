#include <assert.h>
#include <math.h>

#include "runtime/menu.h"
#include "runtime/style.h"

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

int
main(void)
{
    StyleFrame panel = {0};
    StyleFrame item = {0};
    StyleFrame bar = {0};
    MenuMetrics metrics = MenuMetricsFor(2.0f, panel, item, bar);
    Rectangle bounds;
    Rectangle row;
    Rectangle bar_item;
    Vector2 origin;
    MenuLine line;
    MenuKeyboardInput keyboard_input;
    MenuKeyboardDecision keyboard_decision;
    MenuBarKeyboardDecision bar_keyboard_decision;
    int width;

    assert(metrics.row_height == 60);
    assert(metrics.panel_min_width == 360);
    assert(metrics.panel_padding == 24);
    assert(metrics.panel_margin == 8);
    assert(metrics.accelerator_gap == 176);
    assert(metrics.separator_inset == 16);
    assert(metrics.checked_mark_inset == 16);
    assert(metrics.label_inset == 56);
    assert(metrics.submenu_indicator_inset == 36);
    assert(metrics.bar_item_padding == 48);
    assert(metrics.bar_item_gap == 4);
    assert(metrics.bar_item_y_padding == 6);
    assert(metrics.bar_label_inset == 24);

    panel.value.fields = StylePaddingX | StyleGap | StyleContentOffset;
    panel.value.padding_x = 10.0f;
    panel.value.gap = 3.0f;
    panel.value.offset_x = 160.0f;
    item.value.fields = StyleFontSize | StylePaddingX | StylePaddingY |
                        StyleIconSize | StyleContentOffset;
    item.value.font_size = 18.0f;
    item.value.padding_x = 7.0f;
    item.value.padding_y = 6.0f;
    item.value.icon_size = 22.0f;
    item.value.offset_x = 70.0f;
    bar.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                       StyleIconSize;
    bar.value.padding_x = 9.0f;
    bar.value.padding_y = 2.0f;
    bar.value.gap = 5.0f;
    bar.value.icon_size = 11.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.row_height == 30);
    assert(metrics.panel_min_width == 160);
    assert(metrics.panel_padding == 10);
    assert(metrics.panel_margin == 3);
    assert(metrics.accelerator_gap == 70);
    assert(metrics.separator_inset == 7);
    assert(metrics.checked_mark_inset == 7);
    assert(metrics.label_inset == 22);
    assert(metrics.submenu_indicator_inset == 6);
    assert(metrics.bar_item_padding == 18);
    assert(metrics.bar_item_gap == 5);
    assert(metrics.bar_item_y_padding == 2);
    assert(metrics.bar_label_inset == 11);

    item.value.offset_y = 44.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.row_height == 44);
    item.value.offset_y = 0.0f;

    panel.value.padding_x = 0.0f;
    panel.value.gap = 0.0f;
    item.value.offset_x = 0.0f;
    item.value.padding_x = 0.0f;
    item.value.padding_y = 0.0f;
    item.value.icon_size = 0.0f;
    bar.value.gap = 0.0f;
    bar.value.icon_size = 0.0f;
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    assert(metrics.panel_padding == 0);
    assert(metrics.panel_margin == 0);
    assert(metrics.accelerator_gap == 0);
    assert(metrics.separator_inset == 0);
    assert(metrics.checked_mark_inset == 0);
    assert(metrics.label_inset == 0);
    assert(metrics.submenu_indicator_inset == 0);
    assert(metrics.bar_item_gap == 0);
    assert(metrics.bar_label_inset == 0);

    panel = (StyleFrame){0};
    item = (StyleFrame){0};
    bar = (StyleFrame){0};
    metrics = MenuMetricsFor(1.0f, panel, item, bar);
    width = MenuPanelWidthStep(0, 50, 20, true, metrics);
    assert(width == 182);
    bounds = MenuPanelBounds(0, 0, width, 3, metrics);
    check_rect(bounds, 0, 0, 182, 98);
    row = MenuRowBounds(bounds, 1, metrics);
    check_rect(row, 4, 34, 174, 30);
    assert(MenuSubmenuX(row) == 178);
    origin = MenuSubmenuOrigin(row);
    assert(fabsf(origin.x - 178.0f) < 0.001f);
    assert(fabsf(origin.y - 34.0f) < 0.001f);
    line = MenuSeparatorLineFor(row, metrics);
    assert(line.x1 == 12);
    assert(line.y1 == 49);
    assert(line.x2 == 170);
    assert(line.y2 == 49);
    assert(MenuTextY(row, 16) == 41);
    assert(MenuCheckedMarkX(row, metrics) == 12);
    assert(MenuLabelX(row, metrics) == 32);
    assert(MenuAcceleratorX(row, 20, metrics) == 146);
    assert(MenuSubmenuIndicatorX(row, metrics) == 160);
    assert(MenuGroupItemWidth(50, metrics) == 74);
    bar_item = MenuGroupItemBounds(10, (Rectangle){0, 0, 240, 30}, 74,
                                   metrics);
    check_rect(bar_item, 10, 3, 74, 24);
    assert(MenuBarFirstItemX((Rectangle){0, 0, 240, 30}, metrics) == 4);
    assert(MenuBarNextItemX(10, 74, metrics) == 86);
    assert(MenuBarLabelX(bar_item, metrics) == 22);
    assert(MenuBarLabelY(bar_item, 16) == 7);
    assert(MenuWrappedItemIndex(2, 1, 4) == 3);
    assert(MenuWrappedItemIndex(3, 1, 4) == 0);
    assert(MenuWrappedItemIndex(0, -1, 4) == 3);
    assert(MenuWrappedItemIndex(2, 0, 4) == 3);
    assert(MenuWrappedItemIndex(2, 1, 0) == -1);
    assert(!MenuItemSelectable(3, 0));
    assert(!MenuItemSelectable(0, 1));
    assert(MenuItemSelectable(0, 0));
    assert(MenuItemSelectable(4, 0));
    assert(MenuItemIsSeparator(3));
    assert(!MenuItemIsSeparator(4));
    assert(MenuItemShowsSubmenu(4));
    assert(!MenuItemShowsSubmenu(0));
    assert(MenuItemCanOpenSubmenu(4, 0, 1, 2, 0, 4));
    assert(!MenuItemCanOpenSubmenu(4, 1, 1, 2, 0, 4));
    assert(!MenuItemCanOpenSubmenu(4, 0, 0, 2, 0, 4));
    assert(!MenuItemCanOpenSubmenu(4, 0, 1, 0, 0, 4));
    assert(!MenuItemCanOpenSubmenu(4, 0, 1, 2, 3, 4));
    assert(MenuItemKeyboardActivates(0, 0, 0));
    assert(MenuItemKeyboardActivates(4, 0, 0));
    assert(!MenuItemKeyboardActivates(4, 0, 1));
    assert(!MenuItemKeyboardActivates(3, 0, 0));
    assert(!MenuItemKeyboardActivates(0, 1, 0));
    keyboard_input = MenuKeyboardInputFor(true, true, false, false,
                                          false, false, false, false, false);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, 2);
    assert(keyboard_decision.key_handled);
    assert(keyboard_decision.move_delta == -1);
    keyboard_input = MenuKeyboardInputFor(false, false, true, false,
                                          false, false, false, false, false);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, 2);
    assert(keyboard_decision.first);
    keyboard_input = MenuKeyboardInputFor(false, false, false, true,
                                          false, false, false, false, false);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, 2);
    assert(keyboard_decision.last);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          true, false, false, false, false);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 1, 2);
    assert(keyboard_decision.close_parent);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, 2);
    assert(!keyboard_decision.key_handled);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          false, true, false, false, false);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, 2);
    assert(keyboard_decision.open_or_activate);
    keyboard_decision = MenuKeyboardDecisionFor(keyboard_input, 0, -1);
    assert(!keyboard_decision.key_handled);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          true, false, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, false, 0);
    assert(bar_keyboard_decision.move_top_delta == -1);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          false, true, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, false, 0);
    assert(bar_keyboard_decision.move_top_delta == 1);
    keyboard_input = MenuKeyboardInputFor(false, false, true, false,
                                          false, false, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, false, 0);
    assert(bar_keyboard_decision.first_top);
    keyboard_input = MenuKeyboardInputFor(false, false, false, true,
                                          false, false, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, false, 0);
    assert(bar_keyboard_decision.last_top);
    keyboard_input = MenuKeyboardInputFor(false, true, false, false,
                                          false, false, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, false, 0);
    assert(bar_keyboard_decision.open_top);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          false, false, false, false, true);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, true, 0);
    assert(bar_keyboard_decision.close_open);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          true, false, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, true, 0);
    assert(bar_keyboard_decision.move_open_delta == -1);
    keyboard_input = MenuKeyboardInputFor(false, false, false, false,
                                          false, true, false, false, false);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, true, 0);
    assert(bar_keyboard_decision.move_open_if_no_submenu_delta == 1);
    bar_keyboard_decision = MenuBarKeyboardDecisionFor(keyboard_input, true, 1);
    assert(bar_keyboard_decision.move_open_if_no_submenu_delta == 0);
    assert(MenuItemPointerActivates(0, 0));
    assert(!MenuItemPointerActivates(4, 0));
    assert(!MenuItemPointerActivates(3, 0));
    assert(!MenuItemPointerActivates(0, 1));
    MenuItemPointerDecision pointer_decision =
        MenuItemPointerDecisionFor(true, true, false, 0, false, 42, 2, 1, 4);
    assert(pointer_decision.consume_release);
    assert(pointer_decision.set_focus);
    assert(pointer_decision.reset_navigation);
    assert(pointer_decision.set_navigation_path);
    assert(pointer_decision.navigation_depth == 1);
    assert(pointer_decision.navigation_index == 2);
    assert(pointer_decision.clear_child_navigation);
    assert(!pointer_decision.set_submenu);
    assert(pointer_decision.activate);
    assert(pointer_decision.activated_id == 42);
    assert(pointer_decision.close_open);
    pointer_decision =
        MenuItemPointerDecisionFor(true, true, true, 4, false, 43, 3, 2, 4);
    assert(pointer_decision.consume_release);
    assert(!pointer_decision.reset_navigation);
    assert(pointer_decision.set_navigation_path);
    assert(pointer_decision.set_submenu);
    assert(pointer_decision.submenu_id == 43);
    assert(!pointer_decision.activate);
    assert(!pointer_decision.close_open);
    pointer_decision =
        MenuItemPointerDecisionFor(true, true, true, 0, true, 44, 1, 1, 4);
    assert(pointer_decision.consume_release);
    assert(!pointer_decision.activate);
    assert(!pointer_decision.close_open);
    pointer_decision =
        MenuItemPointerDecisionFor(true, false, true, 0, false, 45, 1, 1, 4);
    assert(!pointer_decision.consume_release);
    assert(!pointer_decision.activate);
    pointer_decision =
        MenuItemPointerDecisionFor(true, true, true, 0, false, 46, 1, 4, 4);
    assert(!pointer_decision.set_navigation_path);
    assert(pointer_decision.activate);
    assert(MenuBarCountFor(-2, 8) == 0);
    assert(MenuBarCountFor(12, 8) == 8);
    assert(MenuBarCountFor(3, 8) == 3);
    assert(MenuBarOpenIndexFor(20, 21, 3) == 0);
    assert(MenuBarOpenIndexFor(20, 23, 3) == 2);
    assert(MenuBarOpenIndexFor(20, 24, 3) == -1);
    assert(MenuBarOpenIndexFor(0, 1, 3) == -1);
    assert(MenuBarOpenIdFor(20, 0, 3) == 21);
    assert(MenuBarOpenIdFor(20, 2, 3) == 23);
    assert(MenuBarOpenIdFor(20, 3, 3) == 0);
    assert(MenuBarOpenIdFor(0, 0, 3) == 0);
    assert(MenuBarTopIndexFor(-1, 3) == 0);
    assert(MenuBarTopIndexFor(3, 3) == 0);
    assert(MenuBarTopIndexFor(2, 3) == 2);
    assert(MenuBarTopIndexFor(0, 0) == -1);
    assert(MenuBarMoveTopIndex(0, 3, -1) == 2);
    assert(MenuBarMoveTopIndex(2, 3, 1) == 0);
    assert(MenuBarMoveTopIndex(1, 3, 0) == 1);
    assert(MenuBarMoveTopIndex(1, 0, 1) == -1);
    MenuGroupPointerDecision item_decision =
        MenuGroupPointerDecisionFor(21, 0, 0, true, true);
    assert(item_decision.changed_open);
    assert(item_decision.next_open_id == 21);
    assert(!item_decision.clear_submenu);
    assert(item_decision.consume_release);
    assert(item_decision.set_focus);
    assert(item_decision.reset_navigation);
    assert(item_decision.navigation_top == 0);
    item_decision = MenuGroupPointerDecisionFor(21, 0, 21, true, true);
    assert(item_decision.changed_open);
    assert(item_decision.next_open_id == 0);
    assert(item_decision.clear_submenu);
    assert(item_decision.consume_release);
    assert(item_decision.set_focus);
    assert(!item_decision.reset_navigation);
    item_decision = MenuGroupPointerDecisionFor(22, 1, 21, true, false);
    assert(item_decision.changed_open);
    assert(item_decision.next_open_id == 22);
    assert(item_decision.clear_submenu);
    assert(!item_decision.consume_release);
    assert(!item_decision.set_focus);
    assert(!item_decision.reset_navigation);
    item_decision = MenuGroupPointerDecisionFor(22, 1, 21, false, true);
    assert(!item_decision.changed_open);
    assert(item_decision.next_open_id == 21);
    MenuOutsideCloseDecision close_decision =
        MenuOutsideCloseDecisionFor(21, true, false, true, false);
    assert(close_decision.close_open);
    assert(close_decision.clear_submenu);
    assert(close_decision.consume_release);
    assert(close_decision.open_index == -1);
    close_decision = MenuOutsideCloseDecisionFor(21, true, true, true,
                                                false);
    assert(!close_decision.close_open);
    close_decision = MenuOutsideCloseDecisionFor(21, true, false, true,
                                                true);
    assert(!close_decision.close_open);
    close_decision = MenuOutsideCloseDecisionFor(0, true, false, false,
                                                false);
    assert(!close_decision.close_open);
    MenuContextOpenResult context_open =
        MenuContextOpenFor(false, true, false, true);
    assert(context_open.open);
    assert(context_open.changed);
    context_open = MenuContextOpenFor(true, false, true, true);
    assert(!context_open.open);
    assert(context_open.changed);
    context_open = MenuContextOpenFor(false, true, true, false);
    assert(!context_open.open);
    assert(!context_open.changed);
    return 0;
}

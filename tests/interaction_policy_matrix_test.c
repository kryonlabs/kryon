#include <assert.h>
#include <stdint.h>
#include <stddef.h>

#include "runtime/button.h"
#include "runtime/drag.h"
#include "runtime/drag_drop.h"
#include "runtime/focus.h"
#include "runtime/link.h"
#include "runtime/list_box.h"
#include "runtime/menu.h"
#include "runtime/popup_policy.h"
#include "runtime/reorder.h"
#include "runtime/text_input.h"

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
check_disabled_controls(void)
{
    LinkInteraction link;
    PopupDecision popup;

    assert(!ButtonActionEnabled(true, false));
    assert(!ButtonActionEnabled(false, true));
    assert(ButtonActionEnabled(false, false));

    link = LinkInteractionFor(true, false, true, true, true, false, true);
    assert(!link.active);
    assert(link.disabled_marker);
    assert(!link.activated);
    assert(!link.consume_release);
    assert(link.state == ButtonStateDisabled);

    popup = PopupDecisionFor(0, true);
    assert(popup.valid);
    assert(!popup.captures_input);
    assert(PopupOpenAfterDisabled(popup, true, true) == false);
}

static void
check_empty_data(void)
{
    ListBoxNavigation nav;
    ListBoxLayout layout;
    TextContextMenuState context;
    StyleFrame frame = {0};
    Rectangle bounds = {0, 0, 120, 80};

    nav = ListBoxNavigate(3, 0, ListBoxKeyDown(), 24, 20, 80, 100, 1.0f,
                          frame);
    assert(nav.selected == 3);
    assert(nav.scroll == 24);
    assert(!nav.changed);

    layout = ListBoxLayoutFor(bounds, 0, 20, 0, 99, 1.0f, frame);
    assert(layout.content_height == 0);
    assert(layout.max_scroll == 0);
    assert(layout.scroll == 0);

    context = TextContextMenuStateFor(false, true, false, false, true);
    assert(!context.cut_enabled);
    assert(!context.copy_enabled);
    assert(context.paste_enabled);
    assert(!context.select_all_enabled);
}

static void
check_simultaneous_keys(void)
{
    MenuKeyboardInput menu_input;
    MenuKeyboardDecision menu;
    DragKeyboardInput drag_input;
    DragStep drag;

    assert(FocusActivationFor(true, true, false, false, true, true, true));
    assert(!FocusActivationFor(true, true, false, false, false, true, true));
    assert(ListBoxKeyFor(true, true, true, true) == ListBoxKeyHome());
    assert(DragKeyboardDirectionFor(true, true) == 1);

    menu_input = MenuKeyboardInputFor(true, true, true, true, true, true,
                                      true, true, true);
    menu = MenuKeyboardDecisionFor(menu_input, 1, 0);
    assert(menu.key_handled);
    assert(menu.move_delta == -1);
    assert(!menu.open_or_activate);

    drag_input = DragKeyboardInputFor(1, true, true, true, true);
    drag = DragKeyboardValue(5.0f, 1.0f, 0.0f, 10.0f, drag_input);
    assert(drag.changed);
    assert(drag.value == 0.0f);
}

static void
check_release_without_press(void)
{
    LinkInteraction link;
    ListBoxRowDecision row;

    link = LinkInteractionFor(false, false, true, true, true, false, false);
    assert(link.active);
    assert(link.hovered);
    assert(!link.activated);
    assert(!link.consume_release);

    row = ListBoxRowDecisionFor(true, true, false, 2, 4);
    assert(!row.select);
    assert(!row.consume_release);
    assert(row.selected == 2);
}

static void
check_drag_cancellation(void)
{
    DragPointerDecision pointer;
    DragDropSourceDecision source;
    ReorderLifecycleDecision reorder;

    pointer = DragPointerDecisionFor(true, false, false, false, true, false,
                                     true, false);
    assert(pointer.clear_active);
    assert(!pointer.update_delta);
    assert(!pointer.finish_active);

    source = DragDropSourceDecisionFor(true, 7, 7, false, false, true, 5, 64,
                                       true, false, false, false, false);
    assert(source.clear_source);
    assert(source.valid);
    assert(!source.returns_active);

    reorder = ReorderActiveItemLifecycleFor(1, 3, true, true);
    assert(reorder.cancel_active);
    assert(reorder.ignore_list);
}

static void
check_popup_capture(void)
{
    PopupDecision plain = PopupDecisionFor(0, false);
    PopupDecision modal = PopupDecisionFor(2, false);
    PopupDecision tooltip = PopupDecisionFor(1, false);
    PopupDismissDecision dismiss;
    PopupEscapeDecision escape;

    assert(plain.captures_input);
    assert(modal.captures_input);
    assert(!tooltip.captures_input);

    dismiss = PopupDismissDecisionFor(plain, true, true, false);
    assert(!dismiss.close);
    assert(!dismiss.consume_release);

    escape = PopupEscapeFor(true, true, true);
    assert(!escape.close);
    assert(!escape.end_input);
}

static void
check_focus_loss(void)
{
    TextFocusReleaseDecision release;

    assert(!FocusActivationFor(true, true, false, true, true, false, false));
    assert(TextFocusOwnerIsStale(true, 8, 10));

    release = TextFocusReleaseDecisionFor(true, true, true, true, true, true);
    assert(release.release);
    assert(release.cancel_self);
    assert(release.clear_owner);
    assert(release.clear_frame_owner);
    assert(release.clear_active_focus);
    assert(release.clear_field_drag);
    assert(release.clear_area_drag);
}

static void
check_nested_ownership_restoration(void)
{
    TextFocusClaimDecision same_owner;
    TextFocusClaimDecision displaced_owner;
    TextFocusOwnerDecision owner;
    ReorderLifecycleDecision foreign;

    same_owner = TextFocusClaimDecisionFor(true, true, true);
    assert(same_owner.claim);
    assert(!same_owner.displace_previous);
    assert(!same_owner.cancel_previous);
    assert(!same_owner.clear_peer_selection);

    displaced_owner = TextFocusClaimDecisionFor(true, true, false);
    assert(displaced_owner.claim);
    assert(displaced_owner.displace_previous);
    assert(displaced_owner.cancel_previous);
    assert(displaced_owner.clear_peer_selection);

    owner = TextFocusOwnerDecisionFor(true, false, false, true, false);
    assert(owner.focused);
    assert(owner.adopt_owner);
    assert(owner.mark_frame_owner);
    assert(!owner.clear_target);

    owner = TextFocusOwnerDecisionFor(true, false, false, true, true);
    assert(!owner.focused);
    assert(!owner.adopt_owner);
    assert(owner.clear_target);

    foreign = ReorderForeignActiveListFor(true, 1, 2, false);
    assert(foreign.ignore_list);
    assert(foreign.cancel_active);

    foreign = ReorderForeignActiveListFor(true, 1, 2, true);
    assert(foreign.ignore_list);
    assert(!foreign.cancel_active);
}

int
main(void)
{
    check_disabled_controls();
    check_empty_data();
    check_simultaneous_keys();
    check_release_without_press();
    check_drag_cancellation();
    check_popup_capture();
    check_focus_loss();
    check_nested_ownership_restoration();
    return 0;
}

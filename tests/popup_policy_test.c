#include <assert.h>

#ifdef __cplusplus
#include "runtime/popup_policy.hpp"
#else
#include "runtime/popup_policy.h"
#endif

static void
check_decision(PopupDecision decision, int valid, int tooltip, int modal,
               int context, int captures_input, int requires_open,
               int requires_trigger)
{
    assert(decision.valid == valid);
    assert(decision.tooltip == tooltip);
    assert(decision.modal == modal);
    assert(decision.context == context);
    assert(decision.captures_input == captures_input);
    assert(decision.requires_open == requires_open);
    assert(decision.requires_trigger == requires_trigger);
}

int
check_helpers(void)
{
    PopupDecision plain = PopupDecisionFor(0, false);
    PopupDecision tooltip = PopupDecisionFor(1, false);
    PopupDecision modal = PopupDecisionFor(2, false);
    PopupDecision context = PopupDecisionFor(4, false);
    PopupDecision disabled_plain = PopupDecisionFor(0, true);
    PopupDecision invalid = PopupDecisionFor(1 | 2, false);
    PopupDismissDecision dismiss;
    Rectangle bounds = {10, 20, 30, 40};
    Rectangle trigger = {1, 2, 3, 4};
    Rectangle input;
    PopupContextActivation activation;

    check_decision(plain, 1, 0, 0, 0, 1, 1, 0);
    check_decision(tooltip, 1, 1, 0, 0, 0, 0, 1);
    check_decision(modal, 1, 0, 1, 0, 1, 1, 0);
    check_decision(context, 1, 0, 0, 1, 1, 1, 1);
    check_decision(disabled_plain, 1, 0, 0, 0, 0, 1, 0);
    assert(!invalid.valid);

    assert(PopupCanBegin(plain, 1, bounds, (Rectangle){0}, true));
    assert(!PopupCanBegin(plain, 0, bounds, (Rectangle){0}, true));
    assert(!PopupCanBegin(plain, 1, (Rectangle){0, 0, 0, 40},
                          (Rectangle){0}, true));
    assert(!PopupCanBegin(plain, 1, bounds, (Rectangle){0}, false));
    assert(PopupCanBegin(tooltip, 1, bounds, trigger, false));
    assert(!PopupCanBegin(tooltip, 1, bounds, (Rectangle){0}, false));

    assert(PopupOpenAfterDisabled(plain, true, false));
    assert(!PopupOpenAfterDisabled(plain, true, true));
    assert(PopupOpenAfterDisabled(tooltip, true, true));
    assert(PopupTooltipVisible(tooltip, false, true));
    assert(!PopupTooltipVisible(tooltip, true, true));
    assert(!PopupTooltipVisible(tooltip, false, false));
    assert(!PopupTooltipVisible(plain, false, true));

    input = PopupInputBounds(modal, bounds, 200, 100);
    assert(input.x == 0.0f && input.y == 0.0f);
    assert(input.width == 200.0f && input.height == 100.0f);
    input = PopupInputBounds(plain, bounds, 200, 100);
    assert(input.x == bounds.x && input.y == bounds.y);
    assert(input.width == bounds.width && input.height == bounds.height);

    activation = PopupContextActivationFor(context, trigger,
                                           (Vector2){2, 3}, false, false,
                                           true);
    assert(activation.open);
    assert(activation.origin.x == 2.0f && activation.origin.y == 3.0f);
    activation = PopupContextActivationFor(context, trigger,
                                           (Vector2){20, 3}, false, false,
                                           true);
    assert(!activation.open);
    activation = PopupContextActivationFor(context, trigger,
                                           (Vector2){2, 3}, true, false,
                                           true);
    assert(!activation.open);

    dismiss = PopupDismissDecisionFor(plain, true, false, false);
    assert(dismiss.close);
    assert(dismiss.consume_release);
    dismiss = PopupDismissDecisionFor(plain, true, false, true);
    assert(!dismiss.close);
    dismiss = PopupDismissDecisionFor(plain, true, true, false);
    assert(!dismiss.close);
    dismiss = PopupDismissDecisionFor(tooltip, true, false, false);
    assert(!dismiss.close);
    dismiss = PopupDismissDecisionFor(modal, true, false, false);
    assert(!dismiss.close);

    assert(PopupBackdropAlpha(modal) == 180);
    assert(PopupBackdropAlpha(plain) == 0);

    return 0;
}

static PopupFrameInput
frame_input(void)
{
    PopupFrameInput input = {0};
    input.id = 1;
    input.bounds = (Rectangle){80, 60, 100, 80};
    input.trigger = (Rectangle){10, 10, 40, 30};
    input.mouse = (Vector2){20, 20};
    input.has_open = true;
    input.open = true;
    input.view_width = 320;
    input.view_height = 240;
    return input;
}

static void
check_lifecycle(void)
{
    PopupFrameInput input = frame_input();
    PopupLifecycle plain = PopupLifecycleBegin(0, input);
    PopupLifecycle modal = PopupLifecycleBegin(2, input);
    PopupLifecycle state;
    assert(plain.eligible && plain.open && plain.visible);
    assert(plain.decision.captures_input && !plain.close_input);
    assert(plain.input_bounds.x == 80 && plain.input_bounds.width == 100);
    assert(plain.backdrop_alpha == 0);
    assert(modal.input_bounds.x == 0 && modal.input_bounds.width == 320);
    assert(modal.input_bounds.y == 0 && modal.input_bounds.height == 240);
    assert(modal.backdrop_alpha == 180);

    state = PopupLifecycleRelease(plain, false, false, false);
    assert(state.visible && !state.consume_release);
    state = PopupLifecycleRelease(plain, true, true, false);
    assert(state.visible && !state.consume_release);
    state = PopupLifecycleRelease(plain, true, false, true);
    assert(state.visible && !state.consume_release);
    state = PopupLifecycleRelease(modal, true, false, false);
    assert(state.visible && !state.consume_release);
    state = PopupLifecycleRelease(plain, true, false, false);
    assert(!state.visible && !state.open && state.close_input && state.consume_release);
    state = PopupLifecycleRelease(state, true, false, false);
    assert(!state.visible && !state.consume_release);

    state = PopupLifecycleKeyboard(plain, true, true);
    assert(state.visible && !state.close_input);
    state = PopupLifecycleKeyboard(plain, false, false);
    assert(state.visible);
    state = PopupLifecycleKeyboard(plain, true, false);
    assert(!state.visible && !state.open && state.close_input);
    state = PopupLifecycleKeyboard(modal, true, false);
    assert(!state.visible && state.close_input);
    state = PopupLifecycleFinish(plain, false, false);
    assert(!state.visible && !state.open && state.close_input);
    state = PopupLifecycleFinish(plain, true, true);
    assert(!state.visible && !state.open && state.close_input);

    input.open = false;
    state = PopupLifecycleBegin(0, input);
    assert(state.eligible && !state.visible && state.close_input);
    input.right_released = true;
    state = PopupLifecycleBegin(4, input);
    assert(state.visible && state.open && state.context_opened);
    input.trigger_blocked = true;
    state = PopupLifecycleBegin(4, input);
    assert(!state.visible && !state.context_opened);
    input.trigger_blocked = false;
    input.disabled = true;
    input.open = true;
    state = PopupLifecycleBegin(4, input);
    assert(!state.visible && !state.open && state.close_input);
    assert(!state.context_opened && !state.decision.captures_input);

    input = frame_input();
    input.has_open = false;
    input.open = false;
    state = PopupLifecycleBegin(1, input);
    assert(state.eligible && state.visible && state.open);
    assert(!state.decision.captures_input);
    state = PopupLifecycleRelease(state, true, false, false);
    state = PopupLifecycleKeyboard(state, true, false);
    assert(state.visible && !state.consume_release && !state.close_input);
    state = PopupLifecycleFinish(state, true, true);
    assert(!state.visible && !state.open && !state.close_input);
    input.trigger_blocked = true;
    state = PopupLifecycleBegin(1, input);
    assert(!state.visible && !state.close_input);
    input.trigger_blocked = false;
    input.mouse.x = 200;
    state = PopupLifecycleBegin(1, input);
    assert(!state.visible);
    input.mouse.x = 20;
    input.disabled = true;
    state = PopupLifecycleBegin(1, input);
    assert(!state.visible);

    input = frame_input();
    input.open = false;
    input.right_released = true;
    input.mouse.x = input.trigger.x + input.trigger.width;
    assert(!PopupLifecycleBegin(1, input).visible);
    assert(!PopupLifecycleBegin(4, input).context_opened);
    input.mouse.x = input.trigger.x;
    input.mouse.y = input.trigger.y + input.trigger.height;
    assert(!PopupLifecycleBegin(1, input).visible);
    assert(!PopupLifecycleBegin(4, input).context_opened);

    input = frame_input();
    input.has_open = false;
    state = PopupLifecycleBegin(0, input);
    assert(!state.eligible && !state.visible && !state.close_input);
    input = frame_input();
    input.id = 0;
    assert(!PopupLifecycleBegin(0, input).eligible);
    input = frame_input();
    input.bounds.width = 0;
    assert(!PopupLifecycleBegin(0, input).eligible);
    input = frame_input();
    assert(!PopupLifecycleBegin(3, input).decision.valid);
    input.trigger.height = 0;
    assert(!PopupLifecycleBegin(1, input).eligible);
}

int
main(void)
{
    check_helpers();
    check_lifecycle();
    return 0;
}

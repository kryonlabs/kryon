#include <assert.h>

#include "runtime/popup_policy.h"

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
main(void)
{
    PopupDecision plain = PopupDecisionFor(0, false);
    PopupDecision tooltip = PopupDecisionFor(1, false);
    PopupDecision modal = PopupDecisionFor(2, false);
    PopupDecision context = PopupDecisionFor(4, false);
    PopupDecision disabled_plain = PopupDecisionFor(0, true);
    PopupDecision invalid = PopupDecisionFor(1 | 2, false);
    PopupKeyboardInput popup_input;
    PopupEscapeDecision escape;
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

    escape = PopupEscapeFor(true, true, false);
    assert(escape.close);
    assert(escape.end_input);
    escape = PopupEscapeFor(false, true, false);
    assert(!escape.close);
    escape = PopupEscapeFor(true, false, false);
    assert(!escape.close);
    escape = PopupEscapeFor(true, true, true);
    assert(!escape.close);
    popup_input = PopupKeyboardInputFor(true);
    escape = PopupEscapeDecisionFor(true, popup_input, false);
    assert(escape.close);
    assert(escape.end_input);
    escape = PopupEscapeDecisionFor(true, popup_input, true);
    assert(!escape.close);
    return 0;
}

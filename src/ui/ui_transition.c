#include "ui_transition.h"
#include "runtime/transition_fade.h"

#include <stddef.h>

void
ResetTransition(TransitionState *transition)
{
    if(transition == NULL)
        return;
    transition->active = 0;
    transition->phase = TransitionNone;
    transition->elapsed_seconds = 0.0f;
    transition->duration_seconds = 0.0f;
}

void
BeginTransition(TransitionState *transition, float duration_seconds)
{
    if(transition == NULL)
        return;
    transition->active = 1;
    transition->phase = TransitionOut;
    transition->elapsed_seconds = 0.0f;
    transition->duration_seconds = TransitionDuration(duration_seconds);
}

void
ReverseTransitionToOut(TransitionState *transition)
{
    if(transition == NULL || !transition->active)
        return;
    if(transition->phase == TransitionIn) {
        transition->phase = TransitionOut;
        transition->elapsed_seconds = TransitionReverseElapsed(
            transition->duration_seconds, transition->elapsed_seconds);
    }
}

float
GetTransitionAlpha(const TransitionState *transition)
{
    if(transition == NULL)
        return 0.0f;
    return TransitionAlpha(transition->active != 0, transition->phase,
                           transition->elapsed_seconds,
                           transition->duration_seconds);
}

TransitionPhase
StepTransition(TransitionState *transition, float delta_seconds)
{
    if(transition == NULL || !transition->active)
        return TransitionNone;

    transition->elapsed_seconds += TransitionDelta(delta_seconds);
    if(transition->elapsed_seconds < transition->duration_seconds)
        return TransitionNone;

    if(transition->phase == TransitionOut) {
        transition->phase = TransitionIn;
        transition->elapsed_seconds = 0.0f;
        return TransitionOut;
    }

    ResetTransition(transition);
    return TransitionIn;
}

void
RenderTransitionFade(const TransitionState *transition,
                           int width, int height, Color color)
{
    int alpha;

    if(transition == NULL || !transition->active)
        return;
    alpha = TransitionFadeAlphaByte(transition->active != 0,
                                    transition->phase,
                                    transition->elapsed_seconds,
                                    transition->duration_seconds);
    if(alpha <= 0)
        return;
    color.a = (unsigned char)TransitionApplyAlpha((int)color.a, alpha);
    if(color.a == 0)
        return;
    DrawRectangle(0, 0, width, height, color);
}

#include "ui_transition.h"

#include <stddef.h>

static float
ClampTransitionProgress(float value)
{
    if(value < 0.0f)
        return 0.0f;
    if(value > 1.0f)
        return 1.0f;
    return value;
}

static float
SmoothTransitionProgress(float value)
{
    value = ClampTransitionProgress(value);
    return value * value * (3.0f - 2.0f * value);
}

void
ResetUITransition(UITransition *transition)
{
    if(transition == NULL)
        return;
    transition->active = 0;
    transition->phase = UI_TRANSITION_NONE;
    transition->elapsed_seconds = 0.0f;
    transition->duration_seconds = 0.0f;
}

void
BeginUITransition(UITransition *transition, float duration_seconds)
{
    if(transition == NULL)
        return;
    if(duration_seconds <= 0.0f)
        duration_seconds = 0.001f;
    transition->active = 1;
    transition->phase = UI_TRANSITION_OUT;
    transition->elapsed_seconds = 0.0f;
    transition->duration_seconds = duration_seconds;
}

void
ReverseUITransitionToOut(UITransition *transition)
{
    if(transition == NULL || !transition->active)
        return;
    if(transition->phase == UI_TRANSITION_IN) {
        transition->phase = UI_TRANSITION_OUT;
        transition->elapsed_seconds = transition->duration_seconds - transition->elapsed_seconds;
        if(transition->elapsed_seconds < 0.0f)
            transition->elapsed_seconds = 0.0f;
    }
}

float
GetUITransitionAlpha(const UITransition *transition)
{
    float progress;

    if(transition == NULL || !transition->active || transition->duration_seconds <= 0.0f)
        return 0.0f;
    progress = SmoothTransitionProgress(transition->elapsed_seconds / transition->duration_seconds);
    if(transition->phase == UI_TRANSITION_OUT)
        return progress;
    if(transition->phase == UI_TRANSITION_IN)
        return 1.0f - progress;
    return 0.0f;
}

int
StepUITransition(UITransition *transition, float delta_seconds)
{
    if(transition == NULL || !transition->active)
        return UI_TRANSITION_NONE;

    if(delta_seconds < 0.0f)
        delta_seconds = 0.0f;
    transition->elapsed_seconds += delta_seconds;
    if(transition->elapsed_seconds < transition->duration_seconds)
        return UI_TRANSITION_NONE;

    if(transition->phase == UI_TRANSITION_OUT) {
        transition->phase = UI_TRANSITION_IN;
        transition->elapsed_seconds = 0.0f;
        return UI_TRANSITION_OUT;
    }

    ResetUITransition(transition);
    return UI_TRANSITION_IN;
}

void
DrawUITransitionFade(const UITransition *transition,
                           int width, int height, Color color)
{
    int alpha;

    if(transition == NULL || !transition->active)
        return;
    alpha = (int)(GetUITransitionAlpha(transition) * 255.0f);
    if(alpha <= 0)
        return;
    if(alpha > 255)
        alpha = 255;
    color.a = (unsigned char)(((int)color.a * alpha) / 255);
    if(color.a == 0)
        return;
    DrawRectangle(0, 0, width, height, color);
}

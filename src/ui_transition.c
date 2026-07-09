#include "ui_transition.h"

#include <stddef.h>

void
ResetUITransition(UITransition *transition)
{
    if(transition == NULL)
        return;
    transition->active = 0;
    transition->phase = UI_TRANSITION_NONE;
    transition->ticks = 0;
    transition->duration = 0;
}

void
BeginUITransition(UITransition *transition, int duration)
{
    if(transition == NULL)
        return;
    if(duration <= 0)
        duration = 1;
    transition->active = 1;
    transition->phase = UI_TRANSITION_OUT;
    transition->ticks = 0;
    transition->duration = duration;
}

void
ReverseUITransitionToOut(UITransition *transition)
{
    if(transition == NULL || !transition->active)
        return;
    if(transition->phase == UI_TRANSITION_IN) {
        transition->phase = UI_TRANSITION_OUT;
        transition->ticks = transition->duration - transition->ticks;
        if(transition->ticks < 0)
            transition->ticks = 0;
    }
}

float
GetUITransitionAlpha(const UITransition *transition)
{
    float progress;

    if(transition == NULL || !transition->active || transition->duration <= 0)
        return 0.0f;
    progress = (float)transition->ticks / (float)transition->duration;
    if(progress < 0.0f)
        progress = 0.0f;
    if(progress > 1.0f)
        progress = 1.0f;
    if(transition->phase == UI_TRANSITION_OUT)
        return progress;
    if(transition->phase == UI_TRANSITION_IN)
        return 1.0f - progress;
    return 0.0f;
}

int
StepUITransition(UITransition *transition)
{
    if(transition == NULL || !transition->active)
        return UI_TRANSITION_NONE;

    transition->ticks++;
    if(transition->ticks < transition->duration)
        return UI_TRANSITION_NONE;

    if(transition->phase == UI_TRANSITION_OUT) {
        transition->phase = UI_TRANSITION_IN;
        transition->ticks = 0;
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

#ifndef UI_TRANSITION_H
#define UI_TRANSITION_H

#include "raylib.h"

typedef enum UITransitionPhase {
    UI_TRANSITION_NONE = 0,
    UI_TRANSITION_OUT = 1,
    UI_TRANSITION_IN = 2
} UITransitionPhase;

typedef struct UITransition {
    int active;
    int phase;
    float elapsed_seconds;
    float duration_seconds;
} UITransition;

void ResetUITransition(UITransition *transition);
void BeginUITransition(UITransition *transition, float duration_seconds);
void ReverseUITransitionToOut(UITransition *transition);
float GetUITransitionAlpha(const UITransition *transition);
int StepUITransition(UITransition *transition, float delta_seconds);
void DrawUITransitionFade(const UITransition *transition,
                                int width, int height, Color color);

#endif

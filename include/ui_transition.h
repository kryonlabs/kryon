#ifndef KRYON_TRANSITION_H
#define KRYON_TRANSITION_H

#include "kryon.h"

typedef enum TransitionPhase {
    TRANSITION_NONE = 0,
    TRANSITION_OUT = 1,
    TRANSITION_IN = 2
} TransitionPhase;

typedef struct TransitionState {
    int active;
    int phase;
    float elapsed_seconds;
    float duration_seconds;
} TransitionState;

void ResetTransition(TransitionState *transition);
void BeginTransition(TransitionState *transition, float duration_seconds);
void ReverseTransitionToOut(TransitionState *transition);
float GetTransitionAlpha(const TransitionState *transition);
int StepTransition(TransitionState *transition, float delta_seconds);

#endif

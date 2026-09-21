#ifndef KRYON_TRANSITION_H
#define KRYON_TRANSITION_H

#include "kryon.h"
#include "ui_transition_props.generated.h"

void ResetTransition(TransitionState *transition);
void BeginTransition(TransitionState *transition, float duration_seconds);
void ReverseTransitionToOut(TransitionState *transition);
float GetTransitionAlpha(const TransitionState *transition);
TransitionPhase StepTransition(TransitionState *transition, float delta_seconds);

#endif

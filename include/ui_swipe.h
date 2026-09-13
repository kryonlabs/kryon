#ifndef KRYON_SWIPE_H
#define KRYON_SWIPE_H

#include "ui_swipe_props.generated.h"

SwipeResult UpdateSwipe(SwipeGesture *gesture, SwipeSpec spec);
void ResetSwipe(SwipeGesture *gesture);

#endif

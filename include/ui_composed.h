#ifndef UI_COMPOSED_H
#define UI_COMPOSED_H
#include "kryon_compat.generated.h"

/* Stateless carousel navigation. Pass move=-1/+1 from a swipe or keyboard.
 * Image rendering and content remain owned by the caller. Count <= 1 hides
 * navigation. IDs reserve count+2 focus identities. Bounds are in UI pixels. */
typedef struct CarouselControlsProps {
    Rectangle bounds;
    Rectangle indicators;
    int count;
    int selected;
    int move;
    int disabled;
    int id;
} CarouselControlsProps;
int CarouselControls(CarouselControlsProps props);
#endif

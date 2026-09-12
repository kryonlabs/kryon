#ifndef CLIP_INTERNAL_H
#define CLIP_INTERNAL_H

#include "ui_clip.h"

#define CLIP_STACK_MAX 16
typedef struct ClipState {
    Rectangle bounds[CLIP_STACK_MAX];
    int count;
} ClipState;

ClipState ui_clip_save(void);
void ui_clip_restore(ClipState state);
int ui_clip_current(Rectangle *bounds);

#endif

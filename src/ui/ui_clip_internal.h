#ifndef UI_CLIP_INTERNAL_H
#define UI_CLIP_INTERNAL_H

#include "ui_clip.h"

#define UI_CLIP_STACK_MAX 16
typedef struct UIClipState {
    Rectangle bounds[UI_CLIP_STACK_MAX];
    int count;
} UIClipState;

UIClipState ui_clip_save(void);
void ui_clip_restore(UIClipState state);
int ui_clip_current(Rectangle *bounds);

#endif

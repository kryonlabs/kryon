#ifndef KRYON_UI_INPUT_CLIP_INTERNAL_H
#define KRYON_UI_INPUT_CLIP_INTERNAL_H
#include "kryon.h"
#define UI_INPUT_CLIP_DEPTH 16
typedef struct UIInputClipScope {
    Rectangle clips[UI_INPUT_CLIP_DEPTH];
    int count, scroll_depth;
} UIInputClipScope;
UIInputClipScope ui_input_clip_suspend(void);
void ui_input_clip_resume(UIInputClipScope scope);
#endif

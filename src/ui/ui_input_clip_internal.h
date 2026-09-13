#ifndef KRYON_UI_INPUT_CLIP_INTERNAL_H
#define KRYON_UI_INPUT_CLIP_INTERNAL_H
#include "kryon.h"
#define UI_INPUT_CLIP_DEPTH 16
typedef struct InputClipScopeState {
    Rectangle clips[UI_INPUT_CLIP_DEPTH];
    int count, scroll_depth;
} InputClipScopeState;
InputClipScopeState ui_input_clip_suspend(void);
void ui_input_clip_resume(InputClipScopeState scope);
#endif

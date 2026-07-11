#ifndef UI_CLIP_H
#define UI_CLIP_H

#include "flint.h"

Rectangle GetUIClipIntersection(Rectangle a, Rectangle b);
Rectangle GetUIClipEffective(Rectangle bounds);
void BeginUIClip(int x, int y, int w, int h);
void EndUIClip(void);
void ResetUIClip(void);

#endif

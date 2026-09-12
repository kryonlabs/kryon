#ifndef KRYON_CLIP_H
#define KRYON_CLIP_H

#include "kryon.h"

Rectangle GetClipIntersection(Rectangle a, Rectangle b);
Rectangle GetClipEffective(Rectangle bounds);
void BeginClip(int x, int y, int w, int h);
void EndClip(void);
void ResetClip(void);

#endif

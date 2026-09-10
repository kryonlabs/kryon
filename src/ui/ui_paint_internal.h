#ifndef UI_PAINT_INTERNAL_H
#define UI_PAINT_INTERNAL_H

#include "runtime/paint.h"
#include "runtime/material.h"

extern const SurfacePainter ui_surface_painter;
extern const Painter ui_painter;

void ui_draw(Drawing command);
void ui_draw_surface(SurfaceDrawing command);
void ui_draw_surface_direct(SurfaceDrawing command);
int ui_draw_surface_cached(SurfaceDrawing command);
void ui_surface_cache_shutdown(void);

#endif

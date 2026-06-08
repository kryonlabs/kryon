#ifndef FLINT_UI_H
#define FLINT_UI_H

#include "raylib.h"

void flint_ui_init(int width, int height, float dpi);
void flint_ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark);
void flint_ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color);
int flint_ui_icon_btn_size(int size);
int flint_ui_icon_btn_padding(int size);

#endif

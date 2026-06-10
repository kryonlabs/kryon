#ifndef FLINT_TEXT_H
#define FLINT_TEXT_H

#include "raylib.h"

void flint_text_set_font(Font font);
Font flint_text_font(void);
int flint_text_measure(const char *text, int font_size);
int flint_text_measure_scaled(const char *text, int scale);
void flint_text_draw(const char *text, int x, int y, int font_size, Color color);
void flint_text_draw_scaled(const char *text, int x, int y, int scale, Color color);
void flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color);
int flint_text_y(const char *text, int box_y, int box_h, int font_size);
int flint_text_y_scaled(const char *text, int box_y, int box_h, int scale);

#endif

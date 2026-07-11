#ifndef UI_TEXT_H
#define UI_TEXT_H

#include "flint.h"

#define UI_TEXT_8 8
#define UI_TEXT_12 12
#define UI_TEXT_16 16
#define UI_TEXT_24 24
#define UI_TEXT_BASE_SIZE 16

void SetUIFont(Font font);
void SetUISmallFont(Font font);
Font GetUIFont(void);
Font LoadUIChoppedFont(const char *png_path, const char *dat_path, int base_size);
Font LoadUIChoppedFontFromMemory(const unsigned char *png_data, unsigned int png_size, const unsigned char *dat_data, unsigned int dat_size, int base_size);
void UnloadUIFont(Font *font);
int MeasureUIText(const char *text, int font_size);
int GetUITextHeight(const char *text, int font_size);
int GetUITextLineHeight(int font_size);
int MeasureScaledUIText(const char *text, int scale);
void DrawUIText(const char *text, int x, int y, int font_size, Color color);
void DrawScaledUIText(const char *text, int x, int y, int scale, Color color);
void DrawCenteredUIText(const char *text, int center_x, int center_y, int font_size, Color color);
void DrawUITextInRect(const char *text, Rectangle rect, int font_size, Color color);
int GetUITextY(const char *text, int box_y, int box_h, int font_size);
int GetScaledUITextY(const char *text, int box_y, int box_h, int scale);

#endif

#ifndef UI_GRAPHEME_H
#define UI_GRAPHEME_H

int ui_grapheme_floor_offset(const char *text, int offset);
int ui_grapheme_next_offset(const char *text, int offset);
int ui_grapheme_prev_offset(const char *text, int offset);
int ui_grapheme_next_boundary(const char *text, int length, int offset);
int ui_utf8_valid(const char *text, int length);

#endif

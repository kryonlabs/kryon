#ifndef UI_TEXT_LAYOUT_H
#define UI_TEXT_LAYOUT_H

#include "raylib.h"
#include "ui_icon_types.h"
#include "ui_text.h"

typedef enum {
    UI_TEXT_ELEMENT_TEXT,
    UI_TEXT_ELEMENT_ICON,
    UI_TEXT_ELEMENT_LINE_BREAK
} UITextElementType;

typedef struct {
    UITextElementType type;
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int text_width;
} UITextElement;

typedef struct UITextLayout {
    UITextElement *elements;
    int element_count;
    int *line_breaks;
    int line_count;
    int *line_widths;
    int total_height;
    int line_height;  /* Store line height from reflow for use in draw */
    int last_reflow_width;
} UITextLayout;

UITextLayout ParseUITextLayout(const char *input, Texture2D icon, UIIconType icon_type, int icon_size);
void ReflowUITextLayout(UITextLayout *layout, int max_width, int font_size, int line_height);
void DrawUITextLayout(UITextLayout *layout, int x, int *y, int font_size, Color color);
int GetUITextLayoutHeight(UITextLayout *layout);
void FreeUITextLayout(UITextLayout *layout);

#endif

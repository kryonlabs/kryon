#include "ui_text_layout.h"

#include "ui_scaling.h"
#include "ui.h"

#include <stdlib.h>
#include <string.h>

UITextLayout
ParseUITextLayout(const char *input, Texture2D icon, UIIconType icon_type, int icon_size)
{
    UITextLayout layout = {0};

    if(input == NULL || input[0] == '\0')
        return layout;

    int element_count = 0;
    const char *p = input;
    while(*p) {
        if(strncmp(p, "%i", 2) == 0) {
            element_count++;
            p += 2;
        } else if(*p == '\n') {
            element_count++;
            p++;
        } else {
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0)
                p++;
            if(p > word_start)
                element_count++;
            if(*p == ' ')
                p++;
        }
    }

    layout.elements = (UITextElement *)calloc((size_t)element_count, sizeof(UITextElement));
    if(layout.elements == NULL)
        return layout;
    layout.element_count = element_count;

    int element_idx = 0;
    p = input;
    while(*p && element_idx < element_count) {
        if(strncmp(p, "%i", 2) == 0) {
            layout.elements[element_idx].type = UI_TEXT_ELEMENT_ICON;
            layout.elements[element_idx].icon = icon;
            layout.elements[element_idx].icon_type = icon_type;
            layout.elements[element_idx].icon_size = icon_size;
            element_idx++;
            p += 2;
        } else if(*p == '\n') {
            layout.elements[element_idx].type = UI_TEXT_ELEMENT_LINE_BREAK;
            element_idx++;
            p++;
        } else {
            const char *word_start = p;
            while(*p && *p != ' ' && *p != '\n' && strncmp(p, "%i", 2) != 0)
                p++;
            if(p > word_start) {
                size_t len = (size_t)(p - word_start);
                char *text_copy = (char *)malloc(len + 1);
                if(text_copy != NULL) {
                    memcpy(text_copy, word_start, len);
                    text_copy[len] = '\0';
                    layout.elements[element_idx].type = UI_TEXT_ELEMENT_TEXT;
                    layout.elements[element_idx].text = text_copy;
                    element_idx++;
                }
            }
            if(*p == ' ')
                p++;
        }
    }

    return layout;
}

void
ReflowUITextLayout(UITextLayout *layout, int max_width, int font_size, int line_height)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0)
        return;

    int *new_line_breaks = (int *)calloc((size_t)layout->element_count + 1, sizeof(int));
    int *new_line_widths = (int *)calloc((size_t)layout->element_count + 1, sizeof(int));
    if(new_line_breaks == NULL || new_line_widths == NULL) {
        free(new_line_breaks);
        free(new_line_widths);
        return;
    }

    free(layout->line_breaks);
    free(layout->line_widths);
    layout->line_breaks = new_line_breaks;
    layout->line_widths = new_line_widths;

    /* Initialize all line_breaks to -1 (invalid) except line_breaks[0] */
    for(int i = 1; i < layout->element_count + 1; i++) {
        layout->line_breaks[i] = -1;
    }

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == UI_TEXT_ELEMENT_TEXT && layout->elements[i].text != NULL)
            layout->elements[i].text_width = MeasureUIText(layout->elements[i].text, font_size);
    }

    int space_width = MeasureUIText(" ", font_size);
    int icon_spacing = ScaleUIPx(4);
    layout->line_count = 0;
    layout->line_breaks[0] = 0;
    int current_line_width = 0;

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == UI_TEXT_ELEMENT_LINE_BREAK) {
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = 0;
            continue;
        }

        int element_width = 0;
        int spacing = 0;
        if(layout->elements[i].type == UI_TEXT_ELEMENT_TEXT) {
            element_width = layout->elements[i].text_width;
            spacing = (current_line_width > 0) ? space_width : 0;
        } else {
            element_width = layout->elements[i].icon_size;
            spacing = (current_line_width > 0) ? icon_spacing : 0;
        }

        if(current_line_width + spacing + element_width <= max_width) {
            current_line_width += spacing + element_width;
        } else {
            layout->line_count++;
            layout->line_breaks[layout->line_count] = i;
            layout->line_widths[layout->line_count - 1] = current_line_width;
            current_line_width = element_width;
        }
    }

    layout->line_widths[layout->line_count] = current_line_width;
    layout->line_count++;
    {
        int drawn_line_height = GetUITextLineHeight(font_size);
        layout->total_height = layout->line_count > 0
                                   ? layout->line_count * drawn_line_height +
                                     (layout->line_count - 1) * line_height
                                   : 0;
    }
    layout->line_height = line_height;  /* Store for later use in draw */
}

static int
ui_text_layout_line_text_len(UITextLayout *layout, int start, int end)
{
    int len = 0;
    int text_count = 0;

    for(int i = start; i < end; i++) {
        UITextElement *element = &layout->elements[i];

        if(element->type == UI_TEXT_ELEMENT_LINE_BREAK)
            continue;
        if(element->type != UI_TEXT_ELEMENT_TEXT)
            return -1;
        if(element->text == NULL || element->text[0] == '\0')
            continue;
        if(text_count > 0)
            len++;
        len += (int)strlen(element->text);
        text_count++;
    }

    return len;
}

static void
ui_text_layout_draw_text_line(UITextLayout *layout, int start, int end,
                              int x, int y, int font_size, Color color)
{
    int len = ui_text_layout_line_text_len(layout, start, end);
    char *line;
    int offset = 0;
    int text_count = 0;

    if(len <= 0)
        return;

    line = (char *)malloc((size_t)len + 1);
    if(line == NULL)
        return;

    for(int i = start; i < end; i++) {
        UITextElement *element = &layout->elements[i];
        int element_len;

        if(element->type != UI_TEXT_ELEMENT_TEXT ||
           element->text == NULL || element->text[0] == '\0')
            continue;
        if(text_count > 0)
            line[offset++] = ' ';
        element_len = (int)strlen(element->text);
        memcpy(line + offset, element->text, (size_t)element_len);
        offset += element_len;
        text_count++;
    }
    line[offset] = '\0';
    DrawUIText(line, x, y, font_size, color);
    free(line);
}

static void
ui_text_layout_draw_mixed_line(UITextLayout *layout, int start, int end,
                               int x, int y, int font_size, Color color,
                               int space_width, int icon_spacing)
{
    int current_x = x;

    for(int i = start; i < end; i++) {
        int spacing = 0;

        if(layout->elements[i].type == UI_TEXT_ELEMENT_LINE_BREAK)
            continue;

        if(current_x > x)
            spacing = (layout->elements[i].type == UI_TEXT_ELEMENT_TEXT) ? space_width : icon_spacing;
        current_x += spacing;

        if(layout->elements[i].type == UI_TEXT_ELEMENT_TEXT) {
            if(layout->elements[i].text != NULL && layout->elements[i].text[0] != '\0') {
                DrawUIText(layout->elements[i].text, current_x, y, font_size, color);
                current_x += layout->elements[i].text_width;
            }
        } else {
            Texture2D icon = layout->elements[i].icon;
            int icon_size = layout->elements[i].icon_size;
            if(icon.id != 0) {
                Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
                Rectangle dst = {(float)current_x, (float)y, (float)icon_size, (float)icon_size};
                DrawTexturePro(icon, src, dst, (Vector2){0}, 0, color);
            }
            current_x += icon_size;
        }
    }
}

void
DrawUITextLayout(UITextLayout *layout, int x, int *y, int font_size, Color color)
{
    if(layout == NULL || layout->elements == NULL || layout->element_count == 0)
        return;

    int current_y = *y;
    int space_width = MeasureUIText(" ", font_size);
    int icon_spacing = ScaleUIPx(4);
    int line_spacing = (layout->line_height > 0) ? layout->line_height : ScaleUIPx(4);
    int drawn_line_height = GetUITextLineHeight(font_size);
    int line_count = layout->line_count > 0 ? layout->line_count : 1;

    for(int line = 0; line < line_count; line++) {
        int start = (layout->line_breaks != NULL && layout->line_breaks[line] >= 0)
                        ? layout->line_breaks[line] : 0;
        int end = (layout->line_breaks != NULL && line + 1 < line_count &&
                   layout->line_breaks[line + 1] >= 0)
                      ? layout->line_breaks[line + 1] : layout->element_count;

        if(start < 0)
            start = 0;
        if(end < start)
            end = start;
        if(end > layout->element_count)
            end = layout->element_count;

        if(ui_text_layout_line_text_len(layout, start, end) >= 0)
            ui_text_layout_draw_text_line(layout, start, end, x, current_y,
                                          font_size, color);
        else
            ui_text_layout_draw_mixed_line(layout, start, end, x, current_y,
                                           font_size, color, space_width,
                                           icon_spacing);
        current_y += drawn_line_height;
        if(line + 1 < line_count)
            current_y += line_spacing;
    }

    *y = current_y;
}

int
GetUITextLayoutHeight(UITextLayout *layout)
{
    if(layout == NULL)
        return 0;
    return layout->total_height;
}

void
FreeUITextLayout(UITextLayout *layout)
{
    if(layout == NULL)
        return;

    if(layout->elements != NULL) {
        for(int i = 0; i < layout->element_count; i++) {
            if(layout->elements[i].type == UI_TEXT_ELEMENT_TEXT && layout->elements[i].text != NULL)
                free((void *)layout->elements[i].text);
        }
        free(layout->elements);
    }
    free(layout->line_breaks);
    free(layout->line_widths);
    memset(layout, 0, sizeof(*layout));
}

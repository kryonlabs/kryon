#include "ui_text_layout.h"

#include "ui_icons.h"
#include "ui_scaling.h"
#include "ui_internal.h"
#include "runtime/paragraph.h"

#include <stdlib.h>
#include <string.h>
#include <limits.h>

/* zero constants: the native Plan 9 compiler rejects short
 * compound literals like (Type){0}, and a copy of a zero
 * object is equivalent on every platform. */
static const Vector2 kryon_zero_vector2;


TextLayout
ParseTextLayout(const char *input, Texture2D icon, IconType icon_type, int icon_size)
{
    TextLayout layout = {0};

    if(input == NULL)
        return layout;
    size_t input_length = strlen(input);
    if(input_length >= INT_MAX)
        return layout;
    String source = StringView(input, input_length);
    bool icons = icon.id != 0 || icon_type != ICON_NONE;
    int element_count = 0;
    ParagraphToken token = ParagraphTokenNext(source, 0, icons);
    while(token.valid) {
        element_count++;
        token = ParagraphTokenNext(source, token.next, icons);
    }

    /* Even empty text has one logical line after reflow. */
    layout.elements = calloc((size_t)element_count + 1, sizeof(TextElement));
    if(layout.elements == NULL)
        return layout;

    token = ParagraphTokenNext(source, 0, icons);
    while(token.valid) {
        TextElement *element = &layout.elements[layout.element_count];
        if(token.icon) {
            element->type = TEXT_ELEMENT_ICON;
            element->icon = icon;
            element->icon_type = icon_type;
            element->icon_size = icon_size;
        } else if(token.line_break) {
            element->type = TEXT_ELEMENT_LINE_BREAK;
        } else {
            size_t length = (size_t)(token.end - token.start);
            char *text = malloc(length + 1);
            if(text == NULL) {
                FreeTextLayout(&layout);
                return layout;
            }
            memcpy(text, input + token.start, length);
            text[length] = '\0';
            element->type = TEXT_ELEMENT_TEXT;
            element->text = text;
        }
        layout.element_count++;
        token = ParagraphTokenNext(source, token.next, icons);
    }

    return layout;
}

void
ReflowTextLayout(TextLayout *layout, int max_width, int font_size, int line_height)
{
    if(layout == NULL || layout->elements == NULL)
        return;

    size_t text_capacity = (size_t)layout->element_count + 1;
    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_TEXT && layout->elements[i].text != NULL)
            text_capacity += strlen(layout->elements[i].text);
    }
    char *candidate = malloc(text_capacity);
    int *new_line_breaks = calloc((size_t)layout->element_count + 1, sizeof(int));
    int *new_line_widths = calloc((size_t)layout->element_count + 1, sizeof(int));
    if(candidate == NULL || new_line_breaks == NULL || new_line_widths == NULL) {
        free(candidate);
        free(new_line_breaks);
        free(new_line_widths);
        return;
    }

    free(layout->line_breaks);
    free(layout->line_widths);
    layout->line_breaks = new_line_breaks;
    layout->line_widths = new_line_widths;

    for(int i = 0; i < layout->element_count; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_TEXT && layout->elements[i].text != NULL)
            layout->elements[i].text_width = TextWidth(layout->elements[i].text, font_size);
    }

    ParagraphLayoutPolicy policy = ParagraphLayoutPolicyFor(
        TextWidth(" ", font_size), ui_get_text_letter_spacing(),
        line_height, ParagraphDefaultLineGap((float)Scale(1000) / 1000.0f),
        (float)Scale(1000) / 1000.0f);
    layout->line_count = 0;
    ParagraphLine line = {0};
    size_t candidate_length = 0;
    bool has_icon = false;

    for(int i = 0; i <= layout->element_count; i++) {
        bool end = i == layout->element_count;
        TextElement *element = end ? NULL : &layout->elements[i];
        bool line_break = !end && element->type == TEXT_ELEMENT_LINE_BREAK;
        bool icon_element = !end && element->type == TEXT_ELEMENT_ICON;
        const char *text = !end && element->type == TEXT_ELEMENT_TEXT &&
            element->text != NULL ? element->text : "";
        size_t length = strlen(text);
        int element_width = 0;
        float candidate_width = 0;
        if(!end && !line_break) {
            element_width = icon_element ? element->icon_size : element->text_width;
            if(!icon_element && !has_icon) {
                String separator = ParagraphTextSeparator(line);
                if(separator.length > 0)
                    memcpy(candidate + candidate_length, separator.data, separator.length);
                candidate_length += separator.length;
                memcpy(candidate + candidate_length, text, length + 1);
                candidate_length += length;
                candidate_width = (float)TextWidth(candidate, font_size);
            } else {
                candidate_width = line.width + (float)(element_width +
                    ParagraphElementSpacing(line.has_content, icon_element, policy));
            }
        }
        ParagraphLineDecision decision = ParagraphLineAdvance(line, i,
            line_break, end, (float)element_width, candidate_width, (float)max_width);
        if(decision.emit) {
            layout->line_breaks[layout->line_count] = decision.line.start;
            layout->line_widths[layout->line_count] = (int)decision.line.width;
            layout->line_count++;
            candidate_length = 0;
            has_icon = false;
            if(decision.next.has_content && !icon_element) {
                memcpy(candidate, text, length + 1);
                candidate_length = length;
            }
        }
        line = decision.next;
        has_icon = has_icon || icon_element;
    }
    free(candidate);
    {
        int drawn_line_height = TextLineHeight(font_size);
        layout->total_height = ParagraphLayoutTotalHeight(
            layout->line_count, drawn_line_height, policy.line_gap);
    }
    layout->line_height = policy.line_gap;  /* Store for later use in draw */
}

static int
ui_text_layout_line_text_len(TextLayout *layout, int start, int end)
{
    int len = 0;
    int text_count = 0;

    for(int i = start; i < end; i++) {
        TextElement *element = &layout->elements[i];

        if(element->type == TEXT_ELEMENT_LINE_BREAK)
            continue;
        if(element->type != TEXT_ELEMENT_TEXT)
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
ui_text_layout_draw_text_line(TextLayout *layout, int start, int end,
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
        TextElement *element = &layout->elements[i];
        int element_len;

        if(element->type != TEXT_ELEMENT_TEXT ||
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
    RenderText(line, x, y, font_size, color);
    free(line);
}

static void
ui_text_layout_draw_mixed_line(TextLayout *layout, int start, int end,
                               int x, int y, int font_size, Color color,
                               int space_width, int icon_spacing)
{
    int current_x = x;

    for(int i = start; i < end; i++) {
        if(layout->elements[i].type == TEXT_ELEMENT_LINE_BREAK)
            continue;

        ParagraphLayoutPolicy policy = {space_width, icon_spacing, 0};
        current_x += ParagraphElementSpacing(i > start,
            layout->elements[i].type == TEXT_ELEMENT_ICON, policy);

        if(layout->elements[i].type == TEXT_ELEMENT_TEXT) {
            if(layout->elements[i].text != NULL && layout->elements[i].text[0] != '\0') {
                RenderText(layout->elements[i].text, current_x, y, font_size, color);
                current_x += layout->elements[i].text_width;
            }
        } else {
            Texture2D icon = layout->elements[i].icon;
            int icon_size = layout->elements[i].icon_size;
            if(icon.id != 0) {
                Rectangle src = {0, 0, (float)icon.width, (float)icon.height};
                Rectangle dst = {(float)current_x, (float)y, (float)icon_size, (float)icon_size};
                DrawTexturePro(icon, src, dst, kryon_zero_vector2, 0, color);
            } else if(layout->elements[i].icon_type != ICON_NONE) {
                Rectangle dst = {(float)current_x, (float)y,
                                 (float)icon_size, (float)icon_size};
                DrawIcon(layout->elements[i].icon_type, dst, color);
            }
            current_x += icon_size;
        }
    }
}

void
DrawTextLayout(TextLayout *layout, int x, int *y, int font_size, Color color)
{
    DrawTextLayoutAligned(layout, x, y, font_size, color, 0, 0);
}

void
DrawTextLayoutAligned(TextLayout *layout, int x, int *y, int font_size,
                      Color color, int width, int align)
{
    if(layout == NULL || layout->elements == NULL)
        return;

    int current_y = *y;
    ParagraphLayoutPolicy policy = ParagraphLayoutPolicyFor(
        TextWidth(" ", font_size), ui_get_text_letter_spacing(),
        layout->line_height,
        ParagraphDefaultLineGap((float)Scale(1000) / 1000.0f),
        (float)Scale(1000) / 1000.0f);
    int drawn_line_height = TextLineHeight(font_size);
    int line_count = layout->line_count > 0 ? layout->line_count : 1;

    for(int line = 0; line < line_count; line++) {
        int line_x = x;
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

        if(width > 0 && layout->line_widths != NULL)
            line_x = ParagraphLineXFor(x, width, layout->line_widths[line],
                                       (TextAlign)align);

        if(ui_text_layout_line_text_len(layout, start, end) >= 0)
            ui_text_layout_draw_text_line(layout, start, end, line_x, current_y,
                                          font_size, color);
        else
            ui_text_layout_draw_mixed_line(layout, start, end, line_x, current_y,
                                           font_size, color, policy.space_width,
                                           policy.icon_spacing);
        current_y = ParagraphNextLineY(current_y, drawn_line_height,
                                       policy.line_gap, line + 1 < line_count);
    }

    *y = current_y;
}

int
GetTextLayoutHeight(TextLayout *layout)
{
    if(layout == NULL)
        return 0;
    return layout->total_height;
}

void
FreeTextLayout(TextLayout *layout)
{
    if(layout == NULL)
        return;

    if(layout->elements != NULL) {
        for(int i = 0; i < layout->element_count; i++) {
            if(layout->elements[i].type == TEXT_ELEMENT_TEXT && layout->elements[i].text != NULL)
                free((void *)layout->elements[i].text);
        }
        free(layout->elements);
    }
    free(layout->line_breaks);
    free(layout->line_widths);
    memset(layout, 0, sizeof(*layout));
}

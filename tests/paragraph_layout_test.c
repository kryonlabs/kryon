#include "ui_text_layout.h"
#include "../src/ui/ui_internal.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* A deterministic measurement/paint host for the actual C layout adapter. */
int
TextWidth(const char *text, int font)
{
    int width = 0;
    (void)font;
    for(const unsigned char *p = (const unsigned char *)text; *p; p++) {
        if((*p & 0xc0) != 0x80 && *p != '~')
            width++;
        if(strncmp((const char *)p, "A V", 3) == 0)
            width--;
    }
    return width;
}

int
TextLineHeight(int font)
{
    (void)font;
    return 10;
}

int
Scale(int value)
{
    return value;
}

int
ui_get_text_letter_spacing(void)
{
    return 0;
}

static int paint_width;
static int paint_align;

void
RenderText(const char *text, int x, int y, int font, Color color)
{
    int width = TextWidth(text, font);
    (void)color;
    assert(y >= 20);
    if(paint_align == TextAlignEnd && paint_width > 0)
        assert(x == 30 + paint_width - width);
}

void
DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
               Vector2 origin, float rotation, Color color)
{
    (void)texture;
    (void)source;
    (void)dest;
    (void)origin;
    (void)rotation;
    (void)color;
}

void
DrawIcon(IconType icon, Rectangle bounds, Color color)
{
    (void)icon;
    (void)bounds;
    (void)color;
}

static void
check_inline_icons(void)
{
    Texture2D texture = {0};
    texture.id = 1;
    TextLayout layout = ParseTextLayout("a%i b\n%i", texture, ICON_NONE, 2);
    assert(layout.element_count == 5);
    assert(layout.elements[1].type == TEXT_ELEMENT_ICON);
    ReflowTextLayout(&layout, 5, 16, 4);
    assert(layout.line_count == 3);
    assert(layout.line_breaks[0] == 0 && layout.line_widths[0] == 1);
    assert(layout.line_breaks[1] == 1 && layout.line_widths[1] == 4);
    assert(layout.line_breaks[2] == 4 && layout.line_widths[2] == 2);
    FreeTextLayout(&layout);
    layout = ParseTextLayout(NULL, texture, ICON_NONE, 2);
    ReflowTextLayout(&layout, 5, 16, 4);
    assert(layout.elements == NULL && layout.line_count == 0);
}

int
main(int argc, char **argv)
{
    assert(argc == 3);
    check_inline_icons();
    int width = (int)strtod(argv[2], NULL);
    Texture2D texture = {0};
    TextLayout layout = ParseTextLayout(argv[1], texture, ICON_NONE, 10);
    assert(layout.elements != NULL);
    ReflowTextLayout(&layout, width, 16, 4);
    assert(layout.line_count >= 1);
    assert(layout.total_height == layout.line_count * 10 + (layout.line_count - 1) * 4);
    for(int line = 0; line < layout.line_count; line++) {
        int start = layout.line_breaks[line];
        int end = line + 1 < layout.line_count ? layout.line_breaks[line + 1] : layout.element_count;
        int count = 0;
        for(int i = start; i < end; i++) {
            TextElement *element = &layout.elements[i];
            if(element->type != TEXT_ELEMENT_TEXT)
                continue;
            if(count++)
                putchar(' ');
            fputs(element->text, stdout);
        }
        putchar('\n');
    }
    paint_width = width;
    paint_align = TextAlignEnd;
    int y = 20;
    DrawTextLayoutAligned(&layout, 30, &y, 16, (Color){255, 255, 255, 255}, width, paint_align);
    assert(y == 20 + layout.total_height);
    FreeTextLayout(&layout);
    assert(layout.elements == NULL && layout.line_count == 0);
    return 0;
}

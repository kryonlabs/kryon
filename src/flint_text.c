#include "flint_text.h"

#include <stddef.h>

static Font g_flint_text_font = {0};

static Font
active_font(void)
{
    if(g_flint_text_font.texture.id != 0 && g_flint_text_font.glyphs != NULL &&
       g_flint_text_font.recs != NULL && g_flint_text_font.glyphCount > 0 &&
       g_flint_text_font.baseSize > 0)
        return g_flint_text_font;

    return GetFontDefault();
}

static float
font_spacing(Font font, int font_size)
{
    if(font.baseSize <= 0)
        return 1.0f;

    float spacing = (float)font_size / (float)font.baseSize;
    return spacing > 0.0f ? spacing : 1.0f;
}

void
flint_text_set_font(Font font)
{
    g_flint_text_font = font;
}

Font
flint_text_font(void)
{
    return active_font();
}

int
flint_text_measure(const char *text, int font_size)
{
    Font font = active_font();
    Vector2 size;

    if(text == NULL || font.texture.id == 0)
        return 0;

    size = MeasureTextEx(font, text, (float)font_size, font_spacing(font, font_size));
    return (int)size.x;
}

void
flint_text_draw(const char *text, int x, int y, int font_size, Color color)
{
    Font font = active_font();

    if(text == NULL || font.texture.id == 0)
        return;

    DrawTextEx(font, text, (Vector2){(float)x, (float)y}, (float)font_size, font_spacing(font, font_size), color);
}

void
flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color)
{
    int text_w = flint_text_measure(text, font_size);
    int y = flint_text_y(text, center_y - font_size / 2, font_size, font_size);

    flint_text_draw(text, center_x - text_w / 2, y, font_size, color);
}

int
flint_text_y(const char *text, int box_y, int box_h, int font_size)
{
    Font font = active_font();
    float scale;
    float min_top;
    float max_bottom;
    int seen_glyph;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0 ||
       font.glyphs == NULL || font.recs == NULL)
        return box_y + (box_h - font_size) / 2;

    scale = (float)font_size / (float)font.baseSize;
    min_top = 0.0f;
    max_bottom = 0.0f;
    seen_glyph = 0;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        if(codepoint != ' ' && codepoint != '\t') {
            int index = GetGlyphIndex(font, codepoint);
            GlyphInfo glyph = font.glyphs[index];
            Rectangle rec = font.recs[index];
            float glyph_top = (float)glyph.offsetY * scale - (float)font.glyphPadding * scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)font.glyphPadding) * scale;

            if(!seen_glyph) {
                min_top = glyph_top;
                max_bottom = glyph_bottom;
                seen_glyph = 1;
            } else {
                if(glyph_top < min_top)
                    min_top = glyph_top;
                if(glyph_bottom > max_bottom)
                    max_bottom = glyph_bottom;
            }
        }

        i += codepoint_byte_count;
    }

    if(!seen_glyph)
        return box_y + (box_h - font_size) / 2;

    return box_y + (int)(((float)box_h - (max_bottom - min_top)) * 0.5f - min_top + 0.5f);
}

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

int
flint_text_measure_scaled(const char *text, int scale)
{
    Font font = active_font();
    int width = 0;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.baseSize <= 0)
        return 0;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        int index = GetGlyphIndex(font, codepoint);
        width += font.glyphs[index].advanceX * scale;
        i += codepoint_byte_count;
    }

    return width;
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
flint_text_draw_scaled(const char *text, int x, int y, int scale, Color color)
{
    Font font = active_font();
    int cursor_x = x;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.recs == NULL)
        return;
    if(scale < 1)
        scale = 1;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        int index = GetGlyphIndex(font, codepoint);
        GlyphInfo glyph = font.glyphs[index];
        Rectangle src = font.recs[index];

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)(cursor_x + glyph.offsetX * scale),
                .y = (float)(y + glyph.offsetY * scale),
                .width = src.width * (float)scale,
                .height = src.height * (float)scale
            };
            DrawTexturePro(font.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += glyph.advanceX * scale;
        i += codepoint_byte_count;
    }
}

void
flint_text_draw_centered(const char *text, int center_x, int center_y, int font_size, Color color)
{
    int text_w = flint_text_measure(text, font_size);
    int y = flint_text_y(text, center_y - font_size / 2, font_size, font_size);

    flint_text_draw(text, center_x - text_w / 2, y, font_size, color);
}

int
flint_text_y_scaled(const char *text, int box_y, int box_h, int scale)
{
    Font font = active_font();
    int font_size;

    if(scale < 1)
        scale = 1;
    font_size = font.baseSize > 0 ? font.baseSize * scale : 16 * scale;
    return flint_text_y(text, box_y, box_h, font_size);
}

int
flint_text_y(const char *text, int box_y, int box_h, int font_size)
{
    Font font = active_font();
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0)
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    scale = (float)font_size / (float)font.baseSize;

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
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    return box_y + (int)(((float)box_h - (max_bottom - min_top)) * 0.5f - min_top + 0.5f);
}

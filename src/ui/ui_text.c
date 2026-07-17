#include "ui_text.h"
#include "ui_clip.h"
#include "ui_internal.h"
#include "embedded_assets.h"
#include "ui_scaling.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>

#define UI_FONT_MAX_REGISTERED 16
#define UI_FONT_DEFAULT_NAME "default"
#define UI_FONT_CACHE_COUNT 8

typedef struct UIFontEntry {
    char name[32];
    Font font;
    Font small_font;
    const char *file_type;
    const unsigned char *font_data;
    unsigned int font_data_size;
    int *codepoints;
    int codepoint_count;
    Font cache[UI_FONT_CACHE_COUNT];
    int cache_size[UI_FONT_CACHE_COUNT];
} UIFontEntry;

static UIFontEntry g_ui_fonts[UI_FONT_MAX_REGISTERED];
static int g_ui_font_count = 0;
static int g_ui_active_font = -1;

static Rectangle
text_world_rect_to_screen(Rectangle rect)
{
    return (Rectangle){
        g_ui_camera.offset.x + rect.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + rect.y * g_ui_camera.zoom,
        rect.width * g_ui_camera.zoom,
        rect.height * g_ui_camera.zoom
    };
}

static int
font_valid(Font font)
{
    return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL &&
           font.glyphCount > 0 && font.baseSize > 0;
}

int
UIFontHasGlyph(Font font, int codepoint)
{
    if(!font_valid(font))
        return 0;

    for(int i = 0; i < font.glyphCount; i++) {
        if(font.glyphs[i].value == codepoint)
            return 1;
    }

    return 0;
}

static int
font_entry_index(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : UI_FONT_DEFAULT_NAME;

    for(int i = 0; i < g_ui_font_count; i++) {
        if(strcmp(g_ui_fonts[i].name, key) == 0)
            return i;
    }

    return -1;
}

static int
font_entry_alloc(const char *name)
{
    const char *key = name != NULL && name[0] != '\0' ? name : UI_FONT_DEFAULT_NAME;
    int index = font_entry_index(key);

    if(index >= 0)
        return index;
    if(g_ui_font_count >= UI_FONT_MAX_REGISTERED)
        return -1;

    index = g_ui_font_count++;
    memset(&g_ui_fonts[index], 0, sizeof(g_ui_fonts[index]));
    snprintf(g_ui_fonts[index].name, sizeof(g_ui_fonts[index].name), "%s", key);
    if(g_ui_active_font < 0)
        g_ui_active_font = index;
    return index;
}

typedef struct UIChoppedGlyph {
    int32_t value;
    int32_t x;
    int32_t y;
    int32_t w;
    int32_t h;
    int32_t offsetX;
    int32_t offsetY;
    int32_t advanceX;
} UIChoppedGlyph;

static int
font_physical_size(int font_size)
{
    int size = ScaleUIPx(font_size);

    if(size <= 0)
        size = font_size > 0 ? font_size : UI_TEXT_BASE_SIZE;
    if(size <= 0)
        size = UI_TEXT_BASE_SIZE;
    return size;
}

static void
clear_font_cache(UIFontEntry *entry)
{
    if(entry == NULL)
        return;
    for(int i = 0; i < UI_FONT_CACHE_COUNT; i++) {
        if(font_valid(entry->cache[i]))
            UnloadFont(entry->cache[i]);
        entry->cache[i] = (Font){0};
        entry->cache_size[i] = 0;
    }
}

static Font
load_font_source_size(UIFontEntry *entry, int physical_size)
{
    Font font;

    if(entry == NULL || entry->font_data == NULL || entry->font_data_size == 0)
        return (Font){0};

    font = LoadFontFromMemory(
        entry->file_type != NULL && entry->file_type[0] != '\0' ? entry->file_type : ".ttf",
        entry->font_data, (int)entry->font_data_size, physical_size,
        entry->codepoints, entry->codepoint_count);
    if(font_valid(font))
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

static Font
entry_source_font_for_size(UIFontEntry *entry, int font_size)
{
    int physical_size = font_physical_size(font_size);
    int empty = -1;

    if(entry == NULL || entry->font_data == NULL || entry->font_data_size == 0)
        return (Font){0};

    for(int i = 0; i < UI_FONT_CACHE_COUNT; i++) {
        if(entry->cache_size[i] == physical_size && font_valid(entry->cache[i]))
            return entry->cache[i];
        if(empty < 0 && entry->cache_size[i] == 0)
            empty = i;
    }

    if(empty < 0) {
        empty = 0;
        if(font_valid(entry->cache[empty]))
            UnloadFont(entry->cache[empty]);
    }

    entry->cache[empty] = load_font_source_size(entry, physical_size);
    entry->cache_size[empty] = font_valid(entry->cache[empty]) ? physical_size : 0;
    return entry->cache[empty];
}

static Font
active_font(void)
{
    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count) {
        Font font;

        font = entry_source_font_for_size(&g_ui_fonts[g_ui_active_font], UI_TEXT_BASE_SIZE);
        if(font_valid(font))
            return font;
        if(font_valid(g_ui_fonts[g_ui_active_font].font))
            return g_ui_fonts[g_ui_active_font].font;
    }

    return GetFontDefault();
}

static Font
active_font_for_size(int font_size)
{
    if(g_ui_active_font >= 0 && g_ui_active_font < g_ui_font_count) {
        Font font;

        font = entry_source_font_for_size(&g_ui_fonts[g_ui_active_font], font_size);
        if(font_valid(font))
            return font;
        if(font_size == UI_TEXT_8 && font_valid(g_ui_fonts[g_ui_active_font].small_font))
            return g_ui_fonts[g_ui_active_font].small_font;
    }

    return active_font();
}

static Font
font_for_codepoint(int codepoint, int font_size)
{
    Font font = active_font_for_size(font_size);

    (void)codepoint;
    return font;
}

static Font
font_for_scaled_codepoint(int codepoint)
{
    Font font = active_font();

    (void)codepoint;
    return font;
}

static float
font_size_scale(Font font, int font_size)
{
    int target_size = font_physical_size(font_size);
    int base_size = font.baseSize > 0 ? font.baseSize : UI_TEXT_BASE_SIZE;

    return (float)target_size / (float)base_size;
}

Font
GetUIFont(void)
{
    return active_font();
}

int
RegisterUIFont(const char *name, Font font)
{
    int index;

    if(!font_valid(font))
        return 0;

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_cache(&g_ui_fonts[index]);
    g_ui_fonts[index].font = font;
    return 1;
}

int
RegisterUISmallFont(const char *name, Font font)
{
    int index;

    if(!font_valid(font))
        return 0;

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_cache(&g_ui_fonts[index]);
    g_ui_fonts[index].small_font = font;
    return 1;
}

int
RegisterUIFontSource(const char *name, const char *file_type,
                     const unsigned char *font_data, unsigned int font_size,
                     const int *codepoints, int codepoint_count)
{
    int index;

    if(font_data == NULL || font_size == 0)
        return 0;

    index = font_entry_alloc(name);
    if(index < 0)
        return 0;

    clear_font_cache(&g_ui_fonts[index]);
    free(g_ui_fonts[index].codepoints);
    g_ui_fonts[index].codepoints = NULL;
    g_ui_fonts[index].codepoint_count = 0;

    if(codepoints != NULL && codepoint_count > 0) {
        g_ui_fonts[index].codepoints = calloc((size_t)codepoint_count, sizeof(*codepoints));
        if(g_ui_fonts[index].codepoints == NULL)
            return 0;
        memcpy(g_ui_fonts[index].codepoints, codepoints,
               (size_t)codepoint_count * sizeof(*codepoints));
        g_ui_fonts[index].codepoint_count = codepoint_count;
    }

    g_ui_fonts[index].file_type = file_type;
    g_ui_fonts[index].font_data = font_data;
    g_ui_fonts[index].font_data_size = font_size;
    g_ui_fonts[index].font = (Font){0};
    g_ui_fonts[index].small_font = (Font){0};
    return font_valid(entry_source_font_for_size(&g_ui_fonts[index], UI_TEXT_BASE_SIZE));
}

int
UseUIFont(const char *name)
{
    int index = font_entry_index(name);

    if(index < 0)
        return 0;
    if(!font_valid(entry_source_font_for_size(&g_ui_fonts[index], UI_TEXT_BASE_SIZE)) &&
       !font_valid(g_ui_fonts[index].font))
        return 0;

    g_ui_active_font = index;
    return 1;
}

int
PushUIFont(const char *name)
{
    int token = g_ui_active_font;

    if(name != NULL && name[0] != '\0')
        (void)UseUIFont(name);
    return token;
}

void
PopUIFont(int token)
{
    if(token >= 0 && token < g_ui_font_count)
        g_ui_active_font = token;
}

Font
LoadUIChoppedFontFromMemory(const unsigned char *png_data, unsigned int png_size,
                                         const unsigned char *dat_data, unsigned int dat_size,
                                         int base_size)
{
    Font font = {0};
    UIChoppedGlyph *glyphs = NULL;
    GlyphInfo *glyph_infos = NULL;
    Rectangle *recs = NULL;
    int32_t glyph_count = 0;
    Image image = {0};
    Texture2D texture = {0};
    const unsigned char *cursor;
    size_t glyph_bytes;

    if(png_data == NULL || png_size == 0 || dat_data == NULL || dat_size < sizeof(glyph_count))
        return font;

    cursor = dat_data;
    memcpy(&glyph_count, cursor, sizeof(glyph_count));
    cursor += sizeof(glyph_count);
    if(glyph_count <= 0)
        return font;

    glyph_bytes = (size_t)glyph_count * sizeof(*glyphs);
    if((size_t)dat_size - sizeof(glyph_count) < glyph_bytes)
        return font;

    glyphs = (UIChoppedGlyph *)calloc((size_t)glyph_count, sizeof(*glyphs));
    glyph_infos = (GlyphInfo *)calloc((size_t)glyph_count, sizeof(*glyph_infos));
    recs = (Rectangle *)calloc((size_t)glyph_count, sizeof(*recs));
    if(glyphs == NULL || glyph_infos == NULL || recs == NULL)
        goto cleanup;

    memcpy(glyphs, cursor, glyph_bytes);

    image = LoadImageFromMemory(".png", png_data, (int)png_size);
    if(image.data == NULL)
        goto cleanup;

#if defined(_WIN32)
    {
        int pot_w = 1;
        int pot_h = 1;
        while(pot_w < image.width)
            pot_w <<= 1;
        while(pot_h < image.height)
            pot_h <<= 1;
        if(pot_w != image.width || pot_h != image.height)
            ImageResizeCanvas(&image, pot_w, pot_h, 0, 0, BLANK);
    }
#endif

    texture = LoadTextureFromImage(image);
    UnloadImage(image);
    image = (Image){0};
    if(texture.id == 0)
        goto cleanup;
    SetTextureFilter(texture, TEXTURE_FILTER_POINT);

    for(int i = 0; i < glyph_count; i++) {
        glyph_infos[i].value = glyphs[i].value;
        glyph_infos[i].offsetX = glyphs[i].offsetX;
        glyph_infos[i].offsetY = glyphs[i].offsetY;
        glyph_infos[i].advanceX = glyphs[i].advanceX;
        glyph_infos[i].image = (Image){0};
        recs[i] = (Rectangle){(float)glyphs[i].x, (float)glyphs[i].y,
                              (float)glyphs[i].w, (float)glyphs[i].h};
    }

    font.texture = texture;
    font.glyphs = glyph_infos;
    font.recs = recs;
    font.glyphCount = glyph_count;
    font.baseSize = base_size > 0 ? base_size : UI_TEXT_BASE_SIZE;
    font.glyphPadding = 0;

    free(glyphs);
    return font;

cleanup:
    if(image.data != NULL)
        UnloadImage(image);
    if(texture.id != 0)
        UnloadTexture(texture);
    free(glyphs);
    free(glyph_infos);
    free(recs);
    return (Font){0};
}

static int
append_codepoint_range(int *codepoints, int index, int first, int last)
{
    for(int codepoint = first; codepoint <= last; codepoint++)
        codepoints[index++] = codepoint;
    return index;
}

static int *
ui_font_codepoints(int *out_count)
{
    int count = 0;
    int *codepoints;

    count += 0x7E - 0x20 + 1;
    count += 0x024F - 0x00A0 + 1;
    count += 0x206F - 0x2000 + 1;
    count += 0x20CF - 0x20A0 + 1;
    count += 0x03FF - 0x0370 + 1;
    count += 0x04FF - 0x0400 + 1;

    codepoints = calloc((size_t)count, sizeof(*codepoints));
    if(codepoints == NULL) {
        *out_count = 0;
        return NULL;
    }

    count = 0;
    count = append_codepoint_range(codepoints, count, 0x20, 0x7E);
    count = append_codepoint_range(codepoints, count, 0x00A0, 0x024F);
    count = append_codepoint_range(codepoints, count, 0x2000, 0x206F);
    count = append_codepoint_range(codepoints, count, 0x20A0, 0x20CF);
    count = append_codepoint_range(codepoints, count, 0x0370, 0x03FF);
    count = append_codepoint_range(codepoints, count, 0x0400, 0x04FF);
    *out_count = count;
    return codepoints;
}

Font
LoadUIFontFromMemory(const char *file_type, const unsigned char *font_data,
                     unsigned int font_size, int base_size)
{
    Font font = {0};
    int codepoint_count = 0;
    int *codepoints;

    if(font_data == NULL || font_size == 0)
        return font;

    codepoints = ui_font_codepoints(&codepoint_count);
    if(codepoints == NULL || codepoint_count == 0)
        return font;

    font = LoadFontFromMemory(
        file_type != NULL && file_type[0] != '\0' ? file_type : ".ttf",
        font_data, (int)font_size,
        base_size > 0 ? base_size : UI_TEXT_BASE_SIZE,
        codepoints, codepoint_count);
    free(codepoints);

    if(font_valid(font))
        SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
    return font;
}

Font
LoadUIFontAsset(const char *path, int base_size)
{
    const EmbeddedAsset *asset;
    const char *file_type;
    unsigned char *data;
    int data_size = 0;
    Font font;

    if(path == NULL || path[0] == '\0')
        return (Font){0};

    file_type = GetEmbeddedAssetExtension(path);
    asset = GetEmbeddedAsset(path);
    if(asset != NULL)
        return LoadUIFontFromMemory(file_type, asset->data, asset->size, base_size);

    data = LoadFileData(path, &data_size);
    if(data == NULL || data_size <= 0)
        return (Font){0};

    font = LoadUIFontFromMemory(file_type, data, (unsigned int)data_size, base_size);
    UnloadFileData(data);
    return font;
}

void
UnloadUIFont(Font *font)
{
    if(font == NULL || font->texture.id == 0)
        return;

    UnloadFont(*font);
    *font = (Font){0};
}

void
ClearUIFonts(void)
{
    for(int i = 0; i < g_ui_font_count; i++) {
        clear_font_cache(&g_ui_fonts[i]);
        free(g_ui_fonts[i].codepoints);
    }
    memset(g_ui_fonts, 0, sizeof(g_ui_fonts));
    g_ui_font_count = 0;
    g_ui_active_font = -1;
}

int
MeasureUIText(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);
    int width = 0;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.baseSize <= 0)
        return 0;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        int index;

        if(codepoint == '\n')
            break;
        glyph_font = font_for_codepoint(codepoint, font_size);
        index = GetGlyphIndex(glyph_font, codepoint);
        width += (int)((float)glyph_font.glyphs[index].advanceX *
                       font_size_scale(glyph_font, font_size) + 0.5f);
        i += codepoint_byte_count;
    }
    return width;
}

int
GetUITextHeight(const char *text, int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale;
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0)
        return font_size;

    scale = font_size_scale(font, font_size);
    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;
        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, font_size);
            int index = GetGlyphIndex(glyph_font, codepoint);
            GlyphInfo glyph = glyph_font.glyphs[index];
            Rectangle rec = glyph_font.recs[index];
            float glyph_scale = font_size_scale(glyph_font, font_size);
            float glyph_top = (float)glyph.offsetY * glyph_scale - (float)glyph_font.glyphPadding * glyph_scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)glyph_font.glyphPadding) * glyph_scale;

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
        return (int)((float)font.baseSize * scale + 0.5f);
    return (int)(max_bottom - min_top + 0.5f);
}

int
GetUITextLineHeight(int font_size)
{
    Font font = active_font_for_size(font_size);
    float scale = font_size_scale(font, font_size);

    return font.baseSize > 0 ? (int)((float)font.baseSize * scale + 0.5f) :
        (int)((float)UI_TEXT_BASE_SIZE * scale + 0.5f);
}

int
MeasureScaledUIText(const char *text, int scale)
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

        Font glyph_font = font_for_scaled_codepoint(codepoint);
        int index = GetGlyphIndex(glyph_font, codepoint);
        width += glyph_font.glyphs[index].advanceX * scale;
        i += codepoint_byte_count;
    }

    return width;
}

void
DrawUIText(const char *text, int x, int y, int font_size, Color color)
{
    Font font = active_font_for_size(font_size);
    int cursor_x = x;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.recs == NULL)
        return;

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        int index;
        float scale;
        GlyphInfo glyph;
        Rectangle src;

        if(codepoint == '\n')
            break;

        glyph_font = font_for_codepoint(codepoint, font_size);
        index = GetGlyphIndex(glyph_font, codepoint);
        scale = font_size_scale(glyph_font, font_size);
        glyph = glyph_font.glyphs[index];
        src = glyph_font.recs[index];

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)cursor_x + (float)glyph.offsetX * scale,
                .y = (float)y + (float)glyph.offsetY * scale,
                .width = src.width * scale,
                .height = src.height * scale
            };
            DrawTexturePro(glyph_font.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += (int)((float)glyph.advanceX * scale + 0.5f);
        i += codepoint_byte_count;
    }
}

void
DrawScaledUIText(const char *text, int x, int y, int scale, Color color)
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

        Font glyph_font = font_for_scaled_codepoint(codepoint);
        int index = GetGlyphIndex(glyph_font, codepoint);
        GlyphInfo glyph = glyph_font.glyphs[index];
        Rectangle src = glyph_font.recs[index];

        if(src.width > 0.0f && src.height > 0.0f) {
            Rectangle dst = {
                .x = (float)(cursor_x + glyph.offsetX * scale),
                .y = (float)(y + glyph.offsetY * scale),
                .width = src.width * (float)scale,
                .height = src.height * (float)scale
            };
            DrawTexturePro(glyph_font.texture, src, dst, (Vector2){0.0f, 0.0f}, 0.0f, color);
        }

        cursor_x += glyph.advanceX * scale;
        i += codepoint_byte_count;
    }
}

void
DrawCenteredUIText(const char *text, int center_x, int center_y, int font_size, Color color)
{
    int text_w = MeasureUIText(text, font_size);
    int line_h = GetUITextLineHeight(font_size);
    int y = GetUITextY("Hg", center_y - line_h / 2, line_h, font_size);

    DrawUIText(text, center_x - text_w / 2, y, font_size, color);
}

void
DrawUITextInRect(const char *text, Rectangle rect, int font_size, Color color)
{
    const char *value = text != NULL ? text : "";
    int text_w = MeasureUIText(value, font_size);
    int x = (int)(rect.x + (rect.width - (float)text_w) * 0.5f);
    int y = GetUITextY(value, (int)rect.y, (int)rect.height, font_size);
    int clip_guard = 1;

    Rectangle clip = text_world_rect_to_screen((Rectangle){
        rect.x, rect.y - clip_guard, rect.width, rect.height + clip_guard * 2
    });
    BeginUIClip((int)clip.x, (int)clip.y, (int)clip.width, (int)clip.height);
    DrawUIText(value, x, y, font_size, color);
    EndUIClip();
}

int
GetScaledUITextY(const char *text, int box_y, int box_h, int scale)
{
    Font font = active_font();
    int font_size;

    if(scale < 1)
        scale = 1;
    font_size = font.baseSize > 0 ? font.baseSize * scale : 16 * scale;
    return GetUITextY(text, box_y, box_h, font_size);
}

int
GetUITextY(const char *text, int box_y, int box_h, int font_size)
{
    Font font = active_font_for_size(font_size);
    float min_top = 0.0f;
    float max_bottom = 0.0f;
    int seen_glyph = 0;

    if(text == NULL || text[0] == '\0' || font.texture.id == 0 || font.baseSize <= 0)
        return box_y + (int)(((float)box_h - (float)font_size) * 0.5f + 0.5f);

    for(int i = 0; text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);

        if(codepoint == '\n')
            break;

        if(codepoint != ' ' && codepoint != '\t') {
            Font glyph_font = font_for_codepoint(codepoint, font_size);
            int index = GetGlyphIndex(glyph_font, codepoint);
            GlyphInfo glyph = glyph_font.glyphs[index];
            Rectangle rec = glyph_font.recs[index];
            float glyph_scale = font_size_scale(glyph_font, font_size);
            float glyph_top = (float)glyph.offsetY * glyph_scale - (float)glyph_font.glyphPadding * glyph_scale;
            float glyph_bottom = glyph_top + ((float)rec.height + 2.0f * (float)glyph_font.glyphPadding) * glyph_scale;

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

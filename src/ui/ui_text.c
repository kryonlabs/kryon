#include "ui_text.h"
#include "ui_clip.h"
#include "ui_internal.h"
#include "embedded_assets.h"
#include "ui_scaling.h"

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
static int g_ui_text_selectable_stack[16];
static int g_ui_text_selectable_stack_count = 0;
static int g_ui_text_selectable = 1;

typedef struct UITextSelectionState {
    int id;
    int anchor;
    int cursor;
    int dragging;
} UITextSelectionState;

static UITextSelectionState g_ui_text_selection = {0};

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

static int
ui_text_hash_int(int hash, int value)
{
    unsigned int h = (unsigned int)hash;

    h ^= (unsigned int)value + 0x9e3779b9u + (h << 6) + (h >> 2);
    return (int)(h & 0x7fffffff);
}

static int
ui_text_id(const char *text, int x, int y, int font_size)
{
    uintptr_t ptr = (uintptr_t)text;
    int hash = 5381;

    hash = ui_text_hash_int(hash, (int)(ptr & 0xffffffffu));
    hash = ui_text_hash_int(hash, (int)(ptr >> 32));
    hash = ui_text_hash_int(hash, x);
    hash = ui_text_hash_int(hash, y);
    hash = ui_text_hash_int(hash, font_size);
    if(hash == 0)
        hash = 1;
    return hash;
}

static int
ui_text_line_byte_len(const char *text)
{
    int len = 0;

    if(text == NULL)
        return 0;
    while(text[len] != '\0' && text[len] != '\n')
        len++;
    return len;
}

static int
ui_text_width_bytes(const char *text, int byte_len, int font_size)
{
    Font font = active_font_for_size(font_size);
    int width = 0;

    if(text == NULL || byte_len <= 0 || font.texture.id == 0 ||
       font.glyphs == NULL || font.baseSize <= 0)
        return 0;

    for(int i = 0; i < byte_len && text[i] != '\0';) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        int index;

        if(codepoint == '\n')
            break;
        if(codepoint_byte_count <= 0)
            codepoint_byte_count = 1;
        if(i + codepoint_byte_count > byte_len)
            break;
        glyph_font = font_for_codepoint(codepoint, font_size);
        index = GetGlyphIndex(glyph_font, codepoint);
        width += (int)((float)glyph_font.glyphs[index].advanceX *
                       font_size_scale(glyph_font, font_size) + 0.5f);
        i += codepoint_byte_count;
    }

    return width;
}

static int
ui_text_byte_offset_at_x(const char *text, int font_size, int target_x)
{
    int byte_len = ui_text_line_byte_len(text);
    int cursor_x = 0;

    if(text == NULL || target_x <= 0)
        return 0;

    for(int i = 0; i < byte_len;) {
        int codepoint_byte_count = 0;
        int codepoint = GetCodepointNext(&text[i], &codepoint_byte_count);
        Font glyph_font;
        int index;
        int advance;

        if(codepoint_byte_count <= 0)
            codepoint_byte_count = 1;
        if(i + codepoint_byte_count > byte_len)
            return i;
        glyph_font = font_for_codepoint(codepoint, font_size);
        index = GetGlyphIndex(glyph_font, codepoint);
        advance = (int)((float)glyph_font.glyphs[index].advanceX *
                        font_size_scale(glyph_font, font_size) + 0.5f);
        if(target_x < cursor_x + advance / 2)
            return i;
        cursor_x += advance;
        i += codepoint_byte_count;
    }

    return byte_len;
}

static Color
ui_text_default_selection_color(Color text_color)
{
    Color color = c_link.a != 0 ? c_link : text_color;

    color.a = 88;
    return color;
}

static void
ui_text_copy_selection(const char *text, int start, int end)
{
    char *copy;
    int len;

    if(text == NULL || end <= start)
        return;

    len = end - start;
    copy = (char *)malloc((size_t)len + 1);
    if(copy == NULL)
        return;
    memcpy(copy, text + start, (size_t)len);
    copy[len] = '\0';
    SetClipboardText(copy);
    free(copy);
}

static void
ui_text_draw_selection(const char *text, int x, int y, int font_size,
                       Color color, int start, int end)
{
    int line_h;
    int start_x;
    int end_x;

    if(text == NULL || end <= start)
        return;

    line_h = GetUITextLineHeight(font_size);
    start_x = x + ui_text_width_bytes(text, start, font_size);
    end_x = x + ui_text_width_bytes(text, end, font_size);
    if(end_x <= start_x)
        return;

    DrawRectangle(start_x, y, end_x - start_x, line_h, color);
}

static int
ui_text_mod_key_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
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
DrawUITextEx(const char *text, int x, int y, int font_size, Color color,
             int selectable_arg)
{
    Font font = active_font_for_size(font_size);
    int cursor_x = x;
    int selectable;
    int id;
    int byte_len;
    int text_w;
    int line_h;
    int selected_start = 0;
    int selected_end = 0;

    if(text == NULL || font.texture.id == 0 || font.glyphs == NULL || font.recs == NULL)
        return;

    selectable = selectable_arg && g_ui_text_selectable && text[0] != '\0';
    byte_len = ui_text_line_byte_len(text);
    text_w = MeasureUIText(text, font_size);
    line_h = GetUITextLineHeight(font_size);
    id = selectable ? ui_text_id(text, x, y, font_size) : 0;

    if(selectable) {
        Rectangle bounds = {(float)x, (float)y, (float)text_w, (float)line_h};
        Vector2 mouse = ui_mouse_world();
        int inside = CheckCollisionPointRec(mouse, bounds);
        int captured = UIInputCapturesClick(mouse);

        if(inside && !captured)
            SetMouseCursor(MOUSE_CURSOR_IBEAM);

        if(inside && !captured && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            int offset = ui_text_byte_offset_at_x(text, font_size, (int)mouse.x - x);

            g_ui_text_selection.id = id;
            g_ui_text_selection.anchor = offset;
            g_ui_text_selection.cursor = offset;
            g_ui_text_selection.dragging = 1;
        }
        if(g_ui_text_selection.id == id && g_ui_text_selection.dragging) {
            if(IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
                g_ui_text_selection.cursor =
                    ui_text_byte_offset_at_x(text, font_size, (int)mouse.x - x);
            } else {
                g_ui_text_selection.dragging = 0;
            }
        }
        if(g_ui_text_selection.id == id && ui_text_mod_key_down() && IsKeyPressed(KEY_C)) {
            int start = g_ui_text_selection.anchor;
            int end = g_ui_text_selection.cursor;

            if(start > end) {
                int tmp = start;
                start = end;
                end = tmp;
            }
            start = ui_clampi(start, 0, byte_len);
            end = ui_clampi(end, 0, byte_len);
            ui_text_copy_selection(text, start, end);
        }
        if(g_ui_text_selection.id == id) {
            selected_start = g_ui_text_selection.anchor;
            selected_end = g_ui_text_selection.cursor;
            if(selected_start > selected_end) {
                int tmp = selected_start;
                selected_start = selected_end;
                selected_end = tmp;
            }
            selected_start = ui_clampi(selected_start, 0, byte_len);
            selected_end = ui_clampi(selected_end, 0, byte_len);
            ui_text_draw_selection(text, x, y, font_size,
                                   ui_text_default_selection_color(color),
                                   selected_start, selected_end);
        }
    }

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
DrawUIText(const char *text, int x, int y, int font_size, Color color)
{
    DrawUITextEx(text, x, y, font_size, color, 1);
}

void
DrawUINonSelectableText(const char *text, int x, int y, int font_size, Color color)
{
    DrawUITextEx(text, x, y, font_size, color, 0);
}

int
PushUITextSelectable(int selectable)
{
    int token = g_ui_text_selectable;

    if(g_ui_text_selectable_stack_count <
       (int)(sizeof(g_ui_text_selectable_stack) / sizeof(g_ui_text_selectable_stack[0])))
        g_ui_text_selectable_stack[g_ui_text_selectable_stack_count++] = token;
    g_ui_text_selectable = selectable != 0;
    return token;
}

void
PopUITextSelectable(int token)
{
    if(g_ui_text_selectable_stack_count > 0)
        g_ui_text_selectable = g_ui_text_selectable_stack[--g_ui_text_selectable_stack_count];
    else
        g_ui_text_selectable = token != 0;
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

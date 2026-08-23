#include "libdraw_internal.h"

#ifndef KRYON_NATIVE_PLAN9
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#endif

#define LIBDRAW_FONT_ATLAS_W 1024
#define LIBDRAW_FONT_ATLAS_START_H 128
#define LIBDRAW_FONT_ATLAS_MAX_H 8192

static Font g_default_font;
static int g_default_ready;

static Image
zero_image(void)
{
    Image image;

    memset(&image, 0, sizeof(image));
    return image;
}

static Rectangle
zero_rectangle(void)
{
    Rectangle rec;

    memset(&rec, 0, sizeof(rec));
    return rec;
}

static GlyphInfo
zero_glyph_info(void)
{
    GlyphInfo glyph;

    memset(&glyph, 0, sizeof(glyph));
    return glyph;
}

static Font
zero_font(void)
{
    Font font;

    memset(&font, 0, sizeof(font));
    return font;
}

static unsigned char *
read_font_file(const char *path, int *len)
{
#ifdef KRYON_NATIVE_PLAN9
    (void)path;
    if(len != NULL)
        *len = 0;
    return NULL;
#else
    FILE *f;
    long n;
    unsigned char *data;

    if(len != NULL)
        *len = 0;
    if(path == NULL)
        return NULL;
    f = fopen(path, "rb");
    if(f == NULL)
        return NULL;
    if(fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return NULL;
    }
    n = ftell(f);
    if(n <= 0) {
        fclose(f);
        return NULL;
    }
    rewind(f);
    data = malloc((size_t)n);
    if(data == NULL) {
        fclose(f);
        return NULL;
    }
    if(fread(data, 1, (size_t)n, f) != (size_t)n) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if(len != NULL)
        *len = (int)n;
    return data;
#endif
}

static int
font_has_codepoint(const int *codepoints, int count, int codepoint)
{
    int i;

    if(codepoints == NULL || count <= 0)
        return 0;
    for(i = 0; i < count; i++)
        if(codepoints[i] == codepoint)
            return 1;
    return 0;
}

static int *
font_codepoint_set(const int *codepoints, int codepointCount, int *out_count)
{
    int *set;
    int count;
    int i;
    int write = 0;

    if(out_count != NULL)
        *out_count = 0;
    if(codepoints == NULL || codepointCount <= 0) {
        count = 224;
        set = malloc((size_t)count * sizeof(*set));
        if(set == NULL)
            return NULL;
        for(i = 0; i < count; i++)
            set[i] = 32 + i;
        if(out_count != NULL)
            *out_count = count;
        return set;
    }

    count = codepointCount + (font_has_codepoint(codepoints, codepointCount,
                                                 '?') ? 0 : 1);
    set = malloc((size_t)count * sizeof(*set));
    if(set == NULL)
        return NULL;
    if(!font_has_codepoint(codepoints, codepointCount, '?'))
        set[write++] = '?';
    for(i = 0; i < codepointCount; i++)
        set[write++] = codepoints[i];
    if(out_count != NULL)
        *out_count = write;
    return set;
}

static int
font_atlas_grow(unsigned char **pixels, int *atlas_h)
{
    unsigned char *grown;
    size_t old_size;
    size_t new_size;
    int new_h;

    if(pixels == NULL || *pixels == NULL || atlas_h == NULL ||
       *atlas_h >= LIBDRAW_FONT_ATLAS_MAX_H)
        return 0;
    new_h = *atlas_h * 2;
    if(new_h > LIBDRAW_FONT_ATLAS_MAX_H)
        new_h = LIBDRAW_FONT_ATLAS_MAX_H;
    old_size = (size_t)LIBDRAW_FONT_ATLAS_W * *atlas_h * 4;
    new_size = (size_t)LIBDRAW_FONT_ATLAS_W * new_h * 4;
    grown = realloc(*pixels, new_size);
    if(grown == NULL)
        return 0;
    memset(grown + old_size, 0, new_size - old_size);
    *pixels = grown;
    *atlas_h = new_h;
    return 1;
}

static Font
font_from_rgba(unsigned char *pixels, int atlas_w, int atlas_h,
               GlyphInfo *glyphs, Rectangle *recs, int glyph_count,
               int base_size)
{
    Image image = {0};
    Texture2D tex;
    Font font = {0};

    image.data = pixels;
    image.width = atlas_w;
    image.height = atlas_h;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    tex = LoadTextureFromImage(image);
    if(tex.id == 0)
        return font;
    font.baseSize = base_size;
    font.glyphCount = glyph_count;
    font.glyphPadding = 0;
    font.texture = tex;
    font.recs = recs;
    font.glyphs = glyphs;
    return font;
}

static Font
make_bitmap_font(void)
{
    enum { cell_w = 4, cell_h = 8, cols = 32, glyph_count = 95 };
    GlyphInfo *glyphs;
    Rectangle *recs;
    unsigned char *pixels;
    int rows = (glyph_count + cols - 1) / cols;
    int i;
    Font font;

    if(g_default_ready)
        return g_default_font;

    glyphs = calloc(glyph_count, sizeof(*glyphs));
    recs = calloc(glyph_count, sizeof(*recs));
    pixels = calloc((size_t)cols * cell_w * rows * cell_h * 4, 1);
    if(glyphs == NULL || recs == NULL || pixels == NULL) {
        free(glyphs);
        free(recs);
        free(pixels);
        return zero_font();
    }

    for(i = 0; i < glyph_count; i++) {
        int cp = 32 + i;
        int col = i % cols;
        int row = i / cols;
        int gy;

        glyphs[i].value = cp;
        glyphs[i].advanceX = cell_w;
        glyphs[i].image = zero_image();
        recs[i] = (Rectangle){(float)(col * cell_w), (float)(row * cell_h),
                              cell_w, cell_h};
        for(gy = 0; gy < cell_h; gy++) {
            unsigned char bits = KrySwFont8x8[cp][gy];
            int gx;

            for(gx = 0; gx < cell_w; gx++) {
                unsigned char *dst;

                if(((bits >> (gx * 8 / cell_w)) & 1) == 0)
                    continue;
                dst = pixels + ((size_t)(row * cell_h + gy) *
                                    (cols * cell_w) +
                                col * cell_w + gx) * 4;
                dst[0] = 255;
                dst[1] = 255;
                dst[2] = 255;
                dst[3] = 255;
            }
        }
    }

    font = font_from_rgba(pixels, cols * cell_w, rows * cell_h, glyphs, recs,
                          glyph_count, cell_h);
    free(pixels);
    if(font.texture.id == 0) {
        free(glyphs);
        free(recs);
        return zero_font();
    }
    g_default_font = font;
    g_default_ready = 1;
    return g_default_font;
}

static Font
make_ttf_font(const unsigned char *fileData, int dataSize, int fontSize,
              const int *codepoints, int codepointCount)
{
#ifdef KRYON_NATIVE_PLAN9
    (void)fileData;
    (void)dataSize;
    (void)fontSize;
    (void)codepoints;
    (void)codepointCount;
    return make_bitmap_font();
#else
    stbtt_fontinfo info;
    float scale;
    int ascent = 0;
    int descent = 0;
    int line_gap = 0;
    int *set = NULL;
    int set_count = 0;
    GlyphInfo *glyphs = NULL;
    Rectangle *recs = NULL;
    unsigned char *pixels = NULL;
    int atlas_h = LIBDRAW_FONT_ATLAS_START_H;
    int x = 1;
    int y = 1;
    int row_h = 0;
    int glyph_count = 0;
    int offset;
    int i;
    Font font = {0};

    if(fileData == NULL || dataSize <= 0 || fontSize <= 0)
        return font;

    offset = stbtt_GetFontOffsetForIndex(fileData, 0);
    if(offset < 0 || !stbtt_InitFont(&info, fileData, offset))
        return font;

    set = font_codepoint_set(codepoints, codepointCount, &set_count);
    if(set == NULL || set_count <= 0)
        goto fail;
    glyphs = calloc((size_t)set_count, sizeof(*glyphs));
    recs = calloc((size_t)set_count, sizeof(*recs));
    pixels = calloc((size_t)LIBDRAW_FONT_ATLAS_W * atlas_h * 4, 1);
    if(glyphs == NULL || recs == NULL || pixels == NULL)
        goto fail;

    scale = stbtt_ScaleForPixelHeight(&info, (float)fontSize);
    stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);
    (void)descent;
    (void)line_gap;

    for(i = 0; i < set_count; i++) {
        int cp = set[i];
        int glyph_index;
        int gw = 0;
        int gh = 0;
        int offx = 0;
        int offy = 0;
        int advance = 0;
        int lsb = 0;
        unsigned char *bitmap;

        if(cp <= 0)
            continue;
        glyph_index = stbtt_FindGlyphIndex(&info, cp);
        if(glyph_index == 0 && cp != '?')
            continue;
        bitmap = stbtt_GetCodepointBitmap(&info, scale, scale, cp, &gw, &gh,
                                          &offx, &offy);
        stbtt_GetCodepointHMetrics(&info, cp, &advance, &lsb);
        (void)lsb;

        if(gw > 0 && gh > 0) {
            if(x + gw + 1 > LIBDRAW_FONT_ATLAS_W) {
                x = 1;
                y += row_h + 1;
                row_h = 0;
            }
            while(y + gh + 1 > atlas_h && font_atlas_grow(&pixels, &atlas_h)) {
            }
            if(y + gh + 1 > atlas_h) {
                stbtt_FreeBitmap(bitmap, NULL);
                continue;
            }
        }

        glyphs[glyph_count].value = cp;
        glyphs[glyph_count].offsetX = offx;
        glyphs[glyph_count].offsetY =
            offy + (int)((float)ascent * scale);
        glyphs[glyph_count].advanceX =
            (int)((float)advance * scale + 0.5f);
        glyphs[glyph_count].image = zero_image();
        recs[glyph_count] = (Rectangle){(float)x, (float)y, (float)gw,
                                        (float)gh};
        if(gw > 0 && gh > 0 && bitmap != NULL) {
            int gy;

            for(gy = 0; gy < gh; gy++) {
                int gx;

                for(gx = 0; gx < gw; gx++) {
                    unsigned char alpha =
                        bitmap[(size_t)gy * gw + gx];
                    unsigned char *dst =
                        pixels + ((size_t)(y + gy) *
                                      LIBDRAW_FONT_ATLAS_W +
                                  x + gx) * 4;

                    dst[0] = 255;
                    dst[1] = 255;
                    dst[2] = 255;
                    dst[3] = alpha;
                }
            }
            x += gw + 1;
            if(gh > row_h)
                row_h = gh;
        }
        stbtt_FreeBitmap(bitmap, NULL);
        glyph_count++;
    }

    if(glyph_count <= 0)
        goto fail;
    font = font_from_rgba(pixels, LIBDRAW_FONT_ATLAS_W, atlas_h, glyphs, recs,
                          glyph_count, fontSize);
    if(font.texture.id == 0)
        goto fail;
    free(set);
    free(pixels);
    return font;

fail:
    free(set);
    free(glyphs);
    free(recs);
    free(pixels);
    return zero_font();
#endif
}

Font
GetFontDefault(void)
{
    return make_bitmap_font();
}

Font
LoadFontFromMemory(const char *fileType, const unsigned char *fileData,
                   int dataSize, int fontSize, const int *codepoints,
                   int codepointCount)
{
    (void)fileType;
    return make_ttf_font(fileData, dataSize, fontSize > 0 ? fontSize : 16,
                         codepoints, codepointCount);
}

Font
LoadFontEx(const char *fileName, int fontSize, const int *codepoints,
           int codepointCount)
{
    int len = 0;
    unsigned char *data = read_font_file(fileName, &len);
    Font font = LoadFontFromMemory(".ttf", data, len, fontSize, codepoints,
                                   codepointCount);

    free(data);
    return font;
}

Font
LoadFont(const char *fileName)
{
    return LoadFontEx(fileName, 16, NULL, 0);
}

bool
IsFontValid(Font font)
{
    return font.texture.id != 0 && font.glyphs != NULL && font.recs != NULL &&
           font.glyphCount > 0 && font.baseSize > 0;
}

void
UnloadFont(Font font)
{
    Font def = GetFontDefault();

    if(font.texture.id != 0 && font.texture.id != def.texture.id)
        UnloadTexture(font.texture);
    if(font.texture.id != def.texture.id) {
        free(font.glyphs);
        free(font.recs);
    }
}

int
GetGlyphIndex(Font font, int codepoint)
{
    int fallback = 0;
    int i;

    if(font.glyphs == NULL || font.glyphCount <= 0)
        return 0;
    for(i = 0; i < font.glyphCount; i++) {
        if(font.glyphs[i].value == '?')
            fallback = i;
        if(font.glyphs[i].value == codepoint)
            return i;
    }
    return fallback;
}

GlyphInfo
GetGlyphInfo(Font font, int codepoint)
{
    if(font.glyphs == NULL || font.glyphCount <= 0)
        return zero_glyph_info();
    return font.glyphs[GetGlyphIndex(font, codepoint)];
}

Rectangle
GetGlyphAtlasRec(Font font, int codepoint)
{
    if(font.recs == NULL || font.glyphCount <= 0)
        return zero_rectangle();
    return font.recs[GetGlyphIndex(font, codepoint)];
}

void
DrawTextCodepoint(Font font, int codepoint, Vector2 position, float fontSize,
                  Color tint)
{
    GlyphInfo glyph;
    Rectangle src;
    float scale;
    Rectangle dst;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font))
        return;
    glyph = GetGlyphInfo(font, codepoint);
    src = GetGlyphAtlasRec(font, codepoint);
    if(src.width <= 0.0f || src.height <= 0.0f)
        return;
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    dst = (Rectangle){position.x + (float)glyph.offsetX * scale,
                      position.y + (float)glyph.offsetY * scale,
                      src.width * scale, src.height * scale};
    DrawTexturePro(font.texture, src, dst, (Vector2){0, 0}, 0.0f, tint);
}

void
DrawTextCodepoints(Font font, const int *codepoints, int count,
                   Vector2 position, float fontSize, float spacing,
                   Color tint)
{
    int i;
    Vector2 p = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || codepoints == NULL || count <= 0)
        return;
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; i < count; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);

        DrawTextCodepoint(font, codepoints[i], p, fontSize, tint);
        p.x += (float)glyph.advanceX * scale + spacing;
    }
}

void
DrawTextEx(Font font, const char *text, Vector2 position, float fontSize,
           float spacing, Color tint)
{
    const char *p = text;
    Vector2 pen = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return;
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    while(*p != '\0') {
        int bytes = 0;
        int cp = GetCodepointNext(p, &bytes);
        GlyphInfo glyph;

        if(bytes <= 0)
            bytes = 1;
        if(cp == '\n') {
            pen.x = position.x;
            pen.y += fontSize;
            p += bytes;
            continue;
        }
        glyph = GetGlyphInfo(font, cp);
        DrawTextCodepoint(font, cp, pen, fontSize, tint);
        pen.x += (float)glyph.advanceX * scale + spacing;
        p += bytes;
    }
}

void
DrawText(const char *text, int posX, int posY, int fontSize, Color color)
{
    DrawTextEx(GetFontDefault(), text, (Vector2){(float)posX, (float)posY},
               (float)fontSize, 0.0f, color);
}

Vector2
MeasureTextEx(Font font, const char *text, float fontSize, float spacing)
{
    const char *p = text;
    float scale;
    float x = 0.0f;
    float max_x = 0.0f;
    float lines = 1.0f;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return (Vector2){0, fontSize};
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    while(*p != '\0') {
        int bytes = 0;
        int cp = GetCodepointNext(p, &bytes);
        GlyphInfo glyph;

        if(bytes <= 0)
            bytes = 1;
        if(cp == '\n') {
            if(x > max_x)
                max_x = x;
            x = 0.0f;
            lines += 1.0f;
            p += bytes;
            continue;
        }
        glyph = GetGlyphInfo(font, cp);
        x += (float)glyph.advanceX * scale + spacing;
        p += bytes;
    }
    if(x > max_x)
        max_x = x;
    return (Vector2){max_x, fontSize * lines};
}

int
MeasureText(const char *text, int fontSize)
{
    Vector2 size = MeasureTextEx(GetFontDefault(), text, (float)fontSize, 0.0f);

    return (int)(size.x + 0.5f);
}

Vector2
MeasureTextCodepoints(Font font, const int *codepoints, int length,
                      float fontSize, float spacing)
{
    float width = 0.0f;
    float scale;
    int i;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || codepoints == NULL || length <= 0)
        return (Vector2){0, fontSize};
    scale = font.baseSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; i < length; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);

        width += (float)glyph.advanceX * scale + spacing;
    }
    return (Vector2){width, fontSize};
}

GlyphInfo *
LoadFontData(const unsigned char *fileData, int dataSize, int fontSize,
             const int *codepoints, int codepointCount, int type,
             int *glyphCount)
{
    Font font;
    GlyphInfo *copy;

    (void)type;
    if(glyphCount != NULL)
        *glyphCount = 0;
    font = LoadFontFromMemory(".ttf", fileData, dataSize, fontSize,
                              codepoints, codepointCount);
    if(!IsFontValid(font))
        return NULL;
    copy = malloc((size_t)font.glyphCount * sizeof(*copy));
    if(copy != NULL) {
        memcpy(copy, font.glyphs, (size_t)font.glyphCount * sizeof(*copy));
        if(glyphCount != NULL)
            *glyphCount = font.glyphCount;
    }
    UnloadFont(font);
    return copy;
}

void
UnloadFontData(GlyphInfo *glyphs, int glyphCount)
{
    (void)glyphCount;
    free(glyphs);
}

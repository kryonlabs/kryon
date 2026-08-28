/*
 * canvas_text.c — fonts and text rendering.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * Text renders as glyph atlases rasterized from FontFace data
 * (EM_ASYNC_JS awaits face loading; the build uses -sASYNCIFY).
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

/* ------------------------------------------------------------------ */
/* Text: FontFace loading (async via Asyncify) + glyph rasterization  */
/* ------------------------------------------------------------------ */

EM_ASYNC_JS(int, js_font_face_load, (int ptr, int len), {
    var g = globalThis;
    if (typeof FontFace === 'undefined') return 0;
    try {
        var buf = new Uint8Array(HEAPU8.subarray(ptr, ptr + len));
        var face = new FontFace('kry-face-' + (g.__kryFaceCount || 0), buf);
        await face.load();
        if (g.document && g.document.fonts) g.document.fonts.add(face);
        g.__kryFaceCount = (g.__kryFaceCount || 0) + 1;
        return g.__kryFaceCount;      /* 1-based face id */
    } catch (e) {
        return 0;
    }
});

EM_JS(int, js_glyph_metrics, (int face_id, int cp, int size,
                              int *adv, int *w, int *h,
                              int *offx, int *offy, int ptr),
{
    var g = globalThis;
    if (!g.__kryGlyphCv) {
        if (typeof document !== 'undefined')
            g.__kryGlyphCv = document.createElement('canvas');
        else if (g.OffscreenCanvas)
            g.__kryGlyphCv = new OffscreenCanvas(64, 64);
        else return 0;
    }
    var cv = g.__kryGlyphCv;
    var ctx = cv.getContext('2d', {willReadFrequently: true});
    var spec = size + 'px ' + (face_id > 0
        ? 'kry-face-' + (face_id - 1) : 'monospace');
    ctx.font = spec;
    ctx.textBaseline = 'alphabetic';
    var ch = String.fromCodePoint(cp);
    var m = ctx.measureText(ch);
    var advX = Math.ceil(m.width);
    var glyphAsc = Math.ceil(m.actualBoundingBoxAscent !== undefined
                             ? m.actualBoundingBoxAscent : size * 0.8);
    var glyphDesc = Math.ceil(m.actualBoundingBoxDescent !== undefined
                              ? m.actualBoundingBoxDescent : 2);
    var fontAsc = Math.ceil(m.fontBoundingBoxAscent !== undefined
                            ? m.fontBoundingBoxAscent : size * 0.8);
    var fontDesc = Math.ceil(m.fontBoundingBoxDescent !== undefined
                             ? m.fontBoundingBoxDescent : Math.max(2, size * 0.2));
    var baseline = Math.max(fontAsc, glyphAsc) + 1;
    var left = Math.ceil(m.actualBoundingBoxLeft !== undefined
                         ? m.actualBoundingBoxLeft : 0);
    var right = Math.ceil(m.actualBoundingBoxRight !== undefined
                          ? m.actualBoundingBoxRight : m.width);
    var gw = Math.max(right + left + 2, 1);
    var gh = Math.max(baseline + Math.max(fontDesc, glyphDesc) + 1, 1);
    if (gw > 256 || gh > 256) return 0;
    if (cv.width < gw || cv.height < gh) {
        cv.width = Math.max(cv.width, gw);
        cv.height = Math.max(cv.height, gh);
        ctx = cv.getContext('2d', {willReadFrequently: true});
        ctx.font = spec;
        ctx.textBaseline = 'alphabetic';
    }
    ctx.clearRect(0, 0, gw, gh);
    ctx.fillStyle = '#fff';
    ctx.fillText(ch, left + 1, baseline);
    var d;
    try {
        d = ctx.getImageData(0, 0, gw, gh).data;
    } catch (e) {
        return 0;
    }
    HEAPU8.set(d.subarray(0, gw * gh * 4), ptr);
    setValue(adv, advX, 'i32');
    setValue(w, gw, 'i32');
    setValue(h, gh, 'i32');
    setValue(offx, -(left + 1), 'i32');
    setValue(offy, 0, 'i32');
    return 1;
});

/* ------------------------------------------------------------------ */
/* Text: atlas building                                               */
/* ------------------------------------------------------------------ */

#define CANVAS_ATLAS_START_W 512
#define CANVAS_ATLAS_START_H 64

static int g_default_font_built;
static Font g_default_font;

static int canvas_font_build(Font *out, int face_id, int baseSize,
                             const int *codepoints, int count)
{
    unsigned char *pixels;
    int atlas_w = CANVAS_ATLAS_START_W;
    int atlas_h = CANVAS_ATLAS_START_H;
    int x = 1;
    int y = 1;
    int row_h = 0;
    int i;
    int ok = 0;

    if(out == NULL || codepoints == NULL || count <= 0 || baseSize <= 0)
        return 0;
    memset(out, 0, sizeof(*out));
    out->glyphs = calloc((size_t)count, sizeof(GlyphInfo));
    out->recs = calloc((size_t)count, sizeof(Rectangle));
    pixels = malloc((size_t)atlas_w * atlas_h * 4);
    if(out->glyphs == NULL || out->recs == NULL || pixels == NULL)
        goto fail;
    memset(pixels, 0, (size_t)atlas_w * atlas_h * 4);
    for(i = 0; i < count; i++) {
        int adv = 0, gw = 0, gh = 0, offx = 0, offy = 0;
        unsigned char gbuf[256 * 256 * 4];

        if(codepoints[i] < 32)
            continue;
        if(js_glyph_metrics(face_id, codepoints[i], baseSize,
                            &adv, &gw, &gh, &offx, &offy,
                            (int)(size_t)gbuf) == 0)
            continue;
        if(x + gw + 1 > atlas_w) {
            x = 1;
            y += row_h + 1;
            row_h = 0;
        }
        while(y + gh + 1 > atlas_h && atlas_h < 4096)
            atlas_h *= 2;
        if(y + gh + 1 > atlas_h)
            continue;
        {
            unsigned char *grown = realloc(pixels,
                                           (size_t)atlas_w * atlas_h * 4);

            if(grown == NULL)
                continue;
            pixels = grown;
        }
        {
            int gy;

            for(gy = 0; gy < gh; gy++) {
                memcpy(pixels + (size_t)(y + gy) * atlas_w * 4 +
                       (size_t)x * 4,
                       gbuf + (size_t)gy * gw * 4, (size_t)gw * 4);
            }
        }
        out->recs[i] = (Rectangle){(float)x, (float)y, (float)gw, (float)gh};
        out->glyphs[i].value = codepoints[i];
        out->glyphs[i].offsetX = offx;
        out->glyphs[i].offsetY = offy;
        out->glyphs[i].advanceX = adv;
        x += gw + 1;
        if(gh > row_h)
            row_h = gh;
    }
    out->baseSize = baseSize;
    out->glyphCount = count;
    out->glyphPadding = 0;
    out->texture.id = (unsigned)js_texture_from_rgba((int)(size_t)pixels,
                                                     atlas_w, atlas_h);
    out->texture.width = atlas_w;
    out->texture.height = atlas_h;
    out->texture.mipmaps = 1;
    out->texture.format = 1;
    ok = out->texture.id != 0;
fail:
    free(pixels);
    if(!ok) {
        free(out->glyphs);
        free(out->recs);
        memset(out, 0, sizeof(*out));
    }
    return ok;
}

Font GetFontDefault(void)
{
    if(!g_default_font_built) {
        static int cps[96];
        int i;

        for(i = 0; i < 95; i++)
            cps[i] = 32 + i;
        if(canvas_font_build(&g_default_font, 0, 16, cps, 95))
            g_default_font_built = 1;
    }
    return g_default_font;
}

Font LoadFontFromMemory(const char *fileType, const unsigned char *fileData,
                        int dataSize, int fontSize, const int *codepoints,
                        int glyphCount)
{
    Font font;
    int face_id;

    (void)fileType;
    memset(&font, 0, sizeof(font));
    if(fileData == NULL || dataSize <= 0 || fontSize <= 0)
        return font;
    face_id = js_font_face_load((int)(size_t)fileData, dataSize);
    if(face_id == 0)
        return font;
    if(codepoints == NULL || glyphCount <= 0) {
        static int default_set[224];
        int i;

        for(i = 0; i < 224; i++)
            default_set[i] = 32 + i;
        glyphCount = 224;
        codepoints = default_set;
    }
    if(!canvas_font_build(&font, face_id, fontSize, codepoints, glyphCount))
        memset(&font, 0, sizeof(font));
    return font;
}

Font LoadFont(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font;

    memset(&font, 0, sizeof(font));
    if(data == NULL)
        return font;
    font = LoadFontFromMemory(".ttf", data, len, 16, NULL, 0);
    free(data);
    return font;
}

Font LoadFontEx(const char *fileName, int fontSize, const int *codepoints,
                int codepointCount)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font;

    memset(&font, 0, sizeof(font));
    if(data == NULL)
        return font;
    font = LoadFontFromMemory(".ttf", data, len, fontSize, codepoints,
                              codepointCount);
    free(data);
    return font;
}

void UnloadFont(Font font)
{
    if(font.glyphs != NULL)
        free(font.glyphs);
    if(font.recs != NULL)
        free(font.recs);
    if(font.texture.id != 0)
        js_texture_free((int)font.texture.id);
}

bool IsFontValid(Font font)
{
    return font.baseSize > 0 && font.glyphCount > 0 &&
           font.glyphs != NULL && font.recs != NULL &&
           font.texture.id != 0;
}

static int canvas_glyph_index(const Font *font, int codepoint)
{
    int i;
    int question = -1;

    if(font == NULL || font->glyphs == NULL)
        return 0;
    for(i = 0; i < font->glyphCount; i++) {
        if(font->glyphs[i].value == codepoint)
            return i;
        if(font->glyphs[i].value == '?')
            question = i;
    }
    return question >= 0 ? question : 0;
}

int GetGlyphIndex(Font font, int codepoint)
{
    return canvas_glyph_index(&font, codepoint);
}

GlyphInfo GetGlyphInfo(Font font, int codepoint)
{
    int i = canvas_glyph_index(&font, codepoint);

    if(!IsFontValid(font))
        return (GlyphInfo){0};
    return font.glyphs[i];
}

Rectangle GetGlyphAtlasRec(Font font, int codepoint)
{
    int i = canvas_glyph_index(&font, codepoint);

    if(!IsFontValid(font))
        return (Rectangle){0};
    return font.recs[i];
}

GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize,
                        int fontSize, const int *codepoints,
                        int codepointCount, int type, int *glyphCount)
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

void UnloadFontData(GlyphInfo *glyphs, int glyphCount)
{
    (void)glyphCount;
    free(glyphs);
}

void DrawTextCodepoint(Font font, int codepoint, Vector2 position,
                       float fontSize, Color tint)
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

void DrawTextCodepoints(Font font, const int *codepoints, int codepointCount,
                        Vector2 position, float fontSize, float spacing,
                        Color tint)
{
    int i;
    Vector2 pen = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || codepoints == NULL || codepointCount <= 0)
        return;
    scale = fontSize > 0 ? (float)fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; i < codepointCount; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);

        DrawTextCodepoint(font, codepoints[i], pen, fontSize, tint);
        pen.x += (float)glyph.advanceX * scale + spacing;
    }
}

void DrawTextEx(Font font, const char *text, Vector2 position, float fontSize,
                float spacing, Color tint)
{
    const char *p = text;
    Vector2 pen = position;
    float scale;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || text == NULL)
        return;
    scale = fontSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
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

EM_JS(void, js_text_transform, (int begin, double x, double y, double ox,
                                double oy, double rotation), {
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    if (!ctx) return;
    if (begin) {
        ctx.save();
        ctx.translate(x, y);
        if (rotation !== 0.0) ctx.rotate(rotation * Math.PI / 180.0);
        ctx.translate(-ox, -oy);
    } else {
        ctx.restore();
    }
});

void DrawTextPro(Font font, const char *text, Vector2 position,
                 Vector2 origin, float rotation, float fontSize,
                 float spacing, Color tint)
{
    js_text_transform(1, position.x, position.y, origin.x, origin.y,
                      rotation);
    DrawTextEx(font, text, (Vector2){0, 0}, fontSize, spacing, tint);
    js_text_transform(0, 0, 0, 0, 0, 0);
}

void DrawText(const char *text, int posX, int posY, int fontSize,
              Color color)
{
    DrawTextEx(GetFontDefault(), text, (Vector2){(float)posX, (float)posY},
               (float)fontSize, 0.0f, color);
}

Vector2 MeasureTextEx(Font font, const char *text, float fontSize,
                      float spacing)
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
    scale = fontSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
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
        } else {
            glyph = GetGlyphInfo(font, cp);
            x += (float)glyph.advanceX * scale + spacing;
            p += bytes;
        }
    }
    if(x > max_x)
        max_x = x;
    return (Vector2){max_x, fontSize * lines};
}

int MeasureText(const char *text, int fontSize)
{
    Vector2 size = MeasureTextEx(GetFontDefault(), text, (float)fontSize,
                                 0.0f);

    return (int)(size.x + 0.5f);
}

Vector2 MeasureTextCodepoints(Font font, const int *codepoints, int length,
                              float fontSize, float spacing)
{
    float width = 0.0f;
    float scale;
    int i;

    if(!IsFontValid(font))
        font = GetFontDefault();
    if(!IsFontValid(font) || codepoints == NULL || length <= 0)
        return (Vector2){0, fontSize};
    scale = fontSize > 0 ? fontSize / (float)font.baseSize : 1.0f;
    for(i = 0; i < length; i++) {
        GlyphInfo glyph = GetGlyphInfo(font, codepoints[i]);

        width += (float)glyph.advanceX * scale + spacing;
    }
    return (Vector2){width, fontSize};
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */

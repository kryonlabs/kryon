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
    var asc = Math.ceil(m.actualBoundingBoxAscent !== undefined
                        ? m.actualBoundingBoxAscent : size * 0.8);
    var desc = Math.ceil(m.actualBoundingBoxDescent !== undefined
                         ? m.actualBoundingBoxDescent : 2);
    var left = Math.ceil(m.actualBoundingBoxLeft !== undefined
                         ? m.actualBoundingBoxLeft : 0);
    var right = Math.ceil(m.actualBoundingBoxRight !== undefined
                          ? m.actualBoundingBoxRight : m.width);
    var gw = Math.max(right + left + 2, 1);
    var gh = Math.max(asc + desc + 2, 1);
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
    /* baseline sits asc+1 px into the cell; left bearing at left+1 */
    ctx.fillText(ch, left + 1, asc + 1);
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
    setValue(offy, -(asc + 1), 'i32');
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

GlyphInfo *LoadFontData(const unsigned char *fileData, int dataSize,
                        int fontSize, const int *codepoints,
                        int codepointCount, int type, int *glyphCount)
{
    Font font;

    (void)type;
    if(glyphCount != NULL)
        *glyphCount = 0;
    font = LoadFontFromMemory(".ttf", fileData, dataSize, fontSize,
                              codepoints, codepointCount);
    if(font.glyphs == NULL)
        return NULL;
    if(glyphCount != NULL)
        *glyphCount = font.glyphCount;
    /* caller owns the array; the transient Font's atlas is kept alive by
     * its texture id in JS (intentionally not unloaded) */
    return font.glyphs;
}

Font LoadFont(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Font font;

    if(data == NULL)
        return font;
    memset(&font, 0, sizeof(font));
    font = LoadFontFromMemory(".ttf", data, len, 16, NULL, 0);
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

static int canvas_glyph_index(const Font *font, int codepoint)
{
    int i;

    if(font == NULL || font->glyphs == NULL)
        return 0;
    for(i = 0; i < font->glyphCount; i++)
        if(font->glyphs[i].value == codepoint)
            return i;
    return 0;
}

int GetGlyphIndex(Font font, int codepoint)
{
    return canvas_glyph_index(&font, codepoint);
}

GlyphInfo GetGlyphInfo(Font font, int codepoint)
{
    int i = canvas_glyph_index(&font, codepoint);

    if(font.glyphs == NULL)
        return (GlyphInfo){0};
    return font.glyphs[i];
}

void DrawText(const char *text, int posX, int posY, int fontSize,
              Color color)
{
    Font font = GetFontDefault();
    float scale;
    float x;
    int i;

    if(text == NULL || font.glyphs == NULL || font.recs == NULL)
        return;
    scale = fontSize > 0 ? (float)fontSize / (float)font.baseSize : 1.0f;
    x = (float)posX;
    for(i = 0; text[i] != '\0'; ) {
        unsigned cp = 0;
        int gi;

        if((text[i] & 0x80) == 0) {
            cp = (unsigned char)text[i];
            i++;
        } else if((text[i] & 0xe0) == 0xc0 && text[i + 1] != '\0') {
            cp = ((unsigned char)text[i] & 0x1f) << 6 |
                 ((unsigned char)text[i + 1] & 0x3f);
            i += 2;
        } else if((text[i] & 0xf0) == 0xe0 && text[i + 1] != '\0' &&
                  text[i + 2] != '\0') {
            cp = ((unsigned char)text[i] & 0x0f) << 12 |
                 ((unsigned char)text[i + 1] & 0x3f) << 6 |
                 ((unsigned char)text[i + 2] & 0x3f);
            i += 3;
        } else {
            i++;
            continue;
        }
        gi = canvas_glyph_index(&font, (int)cp);
        DrawTexturePro(font.texture, font.recs[gi],
                       (Rectangle){x + font.glyphs[gi].offsetX * scale,
                                   (float)posY +
                                       font.glyphs[gi].offsetY * scale,
                                   font.recs[gi].width * scale,
                                   font.recs[gi].height * scale},
                       (Vector2){0, 0}, 0.0f, color);
        x += font.glyphs[gi].advanceX * scale;
    }
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */

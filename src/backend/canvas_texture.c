/*
 * canvas_texture.c — images, textures, render targets, image export.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * Textures are offscreen canvases in a JS registry (putImageData is
 * synchronous — no async ImageBitmap on the draw path), render targets
 * are the same canvases pushed on the draw-target stack.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

/* ------------------------------------------------------------------ */
/* JS side: texture registry, render targets, textured draw           */
/* ------------------------------------------------------------------ */

EM_JS(void, js_draw_texture_pro, (int id, double sx, double sy, double sw,
                                  double sh, double dx, double dy,
                                  double dw, double dh, double ox, double oy,
                                  double rot, int r, int gg, int bb, int aa),
{
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    var tex = K.textures[id];
    if (!ctx || !tex) return;
    var white = (r === 255 && gg === 255 && bb === 255 && aa === 255);
    if (!white) {
        /* tinted copies are cached per (texture, tint): multiply the RGB
         * channels, then restore the source alpha with destination-in */
        var key = id + ':' + r + ',' + gg + ',' + bb + ',' + aa;
        if (!K.tints) K.tints = {};
        if (!K.tints[key]) {
            var cv = K.makeCanvas(tex.width, tex.height);
            var c2 = cv.getContext('2d');
            c2.drawImage(tex, 0, 0);
            c2.globalCompositeOperation = 'multiply';
            c2.fillStyle = 'rgb(' + r + ',' + gg + ',' + bb + ')';
            c2.fillRect(0, 0, cv.width, cv.height);
            c2.globalCompositeOperation = 'destination-in';
            c2.drawImage(tex, 0, 0);
            K.tints[key] = cv;
        }
        tex = K.tints[key];
    }
    ctx.save();
    if (aa < 255) ctx.globalAlpha = aa / 255.0;
    ctx.translate(dx + ox, dy + oy);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    ctx.drawImage(tex, sx, sy, sw, sh, -ox, -oy, dw, dh);
    ctx.restore();
});

EM_JS(int, js_texture_from_rgba, (int ptr, int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv = K.makeCanvas(w, h);
    if (!cv) return 0;
    var c2 = cv.getContext('2d');
    var img = c2.createImageData(w, h);
    img.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
    c2.putImageData(img, 0, 0);
    var id = K.nextTex++;
    K.textures[id] = cv;
    return id;
});

EM_JS(void, js_texture_free, (int id), {
    delete globalThis.__kryCanvas.textures[id];
});

/* Create an offscreen canvas, register it as a texture, do NOT push it as
 * the draw target — BeginTextureMode selects it. */
EM_JS(int, js_render_target, (int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv = K.makeCanvas(w, h);
    if (!cv) return 0;
    var id = K.nextTex++;
    K.textures[id] = cv;
    return id;
});

EM_JS(void, js_target_select, (int id), {
    var K = globalThis.__kryCanvas;
    var cv = K.textures[id];
    if (cv) K.target.push({canvas: cv, ctx: cv.getContext('2d')});
});

EM_JS(void, js_target_deselect, (void), {
    globalThis.__kryCanvas.target.pop();
});

/* Read a texture (or the main canvas for id 0) into C memory, RGBA. */
EM_JS(int, js_texture_read, (int id, int ptr), {
    var K = globalThis.__kryCanvas;
    var cv = id === 0 ? K.canvas : K.textures[id];
    if (!cv || !cv.getContext) return 0;
    var d = cv.getContext('2d').getImageData(0, 0, cv.width, cv.height).data;
    HEAPU8.set(d, ptr);
    return 1;
});

/* ------------------------------------------------------------------ */
/* Images & textures                                                  */
/* ------------------------------------------------------------------ */

static Image canvas_image_from_rgba(unsigned char *rgba, int w, int h)
{
    Image img;

    memset(&img, 0, sizeof(img));
    img.data = rgba;
    img.width = w;
    img.height = h;
    img.mipmaps = 1;
    img.format = 1; /* PIXELFORMAT_UNCOMPRESSED_R8G8B8A8 */
    return img;
}

Image LoadImageFromMemory(const char *fileType, const unsigned char *fileData,
                          int dataSize)
{
    int w = 0;
    int h = 0;
    unsigned char *rgba;

    (void)fileType;
    if(fileData == NULL || dataSize <= 0)
        return canvas_image_from_rgba(NULL, 0, 0);
    rgba = kry_sw_png_rgba(fileData, (size_t)dataSize, &w, &h);
    return canvas_image_from_rgba(rgba, w, h);
}

Image LoadImage(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    Image img;

    if(data == NULL)
        return canvas_image_from_rgba(NULL, 0, 0);
    img = LoadImageFromMemory(".png", data, len);
    free(data);
    return img;
}

void UnloadImage(Image image)
{
    free(image.data);
}

Image LoadImageFromTexture(Texture2D texture)
{
    unsigned char *px;

    if(texture.id == 0 || texture.width <= 0 || texture.height <= 0)
        return canvas_image_from_rgba(NULL, 0, 0);
    px = malloc((size_t)texture.width * texture.height * 4);
    if(px == NULL)
        return canvas_image_from_rgba(NULL, 0, 0);
    if(js_texture_read((int)texture.id, (int)(size_t)px) == 0) {
        free(px);
        return canvas_image_from_rgba(NULL, 0, 0);
    }
    return canvas_image_from_rgba(px, texture.width, texture.height);
}

void ImageFlipVertical(Image *image)
{
    int w, h, y;
    unsigned char *row;

    if(image == NULL || image->data == NULL)
        return;
    w = image->width;
    h = image->height;
    row = malloc((size_t)w * 4);
    if(row == NULL)
        return;
    for(y = 0; y < h / 2; y++) {
        unsigned char *top = (unsigned char *)image->data + (size_t)y * w * 4;
        unsigned char *bot = (unsigned char *)image->data +
            (size_t)(h - 1 - y) * w * 4;

        memcpy(row, top, (size_t)w * 4);
        memcpy(top, bot, (size_t)w * 4);
        memcpy(bot, row, (size_t)w * 4);
    }
    free(row);
}

Texture2D LoadTextureFromImage(Image image)
{
    Texture2D tex;

    memset(&tex, 0, sizeof(tex));
    if(image.data == NULL || image.width <= 0 || image.height <= 0)
        return tex;
    tex.id = (unsigned)js_texture_from_rgba((int)(size_t)image.data,
                                            image.width, image.height);
    tex.width = image.width;
    tex.height = image.height;
    tex.mipmaps = 1;
    tex.format = 1;
    return tex;
}

Texture2D LoadTexture(const char *fileName)
{
    Image img = LoadImage(fileName);
    Texture2D tex = LoadTextureFromImage(img);

    UnloadImage(img);
    return tex;
}

void UnloadTexture(Texture2D texture)
{
    if(texture.id != 0)
        js_texture_free((int)texture.id);
}

void SetTextureFilter(Texture2D texture, int filter)
{
    (void)texture;
    (void)filter;
}

RenderTexture2D LoadRenderTexture(int width, int height)
{
    RenderTexture2D rt;

    memset(&rt, 0, sizeof(rt));
    rt.id = (unsigned)js_render_target(width, height);
    rt.texture.id = rt.id;
    rt.texture.width = width;
    rt.texture.height = height;
    rt.texture.mipmaps = 1;
    rt.texture.format = 1;
    return rt;
}

void UnloadRenderTexture(RenderTexture2D target)
{
    if(target.id != 0)
        js_texture_free((int)target.id);
}

void BeginTextureMode(RenderTexture2D target)
{
    js_target_select((int)target.id);
}

void EndTextureMode(void)
{
    js_target_deselect();
}

void DrawTexture(Texture2D texture, int posX, int posY, Color tint)
{
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width,
                               (float)texture.height},
                   (Rectangle){(float)posX, (float)posY,
                               (float)texture.width, (float)texture.height},
                   (Vector2){0, 0}, 0.0f, tint);
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    js_draw_texture_pro((int)texture.id, source.x, source.y,
                        source.width, source.height, dest.x, dest.y,
                        dest.width, dest.height, origin.x, origin.y,
                        rotation, tint.r, tint.g, tint.b, tint.a);
}

/* Image generation: one plain color (the surface subset apps use). */
Image GenImageColor(int width, int height, Color color)
{
    Image img;
    unsigned char *px;
    int i;

    memset(&img, 0, sizeof(img));
    if(width <= 0 || height <= 0)
        return img;
    px = malloc((size_t)width * height * 4);
    if(px == NULL)
        return img;
    for(i = 0; i < width * height; i++) {
        px[i * 4 + 0] = color.r;
        px[i * 4 + 1] = color.g;
        px[i * 4 + 2] = color.b;
        px[i * 4 + 3] = color.a;
    }
    img.data = px;
    img.width = width;
    img.height = height;
    img.mipmaps = 1;
    img.format = 1;
    return img;
}

/* ExportImage: reuse kry_screenshot.c's internal PNG writer. */
extern int kry_write_png_file(const char *path, const unsigned char *rgba,
                              int w, int h);

bool ExportImage(Image image, const char *fileName)
{
    if(image.data == NULL || fileName == NULL)
        return false;
    return kry_write_png_file(fileName, (const unsigned char *)image.data,
                              image.width, image.height) == 0;
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */

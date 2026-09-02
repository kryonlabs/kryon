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
                                  double rot, int r, int gg, int bb, int aa,
                                  int filter),
{
    var K = globalThis.__kryCanvas;
    var ctx = K.ctxNow();
    var tex = K.textures[id];
    if (!ctx || !tex) return;
    var flipX = false, flipY = false;
    /* Raylib re-anchors a negative source extent at the far corner and
     * mirrors it. Canvas textures are stored upright, so sprites mirror
     * via the flip transform below. Render targets are the exception:
     * raylib stores them bottom-up and the {0,0,w,-h} idiom exists to
     * draw them upright — on this backend that is a plain in-bounds copy
     * of |sh| rows starting at sy, with no mirror. */
    var isTarget = !!(K.targets && K.targets[id]);
    if (sw < 0) { sx += sw; sw = -sw; flipX = true; }
    if (sh < 0) {
        sh = -sh;
        if (!isTarget) { sy -= sh; flipY = true; }
    }
    if (dw < 0) { dw = -dw; flipX = !flipX; }
    if (dh < 0) { dh = -dh; flipY = !flipY; }
    var white = (r === 255 && gg === 255 && bb === 255 && aa === 255);
    if (!white) {
        /* tinted copies are cached per (texture, tint): multiply the RGB
         * channels, then restore the source alpha with destination-in */
        var key = id + ':' + r + ',' + gg + ',' + bb + ',' + aa;
        if (!K.tints) K.tints = {};
        if (!K.tints[key]) {
            var cv = K.makeCanvas(tex.width, tex.height);
            var c2 = cv.getContext('2d');
            c2.imageSmoothingEnabled = false;
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
    ctx.imageSmoothingEnabled = filter !== 0;
    if (aa < 255) ctx.globalAlpha = aa / 255.0;
    ctx.translate(dx + ox, dy + oy);
    if (rot !== 0.0) ctx.rotate(rot * Math.PI / 180.0);
    if (flipX || flipY) ctx.scale(flipX ? -1 : 1, flipY ? -1 : 1);
    ctx.drawImage(tex, sx, sy, sw, sh,
                  flipX ? ox - dw : -ox,
                  flipY ? oy - dh : -oy,
                  dw, dh);
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
    var K = globalThis.__kryCanvas;
    delete K.textures[id];
    if (K.filters) delete K.filters[id];
    if (K.targets) delete K.targets[id];
});

EM_JS(void, js_texture_update_rgba,
      (int id, int ptr, int x, int y, int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv = K && K.textures ? K.textures[id] : null;
    if (!cv || !cv.getContext || !ptr || w <= 0 || h <= 0) return;
    var c2 = cv.getContext('2d');
    var img = c2.createImageData(w, h);
    img.data.set(HEAPU8.subarray(ptr, ptr + w * h * 4));
    c2.putImageData(img, x, y);
    /* Tint copies contain pixels from the previous texture contents. */
    if (K.tints) K.tints = {};
});

EM_JS(void, js_texture_filter, (int id, int filter), {
    var K = globalThis.__kryCanvas;
    if (!K || !K.textures[id]) return;
    if (!K.filters) K.filters = {};
    K.filters[id] = filter | 0;
});

EM_JS(int, js_texture_filter_for, (int id), {
    var K = globalThis.__kryCanvas;
    if (!K || !K.filters || K.filters[id] === undefined) return 0;
    return K.filters[id] | 0;
});

/* Create an offscreen canvas, register it as a texture, do NOT push it as
 * the draw target — BeginTextureMode selects it. Ids registered here are
 * flagged in K.targets so js_draw_texture_pro can apply raylib's
 * render-target orientation semantics to them. */
EM_JS(int, js_render_target, (int w, int h), {
    var K = globalThis.__kryCanvas;
    var cv = K.makeCanvas(w, h);
    if (!cv) return 0;
    var id = K.nextTex++;
    K.textures[id] = cv;
    if (!K.targets) K.targets = {};
    K.targets[id] = 1;
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

void UpdateTexture(Texture2D texture, const void *pixels)
{
    if(texture.id == 0 || pixels == NULL || texture.width <= 0 ||
       texture.height <= 0)
        return;
    js_texture_update_rgba((int)texture.id, (int)(size_t)pixels, 0, 0,
                           texture.width, texture.height);
}

void UpdateTextureRec(Texture2D texture, Rectangle rec, const void *pixels)
{
    int x = (int)rec.x;
    int y = (int)rec.y;
    int width = (int)rec.width;
    int height = (int)rec.height;

    if(texture.id == 0 || pixels == NULL || width <= 0 || height <= 0)
        return;
    js_texture_update_rgba((int)texture.id, (int)(size_t)pixels,
                           x, y, width, height);
}

void SetTextureFilter(Texture2D texture, int filter)
{
    if(texture.id != 0)
        js_texture_filter((int)texture.id, filter);
}

bool IsTextureValid(Texture2D texture)
{
    return texture.id != 0 && texture.width > 0 && texture.height > 0;
}

bool IsRenderTextureValid(RenderTexture2D target)
{
    return target.id != 0 && IsTextureValid(target.texture);
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
    js_ctx_call(13, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
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

void DrawTextureV(Texture2D texture, Vector2 position, Color tint)
{
    DrawTexture(texture, (int)position.x, (int)position.y, tint);
}

void DrawTextureEx(Texture2D texture, Vector2 position, float rotation,
                   float scale, Color tint)
{
    DrawTexturePro(texture,
                   (Rectangle){0, 0, (float)texture.width,
                               (float)texture.height},
                   (Rectangle){position.x, position.y,
                               texture.width * scale,
                               texture.height * scale},
                   (Vector2){0, 0}, rotation, tint);
}

void DrawTextureRec(Texture2D texture, Rectangle rec, Vector2 position,
                    Color tint)
{
    DrawTexturePro(texture, rec,
                   (Rectangle){position.x, position.y,
                               rec.width < 0 ? -rec.width : rec.width,
                               rec.height < 0 ? -rec.height : rec.height},
                   (Vector2){0, 0}, 0.0f, tint);
}

void DrawTexturePro(Texture2D texture, Rectangle source, Rectangle dest,
                    Vector2 origin, float rotation, Color tint)
{
    js_draw_texture_pro((int)texture.id, source.x, source.y,
                        source.width, source.height, dest.x, dest.y,
                        dest.width, dest.height, origin.x, origin.y,
                        rotation, tint.r, tint.g, tint.b, tint.a,
                        js_texture_filter_for((int)texture.id));
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

bool IsImageValid(Image image)
{
    return image.data != NULL && image.width > 0 && image.height > 0 &&
           image.mipmaps > 0 && image.format > 0;
}

void ImageFormat(Image *image, int newFormat)
{
    if(image == NULL || image->data == NULL)
        return;
    if(newFormat == PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
        image->format = newFormat;
}

int kry_backend_capture_screen(Image *image)
{
    unsigned char *px;
    int w;
    int h;

    if(image == NULL)
        return -1;
    memset(image, 0, sizeof(*image));
    w = GetRenderWidth();
    h = GetRenderHeight();
    if(w <= 0 || h <= 0)
        return -1;
    px = malloc((size_t)w * h * 4);
    if(px == NULL)
        return -1;
    if(js_texture_read(0, (int)(size_t)px) == 0) {
        free(px);
        return -1;
    }
    *image = canvas_image_from_rgba(px, w, h);
    return 0;
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

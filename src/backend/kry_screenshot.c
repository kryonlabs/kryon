/* Front-end screenshot capture, defined once for every backend.
 *
 * raylib's TakeScreenshot unconditionally prefixes CORE.Storage.basePath -
 * on the SDL backend that is the directory holding the binary (SDL_GetBasePath)
 * - so an absolute output path arrived as "<bindir>//abs/path" and the export
 * silently failed (or landed somewhere unexpected). Kryon owns the surface
 * symbol (tools/generate-kryon-compat.sh lists it with the front-end-owned
 * names): absolute paths are used as-is, relative paths keep the raylib
 * behavior of resolving against the application directory. The pixels come
 * from LoadImageFromScreen(), which already applies the render-size and
 * HiDPI handling, so no backend internals are needed. */

#include "kryon.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* glReadBuffer is GL 1.3+; declared manually so this stays backend-neutral
 * (the raylib rename header renames raylib symbols, not GL entry points). */
extern void glReadPixels(int x, int y, int width, int height,
                         unsigned int format, unsigned int type,
                         void *data);
#define KR_GL_RGBA 0x1908u
#define KR_GL_UNSIGNED_BYTE 0x1401u

/* The raylib backend symbols these wrappers forward to (the rename header
 * only applies inside raylib sources). Weak: on null/canvas links they
 * stay unresolved and the capture path stays dormant. */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
extern void KryonRaylibBackend_EndDrawing(void);
#if defined(__GNUC__) || defined(__clang__)
__attribute__((weak))
#endif
extern void KryonRaylibBackend_rlDrawRenderBatch(void);

/* Event-driven programs opt into Kryon's frame pacing through
 * EnableEventWaiting().  Keep it after the backend frame completes so input
 * has already been collected for the next application update. */
extern void kry_event_wait_after_frame(void);

/* When KRYON_SHOT_ARM (or the older INBE_SHOT_ARM name) is set, every
 * EndDrawing captures the completed back buffer BEFORE the swap (the only
 * readback point OpenGL ES 2 guarantees: no glReadBuffer, and post-swap
 * reads return undefined/cleared data on Mesa's software drivers). The last
 * captured frame is what LoadImageFromScreen returns. */
static unsigned char *g_shot_buf = NULL;
static int g_shot_w = 0;
static int g_shot_h = 0;

void EndDrawing(void)
{
    if(KryonRaylibBackend_EndDrawing == NULL)
        return; /* non-raylib link: nothing to swap */
    if(getenv("KRYON_SHOT_ARM") == NULL && getenv("INBE_SHOT_ARM") == NULL) {
        KryonRaylibBackend_EndDrawing();
        kry_event_wait_after_frame();
        return;
    }
    if(KryonRaylibBackend_rlDrawRenderBatch != NULL)
        KryonRaylibBackend_rlDrawRenderBatch();
    {
        int w = GetRenderWidth();
        int h = GetRenderHeight();


        if(w > 0 && h > 0) {
            if(w != g_shot_w || h != g_shot_h) {
                free(g_shot_buf);
                g_shot_buf = malloc((size_t)w * h * 4);
                g_shot_w = w;
                g_shot_h = h;
            }
            if(g_shot_buf != NULL) {
                glReadPixels(0, 0, w, h, KR_GL_RGBA, KR_GL_UNSIGNED_BYTE,
                             g_shot_buf);
            }
        }
    }
    KryonRaylibBackend_EndDrawing();
    kry_event_wait_after_frame();
}


/* Returns the frame captured by the armed EndDrawing wrapper, flipped
 * vertically (glReadPixels origin is bottom-left) with alpha flattened
 * (the ARGB window visual renders transparent without a compositor). */

/* Minimal PNG writer (stored deflate): deterministic, no dependencies,
 * and deliberately not raylib's ExportImage — on this stack the raylib
 * exporter does not honor the passed image. */
static unsigned kr_png_crc_update(unsigned c, const unsigned char *p, size_t n)
{
    static unsigned t[256];
    static int built = 0;
    size_t i;

    if(!built) {
        int k;
        unsigned j;
        unsigned v;

        for(k = 0; k < 256; k++) {
            v = (unsigned)k;
            for(j = 0; j < 8; j++)
                v = (v & 1) ? 0xedb88320u ^ (v >> 1) : v >> 1;
            t[k] = v;
        }
        built = 1;
    }
    for(i = 0; i < n; i++)
        c = t[(c ^ p[i]) & 0xff] ^ (c >> 8);
    return c;
}

/* Chunk CRCs cover the chunk type followed by the chunk data. */
static unsigned kr_png_crc(const unsigned char *type, const unsigned char *data,
                           size_t n)
{
    return kr_png_crc_update(kr_png_crc_update(0xffffffffu, type, 4), data, n) ^
           0xffffffffu;
}

static void kr_png_be32(unsigned char *p, unsigned long v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

static int kr_write_png(const char *path, const unsigned char *rgba, int w,
                        int h)
{
    static const unsigned char sig[8] =
        {137, 80, 78, 71, 13, 10, 26, 10};
    unsigned char ihdr[13];
    unsigned char *raw;
    unsigned char *z;
    unsigned zn = 0;
    unsigned long left;
    const unsigned char *src;
    FILE *f;
    int y;
    size_t rowlen = (size_t)w * 4;
    size_t rawlen = (h + 1) * rowlen;

    f = fopen(path, "wb");
    if(f == NULL)
        return -1;
    raw = malloc(rawlen);
    z = malloc(rawlen + (rawlen / 65535 + 1) * 5 + 64);
    if(raw == NULL || z == NULL) {
        free(raw);
        free(z);
        fclose(f);
        return -1;
    }
    for(y = 0; y < h; y++) {
        raw[(size_t)y * (rowlen + 1)] = 0;
        memcpy(raw + (size_t)y * (rowlen + 1) + 1, rgba + (size_t)y * rowlen,
               rowlen);
    }
    z[zn++] = 0x78;
    z[zn++] = 0x01;
    src = raw;
    left = rawlen;
    while(left > 0) {
        unsigned chunk = left > 65535 ? 65535 : left;
        unsigned char bh[5];

        bh[0] = (left - chunk) == 0 ? 1 : 0;
        bh[1] = (unsigned char)(chunk & 0xff);
        bh[2] = (unsigned char)(chunk >> 8);
        bh[3] = (unsigned char)(~chunk & 0xff);
        bh[4] = (unsigned char)((~chunk >> 8) & 0xff);
        memcpy(z + zn, bh, 5);
        zn += 5;
        memcpy(z + zn, src, chunk);
        zn += chunk;
        src += chunk;
        left -= chunk;
    }
    {
        unsigned adler = 1;
        unsigned a = 1;
        unsigned b = 0;
        size_t i;

        for(i = 0; i < rawlen; i++) {
            a = (a + raw[i]) % 65521u;
            b = (b + a) % 65521u;
        }
        adler = (b << 16) | a;
        kr_png_be32(z + zn, adler);
        zn += 4;
    }
    fwrite(sig, 1, 8, f);
    kr_png_be32(ihdr, (unsigned long)w);
    kr_png_be32(ihdr + 4, (unsigned long)h);
    ihdr[8] = 8;
    ihdr[9] = 6;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;
    {
        unsigned char hdr[8];
        unsigned crc;

        kr_png_be32(hdr, 13);
        memcpy(hdr + 4, "IHDR", 4);
        fwrite(hdr, 1, 8, f);
        fwrite(ihdr, 1, 13, f);
        crc = kr_png_crc((const unsigned char *)"IHDR", ihdr, 13);
        kr_png_be32(hdr, crc);
        fwrite(hdr, 1, 4, f);
        kr_png_be32(hdr, zn);
        memcpy(hdr + 4, "IDAT", 4);
        fwrite(hdr, 1, 8, f);
        fwrite(z, 1, zn, f);
        crc = kr_png_crc((const unsigned char *)"IDAT", z, zn);
        kr_png_be32(hdr, crc);
        fwrite(hdr, 1, 4, f);
        kr_png_be32(hdr, 0);
        memcpy(hdr + 4, "IEND", 4);
        fwrite(hdr, 1, 8, f);
        kr_png_be32(hdr, kr_png_crc((const unsigned char *)"IEND",
                                    (const unsigned char *)"", 0));
        fwrite(hdr, 1, 4, f);
    }
    free(raw);
    free(z);
    fclose(f);
    return 0;
}

Image LoadImageFromScreen(void)
{
    Image image = { 0 };
    unsigned char *flip;
    int y;
    int row;

    if(g_shot_buf == NULL || g_shot_w <= 0 || g_shot_h <= 0)
        return image;
    row = g_shot_w * 4;
    flip = malloc((size_t)row * g_shot_h);
    if(flip == NULL)
        return image;
    for(y = 0; y < g_shot_h; y++)
        memcpy(flip + (size_t)y * row,
               g_shot_buf + (size_t)(g_shot_h - 1 - y) * row, (size_t)row);
    for(y = 0; y < g_shot_w * g_shot_h; y++)
        flip[y * 4 + 3] = 255;
    image.data = flip;
    image.width = g_shot_w;
    image.height = g_shot_h;
    image.mipmaps = 1;
    image.format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8;
    if(getenv("KRYON_SHOT_DEBUG") != NULL) {
        FILE *df = fopen("/tmp/shot-flip.raw", "wb");

        if(df != NULL) {
            fwrite(flip, 1, (size_t)row * g_shot_h, df);
            fclose(df);
        }
        kr_write_png("/tmp/kryon-native.png", flip, g_shot_w, g_shot_h);
    }
    return image;
}

/* Shared entry point for kryon's screenshot tooling; kr_write_png itself
 * stays file-local. Returns 0 on success. */
int
kry_write_png_file(const char *path, const unsigned char *rgba, int w, int h)
{
    return kr_write_png(path, rgba, w, h);
}

void TakeScreenshot(const char *fileName)
{
    if ((fileName == NULL) || (strchr(fileName, '\'') != NULL))
    {
        TraceLog(LOG_WARNING, "SYSTEM: Provided fileName could be potentially malicious, avoid ['] character");
        return;
    }

    Image image = LoadImageFromScreen();

    char path[1024] = { 0 };
    if (fileName[0] == '/')
    {
        snprintf(path, sizeof(path), "%s", fileName);
    }
    else
    {
        snprintf(path, sizeof(path), "%s/%s", GetApplicationDirectory(), fileName);
    }

    ExportImage(image, path);
    UnloadImage(image);

    if (FileExists(path)) TraceLog(LOG_INFO, "SYSTEM: [%s] Screenshot taken successfully", path);
    else TraceLog(LOG_WARNING, "SYSTEM: [%s] Screenshot could not be saved", path);
}

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

/* When INBE_SHOT_ARM=1, every EndDrawing captures the completed back
 * buffer BEFORE the swap (the only readback point OpenGL ES 2 guarantees:
 * no glReadBuffer, and post-swap reads return undefined/cleared data on
 * Mesa's software drivers). The last captured frame is what
 * LoadImageFromScreen returns. */
static unsigned char *g_shot_buf = NULL;
static int g_shot_w = 0;
static int g_shot_h = 0;

void EndDrawing(void)
{
    if(KryonRaylibBackend_EndDrawing == NULL)
        return; /* non-raylib link: nothing to swap */
    if(getenv("INBE_SHOT_ARM") == NULL) {
        KryonRaylibBackend_EndDrawing();
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
            if(g_shot_buf != NULL)
                glReadPixels(0, 0, w, h, KR_GL_RGBA, KR_GL_UNSIGNED_BYTE,
                             g_shot_buf);
        }
    }
    KryonRaylibBackend_EndDrawing();
}


/* Returns the frame captured by the armed EndDrawing wrapper, flipped
 * vertically (glReadPixels origin is bottom-left) with alpha flattened
 * (the ARGB window visual renders transparent without a compositor). */
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
    return image;
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

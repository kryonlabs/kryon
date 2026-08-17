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
extern void glReadBuffer(unsigned int mode);
extern void glReadPixels(int x, int y, int width, int height,
                         unsigned int format, unsigned int type,
                         void *data);
#define KR_GL_FRONT 0x0404u
#define KR_GL_RGBA 0x1908u
#define KR_GL_UNSIGNED_BYTE 0x1401u

/* Front-buffer capture with flattened alpha. raylib's stock readback runs
 * after SwapBuffers, which on software GL (llvmpipe, headless X, CI xvfb)
 * returns a cleared/transparent back buffer — the front buffer holds the
 * presented frame. Alpha is forced opaque: the ARGB window visual renders
 * transparent everywhere without a compositor, which would also break
 * external X captures. */
Image LoadImageFromScreen(void)
{
    Image image = { 0 };
    int width = GetRenderWidth();
    int height = GetRenderHeight();
    unsigned char *data;
    int y;

    if (width <= 0 || height <= 0)
        return image;
    data = (unsigned char *)RL_CALLOC(width * height * 4, 1);

    glReadBuffer(KR_GL_FRONT);
    glReadPixels(0, 0, width, height, KR_GL_RGBA, KR_GL_UNSIGNED_BYTE, data);
    glReadBuffer(0x0405u); /* GL_BACK */

    /* flip vertically (glReadPixels origin is bottom-left) and flatten alpha */
    {
        int row = width * 4;
        unsigned char *flip = (unsigned char *)RL_CALLOC(row * height, 1);

        for (y = 0; y < height; y++) {
            memcpy(flip + (size_t)y * row, data + (size_t)(height - 1 - y) * row,
                   (size_t)row);
        }
        for (y = 0; y < width * height; y++)
            flip[y * 4 + 3] = 255;
        RL_FREE(data);
        data = flip;
    }
    image.data = data;
    image.width = width;
    image.height = height;
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

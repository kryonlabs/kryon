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
#include <string.h>

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

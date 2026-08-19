/*
 * canvas_os.c — clipboard, file I/O, URLs, misc surface.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * File I/O rides on Emscripten's MEMFS/IDBFS; the clipboard keeps a
 * local mirror because navigator.clipboard is async.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

/* ------------------------------------------------------------------ */
/* OS services                                                        */
/* ------------------------------------------------------------------ */

/* navigator.clipboard is async, so the canvas backend keeps a mirror of
 * the last text the app wrote: copy/paste round-trips within the app,
 * and the browser write is attempted fire-and-forget. */
static char g_clipboard[4096];

const char *GetClipboardText(void)
{
    return g_clipboard;
}

void SetClipboardText(const char *text)
{
    if(text == NULL)
        return;
    snprintf(g_clipboard, sizeof(g_clipboard), "%s", text);
    EM_ASM({
        if (globalThis.navigator && globalThis.navigator.clipboard &&
            globalThis.navigator.clipboard.writeText)
            globalThis.navigator.clipboard.writeText(UTF8ToString($0));
    }, text);
}

bool FileExists(const char *fileName)
{
    struct stat st;

    return fileName != NULL && stat(fileName, &st) == 0 &&
           !S_ISDIR(st.st_mode);
}

bool DirectoryExists(const char *dirPath)
{
    struct stat st;

    return dirPath != NULL && stat(dirPath, &st) == 0 && S_ISDIR(st.st_mode);
}

const char *GetDirectoryPath(const char *filePath)
{
    static char dir[1024];
    const char *slash;

    if(filePath == NULL)
        return ".";
    slash = strrchr(filePath, '/');
    if(slash == NULL || slash == filePath)
        return ".";
    snprintf(dir, sizeof(dir), "%.*s", (int)(slash - filePath), filePath);
    return dir;
}

int MakeDirectory(const char *dirPath)
{
    if(dirPath == NULL)
        return 0;
    return mkdir(dirPath, 0777) == 0 ? 1 : 0;
}

bool SaveFileData(const char *fileName, const void *data, int bytesToWrite)
{
    FILE *f;

    if(fileName == NULL || data == NULL || bytesToWrite < 0)
        return 0;
    f = fopen(fileName, "wb");
    if(f == NULL)
        return 0;
    if(fwrite(data, 1, (size_t)bytesToWrite, f) != (size_t)bytesToWrite) {
        fclose(f);
        return 0;
    }
    fclose(f);
    return 1;
}

bool SaveFileText(const char *fileName, const char *text)
{
    if(text == NULL)
        return 0;
    return SaveFileData(fileName, text, (int)strlen(text));
}

int ChangeDirectory(const char *dir)
{
    if(dir == NULL)
        return 0;
    return chdir(dir) == 0;
}

const char *GetWorkingDirectory(void)
{
    static char cwd[1024];

    if(getcwd(cwd, sizeof(cwd)) == NULL)
        return ".";
    return cwd;
}

unsigned char *LoadFileData(const char *fileName, int *dataSize)
{
    FILE *f;
    long len;
    unsigned char *data;

    if(dataSize != NULL)
        *dataSize = 0;
    if(fileName == NULL)
        return NULL;
    f = fopen(fileName, "rb");
    if(f == NULL)
        return NULL;
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(len <= 0) {
        fclose(f);
        return NULL;
    }
    data = malloc((size_t)len);
    if(data == NULL || fread(data, 1, (size_t)len, f) != (size_t)len) {
        free(data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    if(dataSize != NULL)
        *dataSize = (int)len;
    return data;
}

void UnloadFileData(unsigned char *data)
{
    free(data);
}

char *LoadFileText(const char *fileName)
{
    int len = 0;
    unsigned char *data = LoadFileData(fileName, &len);
    char *text;

    if(data == NULL)
        return NULL;
    text = malloc((size_t)len + 1);
    if(text != NULL) {
        memcpy(text, data, (size_t)len);
        text[len] = '\0';
    }
    free(data);
    return text;
}

void UnloadFileText(char *text)
{
    free(text);
}

void OpenURL(const char *url)
{
    if(url == NULL)
        return;
    EM_ASM({
        if (typeof window !== 'undefined')
            window.open(UTF8ToString($0), '_blank');
    }, url);
}

/* ------------------------------------------------------------------ */
/* Misc surface                                                       */
/* ------------------------------------------------------------------ */

const char *TextFormat(const char *text, ...)
{
    static char buffers[16][256];
    static int next;
    va_list args;

    va_start(args, text);
    vsnprintf(buffers[next], sizeof(buffers[0]), text, args);
    va_end(args);
    return buffers[next++ % 16];
}

void TraceLog(int logLevel, const char *text, ...)
{
    va_list args;

    (void)logLevel;
    va_start(args, text);
    vfprintf(stderr, text, args);
    fputc('\n', stderr);
    va_end(args);
}

/* File-name helpers apps drive directly (raylib utils). */
static const char *canvas_basename(const char *path)
{
    const char *slash = path != NULL ? strrchr(path, '/') : NULL;

    return slash != NULL && slash[1] != '\0' ? slash + 1 : path;
}

const char *GetFileName(const char *filePath)
{
    return canvas_basename(filePath);
}

const char *GetFileExtension(const char *fileName)
{
    const char *base = canvas_basename(fileName);
    const char *dot = base != NULL ? strrchr(base, '.') : NULL;

    return dot != NULL && dot[1] != '\0' ? dot : "";
}

bool IsFileExtension(const char *fileName, const char *ext)
{
    const char *got = GetFileExtension(fileName);

    return got != NULL && ext != NULL && strcmp(got, ext) == 0;
}

void SetShapesTexture(Texture2D texture, Rectangle rec)
{
    (void)texture;
    (void)rec;
}

void SetTraceLogCallback(TraceLogCallback callback)
{
    (void)callback;
}

#else /* !__EMSCRIPTEN__ */

/* Native builds that sweep kryon's src/ tree (every vendoring app's
 * Makefile does a find over vendor/kryon/src) compile this to an empty
 * translation unit. KRYON_BACKEND=canvas is web-only; selecting it for a
 * native link fails at symbol resolution instead of #error here. */

#endif /* __EMSCRIPTEN__ */

/*
 * canvas_os.c — clipboard, file I/O, URLs, misc surface.
 *
 * Part of the Tier A HTML5 Canvas2D backend; see canvas_internal.h.
 * File I/O rides on Emscripten's MEMFS/IDBFS; the clipboard keeps a
 * local mirror because navigator.clipboard is async.
 */

#ifdef __EMSCRIPTEN__

#include "canvas_internal.h"

#include <dirent.h>
#include <time.h>

/* ------------------------------------------------------------------ */
/* OS services                                                        */
/* ------------------------------------------------------------------ */

/* navigator.clipboard is async, so the canvas backend keeps a mirror of
 * the last text the app wrote: copy/paste round-trips within the app,
 * and the browser write is attempted fire-and-forget. */
static char g_clipboard[4096];

EM_JS(int, js_dropped_count, (void), {
    var K = globalThis.__kryCanvas;
    if (K && K.droppedPending > 0) return 0;
    return K && K.dropped ? K.dropped.length : 0;
});

EM_JS(char *, js_dropped_path, (int index), {
    var K = globalThis.__kryCanvas;
    var path = K && K.dropped && index >= 0 && index < K.dropped.length ?
        K.dropped[index] : "";
    var len = lengthBytesUTF8(path) + 1;
    var ptr = _malloc(len);
    stringToUTF8(path, ptr, len);
    return ptr;
});

EM_JS(void, js_clipboard_pull, (char *dst, int cap), {
    var K = globalThis.__kryCanvas;
    if (!K || !dst || cap <= 0) return;
    stringToUTF8(K.clipboard || "", dst, cap);
});

EM_JS(void, js_clipboard_push, (const char *text), {
    var value = text ? UTF8ToString(text) : "";
    var K = globalThis.__kryCanvas;
    if (K) K.clipboard = value;
    var tryExecCommandCopy = function () {
        if (typeof document === 'undefined' || !document.execCommand)
            return false;
        var input = document.createElement('textarea');
        input.value = value;
        input.setAttribute('readonly', 'readonly');
        input.style.position = 'fixed';
        input.style.left = '-10000px';
        input.style.top = '-10000px';
        document.body.appendChild(input);
        input.focus();
        input.select();
        var ok = false;
        try { ok = !!document.execCommand('copy'); } catch (_) {}
        input.remove();
        if (K && K.canvas && K.canvas.focus) {
            try { K.canvas.focus({preventScroll: true}); } catch (_) {
                try { K.canvas.focus(); } catch (_) {}
            }
        }
        return ok;
    };
    var nowMs = (typeof performance !== 'undefined' && performance.now) ?
        performance.now() : Date.now();
    var gestureActive = K && nowMs <= (K.clipboardGestureUntil || 0);
    if (gestureActive)
        tryExecCommandCopy();
    if (globalThis.navigator && globalThis.navigator.clipboard &&
        globalThis.navigator.clipboard.writeText)
        globalThis.navigator.clipboard.writeText(value).catch(function () {
            if (!gestureActive)
                tryExecCommandCopy();
        });
});

const char *GetClipboardText(void)
{
    js_clipboard_pull(g_clipboard, (int)sizeof(g_clipboard));
    return g_clipboard;
}

void SetClipboardText(const char *text)
{
    if(text == NULL)
        return;
    snprintf(g_clipboard, sizeof(g_clipboard), "%s", text);
    js_clipboard_push(text);
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

static int canvas_mkdir_recursive(const char *dirPath)
{
    char tmp[1024];
    size_t len;
    size_t i;

    if(dirPath == NULL || dirPath[0] == '\0')
        return -1;
    snprintf(tmp, sizeof(tmp), "%s", dirPath);
    len = strlen(tmp);
    while(len > 1 && tmp[len - 1] == '/')
        tmp[--len] = '\0';
    for(i = 1; i < len; i++) {
        if(tmp[i] != '/')
            continue;
        tmp[i] = '\0';
        if(tmp[0] != '\0' && !DirectoryExists(tmp) &&
           mkdir(tmp, 0777) != 0)
            return -1;
        tmp[i] = '/';
    }
    if(!DirectoryExists(tmp) && mkdir(tmp, 0777) != 0)
        return -1;
    return DirectoryExists(tmp) ? 0 : -1;
}

int MakeDirectory(const char *dirPath)
{
    if(dirPath == NULL)
        return -1;
    return canvas_mkdir_recursive(dirPath);
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
        return -1;
    return chdir(dir) == 0 ? 0 : -1;
}

const char *GetWorkingDirectory(void)
{
    static char cwd[1024];

    if(getcwd(cwd, sizeof(cwd)) == NULL)
        return ".";
    return cwd;
}

long GetFileModTime(const char *fileName)
{
    struct stat st;

    if(fileName == NULL || stat(fileName, &st) != 0)
        return 0;
    return (long)st.st_mtime;
}

int GetFileLength(const char *fileName)
{
    struct stat st;

    if(fileName == NULL || stat(fileName, &st) != 0 || S_ISDIR(st.st_mode))
        return 0;
    return (int)st.st_size;
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

const char *GetFileNameWithoutExt(const char *filePath)
{
    static char name[1024];
    const char *base = canvas_basename(filePath);
    const char *dot;
    int len;

    if(base == NULL)
        return "";
    dot = strrchr(base, '.');
    len = dot != NULL ? (int)(dot - base) : (int)strlen(base);
    if(len >= (int)sizeof(name))
        len = (int)sizeof(name) - 1;
    memcpy(name, base, (size_t)len);
    name[len] = '\0';
    return name;
}

const char *GetPrevDirectoryPath(const char *dirPath)
{
    static char prev[1024];
    char *slash;
    size_t len;

    if(dirPath == NULL || dirPath[0] == '\0')
        return ".";
    snprintf(prev, sizeof(prev), "%s", dirPath);
    len = strlen(prev);
    while(len > 1 && prev[len - 1] == '/')
        prev[--len] = '\0';
    slash = strrchr(prev, '/');
    if(slash == NULL)
        return ".";
    if(slash == prev) {
        prev[1] = '\0';
        return prev;
    }
    *slash = '\0';
    return prev;
}

bool IsPathFile(const char *path)
{
    return FileExists(path);
}

bool IsPathDirectory(const char *path)
{
    return DirectoryExists(path);
}

bool IsFileNameValid(const char *fileName)
{
    return fileName != NULL && fileName[0] != '\0' &&
           strchr(fileName, '\0') == fileName + strlen(fileName);
}

static void canvas_free_path_list(FilePathList files)
{
    unsigned int i;

    if(files.paths == NULL)
        return;
    for(i = 0; i < files.count; i++)
        free(files.paths[i]);
    free(files.paths);
}

FilePathList LoadDroppedFiles(void)
{
    FilePathList files = {0};
    int count = js_dropped_count();
    int i;

    if(count <= 0)
        return files;
    files.paths = calloc((size_t)count, sizeof(char *));
    if(files.paths == NULL)
        return files;
    files.count = (unsigned int)count;
    for(i = 0; i < count; i++)
        files.paths[i] = js_dropped_path(i);
    return files;
}

bool IsFileDropped(void)
{
    return js_dropped_count() > 0;
}

void UnloadDroppedFiles(FilePathList files)
{
    canvas_free_path_list(files);
}

FilePathList LoadDirectoryFiles(const char *dirPath)
{
    return LoadDirectoryFilesEx(dirPath, NULL, false);
}

FilePathList LoadDirectoryFilesEx(const char *basePath, const char *filter,
                                  bool scanSubdirs)
{
    FilePathList files = {0};
    DIR *dir;
    struct dirent *entry;
    char **paths = NULL;
    unsigned int count = 0;
    unsigned int capacity = 0;

    (void)scanSubdirs;
    dir = opendir(basePath != NULL ? basePath : ".");
    if(dir == NULL)
        return files;
    while((entry = readdir(dir)) != NULL) {
        char path[1024];
        char *copy;

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        if(filter != NULL && filter[0] != '\0' &&
           strcmp(filter, "*.*") != 0 && !IsFileExtension(entry->d_name, filter))
            continue;
        snprintf(path, sizeof(path), "%s/%s", basePath != NULL ? basePath : ".",
                 entry->d_name);
        if(count == capacity) {
            unsigned int new_capacity = capacity == 0 ? 8 : capacity * 2;
            char **grown = realloc(paths, (size_t)new_capacity * sizeof(char *));

            if(grown == NULL)
                break;
            paths = grown;
            capacity = new_capacity;
        }
        copy = malloc(strlen(path) + 1);
        if(copy == NULL)
            break;
        strcpy(copy, path);
        paths[count++] = copy;
    }
    closedir(dir);
    files.count = count;
    files.paths = paths;
    return files;
}

void UnloadDirectoryFiles(FilePathList files)
{
    canvas_free_path_list(files);
}

unsigned int GetDirectoryFileCount(const char *dirPath)
{
    FilePathList files = LoadDirectoryFiles(dirPath);
    unsigned int count = files.count;

    UnloadDirectoryFiles(files);
    return count;
}

unsigned int GetDirectoryFileCountEx(const char *basePath, const char *filter,
                                     bool scanSubdirs)
{
    FilePathList files = LoadDirectoryFilesEx(basePath, filter, scanSubdirs);
    unsigned int count = files.count;

    UnloadDirectoryFiles(files);
    return count;
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

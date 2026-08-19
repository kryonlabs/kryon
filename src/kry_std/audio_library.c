#include "audio_library.h"
#include "kryon.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define KRY_AUDIO_MKDIR(p) _mkdir(p)
#else
#include <sys/stat.h>
#define KRY_AUDIO_MKDIR(p) mkdir((p), 0755)
#endif

static int
audio_mkdir_one(const char *path)
{
    if(path == 0 || path[0] == '\0')
        return 0;
    if(DirectoryExists(path))
        return 1;
    if(KRY_AUDIO_MKDIR(path) == 0 || errno == EEXIST)
        return 1;
    return DirectoryExists(path) ? 1 : 0;
}

int
KryAudioFileValid(const char *path, const char *extensions)
{
    FILE *file;

    if(path == 0 || path[0] == '\0' ||
       extensions == 0 || extensions[0] == '\0')
        return 0;
    if(!IsFileExtension(path, extensions))
        return 0;
    file = fopen(path, "rb");
    if(file == 0)
        return 0;
    fclose(file);
    return 1;
}

int
KryAudioCopyFile(const char *src, const char *dst)
{
    FILE *in;
    FILE *out;
    unsigned char buf[8192];
    size_t n;
    int ok = 1;

    if(src == 0 || dst == 0 || src[0] == '\0' || dst[0] == '\0')
        return 0;
    in = fopen(src, "rb");
    if(in == 0)
        return 0;
    out = fopen(dst, "wb");
    if(out == 0) {
        fclose(in);
        return 0;
    }
    while((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if(fwrite(buf, 1, n, out) != n) {
            ok = 0;
            break;
        }
    }
    if(ferror(in))
        ok = 0;
    fclose(out);
    fclose(in);
    return ok;
}

void
KryAudioTitleFromPath(char *out, size_t out_size, const char *path)
{
    const char *name;
    const char *ext;
    size_t len;

    if(out == 0 || out_size == 0)
        return;
    name = GetFileName(path != 0 ? path : "");
    if(name == 0 || name[0] == '\0')
        name = "Custom audio";
    snprintf(out, out_size, "%s", name);
    ext = GetFileExtension(out);
    len = strlen(out);
    if(ext != 0 && ext[0] == '.' && strlen(ext) < len)
        out[len - strlen(ext)] = '\0';
    if(out[0] != '\0')
        out[0] = (char)toupper((unsigned char)out[0]);
}

int
KryAudioEnsureLibraryDir(const char *root, const char *kind,
                         char *out, size_t out_size)
{
    char base[512];

    if(root == 0 || root[0] == '\0' || kind == 0 || kind[0] == '\0' ||
       out == 0 || out_size == 0)
        return 0;
    snprintf(base, sizeof(base), "%s/audio", root);
    if(!audio_mkdir_one(base))
        return 0;
    snprintf(out, out_size, "%s/%s", base, kind);
    return audio_mkdir_one(out);
}

int
KryAudioImportItem(KryAudioLibraryItem item, int index,
                   const char *root, const char *kind,
                   const char *extensions, const char *src,
                   int *error_code)
{
    char dir[512];
    char dst[512];
    const char *ext;

    if(error_code != 0)
        *error_code = KRY_AUDIO_IMPORT_COPY_FAILED;
    if(item.title == 0 || item.title_size == 0 ||
       item.path == 0 || item.path_size == 0 || index < 0) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_FULL;
        return 0;
    }
    if(src == 0 || src[0] == '\0') {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_INVALID_PATH;
        return 0;
    }
    if(!KryAudioFileValid(src, extensions)) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_INVALID_FORMAT;
        return 0;
    }
    if(!FileExists(src)) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_FILE_NOT_FOUND;
        return 0;
    }
    if(!KryAudioEnsureLibraryDir(root, kind, dir, sizeof(dir))) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_COPY_FAILED;
        return 0;
    }
    ext = GetFileExtension(src);
    if(ext == 0 || ext[0] == '\0')
        ext = ".ogg";
    if(snprintf(dst, sizeof(dst), "%s/custom-%02d%s", dir, index + 1, ext) >=
       (int)sizeof(dst)) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_COPY_FAILED;
        return 0;
    }
    if(!KryAudioCopyFile(src, dst)) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_COPY_FAILED;
        return 0;
    }
    if(!KryAudioFileValid(dst, extensions)) {
        if(error_code != 0)
            *error_code = KRY_AUDIO_IMPORT_INVALID_FORMAT;
        return 0;
    }
    KryAudioTitleFromPath(item.title, item.title_size, src);
    snprintf(item.path, item.path_size, "%s", dst);
    if(error_code != 0)
        *error_code = KRY_AUDIO_IMPORT_OK;
    return 1;
}

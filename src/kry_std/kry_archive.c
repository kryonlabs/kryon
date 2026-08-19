#include "kry_archive.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#define KRY_MKDIR_ONE(p) _mkdir(p)
#else
#include <sys/stat.h>
#define KRY_MKDIR_ONE(p) mkdir((p), 0755)
#endif

#if defined(__has_include)
#if __has_include("miniz.h")
#define KRY_HAVE_MINIZ 1
#include "miniz.h"
#endif
#endif

int
KryArchiveMkdirP(const char *path)
{
    char partial[1024];
    size_t len;

    if(path == 0 || path[0] == '\0')
        return 0;
    len = strlen(path);
    if(len >= sizeof(partial))
        return 0;
    memcpy(partial, path, len + 1);
    for(size_t i = 1; i < len; i++) {
        if(partial[i] != '/' && partial[i] != '\\')
            continue;
        partial[i] = '\0';
        if(partial[0] != '\0' &&
           KRY_MKDIR_ONE(partial) != 0 && errno != EEXIST)
            return 0;
        partial[i] = path[i];
    }
    if(KRY_MKDIR_ONE(partial) != 0 && errno != EEXIST)
        return 0;
    return 1;
}

int
KryArchiveEntryNameSafe(const char *name)
{
    const char *p = name;

    if(name == 0 || name[0] == '\0')
        return 0;
    if(name[0] == '/' || name[0] == '\\')
        return 0;
    if(((name[0] >= 'a' && name[0] <= 'z') ||
        (name[0] >= 'A' && name[0] <= 'Z')) && name[1] == ':')
        return 0;
    while(*p != '\0') {
        if(p[0] == '.' && p[1] == '.' &&
           (p[2] == '/' || p[2] == '\\' || p[2] == '\0')) {
            if(p == name || p[-1] == '/' || p[-1] == '\\')
                return 0;
        }
        p++;
    }
    return 1;
}

int
KryArchiveExtractZip(const char *zip_path, const char *dest_dir)
{
#if KRY_HAVE_MINIZ
    mz_zip_archive zip;
    mz_uint count;
    int ok = 1;

    if(zip_path == 0 || dest_dir == 0 || dest_dir[0] == '\0')
        return 0;
    if(!KryArchiveMkdirP(dest_dir))
        return 0;
    memset(&zip, 0, sizeof(zip));
    if(!mz_zip_reader_init_file(&zip, zip_path, 0))
        return 0;
    count = mz_zip_reader_get_num_files(&zip);
    for(mz_uint i = 0; i < count; i++) {
        char dest[1024];
        mz_zip_archive_file_stat st;

        if(!mz_zip_reader_file_stat(&zip, i, &st)) {
            ok = 0;
            break;
        }
        if(mz_zip_reader_is_file_a_directory(&zip, i))
            continue;
        if(!KryArchiveEntryNameSafe(st.m_filename)) {
            ok = 0;
            break;
        }
        if(snprintf(dest, sizeof(dest), "%s/%s", dest_dir, st.m_filename) >=
           (int)sizeof(dest)) {
            ok = 0;
            break;
        }
        {
            char *last_slash = strrchr(dest, '/');
            char *last_backslash = strrchr(dest, '\\');
            char *last = last_slash > last_backslash ? last_slash : last_backslash;

            if(last != 0) {
                char saved = *last;

                *last = '\0';
                if(!KryArchiveMkdirP(dest)) {
                    ok = 0;
                    break;
                }
                *last = saved;
            }
        }
        if(!mz_zip_reader_extract_to_file(&zip, i, dest, 0)) {
            ok = 0;
            break;
        }
    }
    mz_zip_reader_end(&zip);
    return ok;
#else
    (void)zip_path;
    (void)dest_dir;
    return 0;
#endif
}

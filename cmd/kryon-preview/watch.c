#define _POSIX_C_SOURCE 200809L

#include "session.h"
#include "watch.h"

#include <dirent.h>
#include <string.h>
#include <sys/stat.h>

static uint64_t
hash_bytes(uint64_t hash, const void *value, size_t length)
{
    const unsigned char *bytes = value;

    for(size_t i = 0; i < length; i++) {
        hash ^= bytes[i];
        hash *= UINT64_C(1099511628211);
    }
    return hash;
}

static int
source_file(const char *name)
{
    const char *extensions[] = {".kry", ".kss", ".c", ".h", ".cc", ".cpp",
        ".hpp", ".mk", ".kryon", ".json", ".txt", ".png", ".jpg", ".ttf", ".otf"};
    const char *extension = strrchr(name, '.');

    if(strcmp(name, "Makefile") == 0 || strcmp(name, "GNUmakefile") == 0)
        return 1;
    if(extension == NULL)
        return 0;
    for(size_t i = 0; i < sizeof(extensions) / sizeof(extensions[0]); i++) {
        if(strcmp(extension, extensions[i]) == 0)
            return 1;
    }
    return 0;
}

static int
source_stamp(const char *directory, uint64_t *stamp, int depth)
{
    DIR *stream;
    struct dirent *entry;
    int ok = 1;

    if(depth > 64)
        return 0;
    stream = opendir(directory);
    if(stream == NULL)
        return 0;
    while((entry = readdir(stream)) != NULL) {
        char filename[PREVIEW_PATH_MAX];
        struct stat metadata;
        uint64_t hash;
        int length;

        if(entry->d_name[0] == '.' || strcmp(entry->d_name, "build") == 0 ||
           strcmp(entry->d_name, "vendor") == 0 || strcmp(entry->d_name, "dist") == 0 ||
           strcmp(entry->d_name, "node_modules") == 0)
            continue;
        length = snprintf(filename, sizeof(filename), "%s/%s", directory, entry->d_name);
        if(length < 0 || (size_t)length >= sizeof(filename) || lstat(filename, &metadata) != 0) {
            ok = 0;
            break;
        }
        if(S_ISDIR(metadata.st_mode)) {
            if(!source_stamp(filename, stamp, depth + 1)) {
                ok = 0;
                break;
            }
            continue;
        }
        if(!S_ISREG(metadata.st_mode) || !source_file(entry->d_name))
            continue;
        hash = hash_bytes(UINT64_C(14695981039346656037), filename, (size_t)length);
        hash = hash_bytes(hash, &metadata.st_size, sizeof(metadata.st_size));
#if defined(__APPLE__)
        hash = hash_bytes(hash, &metadata.st_mtimespec.tv_sec, sizeof(metadata.st_mtimespec.tv_sec));
        hash = hash_bytes(hash, &metadata.st_mtimespec.tv_nsec, sizeof(metadata.st_mtimespec.tv_nsec));
#else
        hash = hash_bytes(hash, &metadata.st_mtim.tv_sec, sizeof(metadata.st_mtim.tv_sec));
        hash = hash_bytes(hash, &metadata.st_mtim.tv_nsec, sizeof(metadata.st_mtim.tv_nsec));
#endif
        /* Directory enumeration order must not cause a rebuild. */
        *stamp += hash;
    }
    closedir(stream);
    return ok;
}

int
PreviewSourceStamp(const char *project, uint64_t *stamp)
{
    *stamp = 0;
    return source_stamp(project, stamp, 0);
}

#ifndef KRYON_AUDIO_LIBRARY_H
#define KRYON_AUDIO_LIBRARY_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum KryAudioImportStatus {
    KRY_AUDIO_IMPORT_OK = 1,
    KRY_AUDIO_IMPORT_INVALID_PATH = -1,
    KRY_AUDIO_IMPORT_INVALID_FORMAT = -2,
    KRY_AUDIO_IMPORT_FILE_NOT_FOUND = -3,
    KRY_AUDIO_IMPORT_COPY_FAILED = -4,
    KRY_AUDIO_IMPORT_FULL = -5
} KryAudioImportStatus;

typedef struct KryAudioLibraryItem {
    char *title;
    size_t title_size;
    char *path;
    size_t path_size;
} KryAudioLibraryItem;

int KryAudioFileValid(const char *path, const char *extensions);
int KryAudioCopyFile(const char *src, const char *dst);
void KryAudioTitleFromPath(char *out, size_t out_size, const char *path);
int KryAudioEnsureLibraryDir(const char *root, const char *kind,
                             char *out, size_t out_size);
int KryAudioImportItem(KryAudioLibraryItem item, int index,
                       const char *root, const char *kind,
                       const char *extensions, const char *src,
                       int *error_code);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_AUDIO_LIBRARY_H */

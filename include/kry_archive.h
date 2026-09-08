#ifndef KRYON_KRY_ARCHIVE_H
#define KRYON_KRY_ARCHIVE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int KryArchiveMkdirP(const char *path);
int KryArchiveEntryNameSafe(const char *name);
int KryArchiveExtractZip(const char *zip_path, const char *dest_dir);

typedef struct Archive {
    void *impl;
} Archive;

typedef struct ArchiveEntry {
    char name[512];
    int is_directory;
    unsigned long long uncompressed_size;
} ArchiveEntry;

typedef enum ArchiveCompression {
    ARCHIVE_STORE = 0,
    ARCHIVE_DEFLATE = 9
} ArchiveCompression;

int ArchiveEntryNameSafe(const char *name);
int ArchiveOpenZip(Archive *archive, const char *path);
void ArchiveClose(Archive *archive);
int ArchiveEntryCount(Archive *archive);
int ArchiveReadEntry(Archive *archive, int index, ArchiveEntry *entry);
int ArchiveFindEntry(Archive *archive, const char *name);
void *ArchiveReadEntryHeap(Archive *archive, int index, size_t *out_size);
void *ArchiveReadNamedEntryHeap(Archive *archive, const char *name,
                                size_t *out_size);
int ArchiveExtractEntry(Archive *archive, int index, const char *path);

int ArchiveCreateZip(Archive *archive, const char *path);
int ArchiveAddMemory(Archive *archive, const char *name,
                     const void *data, size_t data_size,
                     ArchiveCompression compression);
int ArchiveFinishZip(Archive *archive);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_ARCHIVE_H */

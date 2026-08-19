#ifndef KRYON_KRY_ARCHIVE_H
#define KRYON_KRY_ARCHIVE_H

#ifdef __cplusplus
extern "C" {
#endif

int KryArchiveMkdirP(const char *path);
int KryArchiveEntryNameSafe(const char *name);
int KryArchiveExtractZip(const char *zip_path, const char *dest_dir);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_KRY_ARCHIVE_H */

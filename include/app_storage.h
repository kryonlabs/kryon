#ifndef KRYON_APP_STORAGE_H
#define KRYON_APP_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

int KryAppStorageGetString(const char *scope, const char *key,
                           const char *fallback, char *out, int out_size);
int KryAppStorageSetString(const char *scope, const char *key,
                           const char *value);
int KryAppStorageGetInt(const char *scope, const char *key,
                        int fallback, int *out);
int KryAppStorageSetInt(const char *scope, const char *key, int value);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_APP_STORAGE_H */

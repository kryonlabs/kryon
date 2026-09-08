#ifndef KRYON_APP_STORAGE_H
#define KRYON_APP_STORAGE_H

#ifdef __cplusplus
extern "C" {
#endif

/* Per-user data directory for the given application id, created on
 * first use: $XDG_DATA_HOME/<id> (or $HOME/.local/share/<id>) on the
 * desktops, $LOCALAPPDATA/<id> on Windows, $home/.local/share/<id> on
 * Plan 9. The returned pointer is static and stays valid. */
const char *AppDataRoot(const char *app_id);

int AppStorageGetString(const char *scope, const char *key,
                           const char *fallback, char *out, int out_size);
int AppStorageSetString(const char *scope, const char *key,
                           const char *value);
int AppStorageGetInt(const char *scope, const char *key,
                        int fallback, int *out);
int AppStorageSetInt(const char *scope, const char *key, int value);

#ifdef __cplusplus
}
#endif

#endif /* KRYON_APP_STORAGE_H */

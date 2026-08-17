/* Plan-08 capability host backends: portable reference implementations.
 *
 * Storage: key/value strings in a single flat file (one "key\tvalue\n"
 * per line) under a caller-set directory — KRB_CAP_STORE_DIR, else the
 * process cwd. Audio: defers to the host's sound system through weak
 * hooks (kryon's raylib path installs them; headless hosts get silence).
 * HTTP: deferred to the transport hook for the same reason — the
 * reference hosts are offline-first.
 *
 * All entry points take argv-style packed argument blocks: a KrbCapArgs
 * buffer built with krba_push(), so a single KrbFn signature serves
 * every capability. */

#include "krb.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- packed argument blocks ------------------------------------------ */

typedef struct KrbCapArgs {
    char buf[512];
    unsigned len;
} KrbCapArgs;

static const char *
krba_get(const void *block, unsigned *cursor)
{
    const char *s;

    if(block == NULL || *cursor >= 512)
        return "";
    s = (const char *)block + *cursor;
    *cursor += (unsigned)strlen(s) + 1;
    return s;
}

/* ---- storage ---------------------------------------------------------- */

static void
store_path(char *out, size_t out_size)
{
    const char *dir = getenv("KRB_CAP_STORE_DIR");

    if(dir == NULL || dir[0] == '\0')
        dir = ".";
    snprintf(out, out_size, "%s/krb-cap-store.txt", dir);
}

int
KrbCapStorageGet(const char *key, char *out, unsigned out_size)
{
    char path[512];
    FILE *f;
    char line[512];
    size_t klen = strlen(key);
    int found = -1;

    if(key == NULL || out == NULL || out_size == 0)
        return -1;
    out[0] = '\0';
    store_path(path, sizeof(path));
    f = fopen(path, "r");
    if(f == NULL)
        return -1;
    while(fgets(line, sizeof(line), f) != NULL) {
        size_t n = strlen(line);

        while(n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
            line[--n] = '\0';
        if(strncmp(line, key, klen) == 0 && line[klen] == '\t') {
            snprintf(out, out_size, "%s", line + klen + 1);
            found = (int)strlen(out);
            break;
        }
    }
    fclose(f);
    return found;
}

int
KrbCapStorageSet(const char *key, const char *value)
{
    char path[512];
    char tmp[520];
    FILE *f;
    char line[512];
    size_t klen = strlen(key);
    int replaced = 0;

    if(key == NULL || key[0] == '\0')
        return -1;
    if(value == NULL)
        value = "";
    store_path(path, sizeof(path));
    snprintf(tmp, sizeof(tmp), "%s.tmp", path);
    f = fopen(path, "r");
    if(f != NULL) {
        FILE *o = fopen(tmp, "w");

        if(o == NULL) {
            fclose(f);
            return -1;
        }
        while(fgets(line, sizeof(line), f) != NULL) {
            size_t n = strlen(line);

            while(n > 0 && (line[n - 1] == '\n' || line[n - 1] == '\r'))
                line[--n] = '\0';
            if(strncmp(line, key, klen) == 0 && line[klen] == '\t') {
                fprintf(o, "%s\t%s\n", key, value);
                replaced = 1;
            } else if(n > 0) {
                fprintf(o, "%s\n", line);
            }
        }
        fclose(f);
        fclose(o);
        if(rename(tmp, path) != 0)
            return -1;
    }
    if(!replaced) {
        f = fopen(path, "a");
        if(f == NULL)
            return -1;
        fprintf(f, "%s\t%s\n", key, value);
        fclose(f);
    }
    return 0;
}

/* ---- audio hooks (installed by the raylib surface host) --------------- */

int (*KrbCapAudioHook)(const char *asset_path, int stop) = NULL;

/* ---- KrbFn adaptors --------------------------------------------------- */

static int
cap_storage_get(void *ud)
{
    const char *key = krba_get(ud, &(unsigned){0});
    char out[384];

    (void)KrbCapStorageGet(key, out, sizeof(out));
    return 0;
}

static int
cap_storage_set(void *ud)
{
    unsigned cur = 0;
    const char *key = krba_get(ud, &cur);
    const char *value = krba_get(ud, &cur);

    return KrbCapStorageSet(key, value);
}

static int
cap_audio_play(void *ud)
{
    const char *path = krba_get(ud, &(unsigned){0});

    if(KrbCapAudioHook != NULL)
        return KrbCapAudioHook(path, 0);
    return -1; /* no audio system in this host */
}

static int
cap_audio_stop(void *ud)
{
    (void)ud;
    if(KrbCapAudioHook != NULL)
        return KrbCapAudioHook(NULL, 1);
    return -1;
}

/* ---- one-call installer ----------------------------------------------- */

int
KrbCapInstallDefaults(KrbImage *img)
{
    int n = 0;

    if(KrbCapBind(img, KRB_CAP_STORAGE_GET, cap_storage_get, NULL) >= 0)
        n++;
    if(KrbCapBind(img, KRB_CAP_STORAGE_SET, cap_storage_set, NULL) >= 0)
        n++;
    if(KrbCapBind(img, KRB_CAP_AUDIO_PLAY, cap_audio_play, NULL) >= 0)
        n++;
    if(KrbCapBind(img, KRB_CAP_AUDIO_STOP, cap_audio_stop, NULL) >= 0)
        n++;
    return n;
}

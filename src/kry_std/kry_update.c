/*
 * kry_update.c - desktop update checks for the Kry standard library.
 * Appcast fetching rides kry_http; parsing rides kry_json. Nothing here
 * downloads or replaces files.
 */
#include "kry_update.h"
#include "kry_filesystem.h"
#include "kry_http.h"
#include "kry_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static void
copy_str(char *dst, size_t cap, const char *src)
{
    if(src == NULL)
        src = "";
    snprintf(dst, cap, "%s", src);
}

static const char *
env_or_empty(const char *name)
{
    const char *v = getenv(name);

    return v != NULL ? v : "";
}

/* --- channel detection --------------------------------------------------- */

KryUpdateChannel
kry_update_detect_channel(void)
{
    if(env_or_empty("APPIMAGE")[0] != '\0')
        return KRY_UPDATE_CHANNEL_APPIMAGE;
    if(env_or_empty("SNAP")[0] != '\0')
        return KRY_UPDATE_CHANNEL_SNAP;
    if(env_or_empty("FLATPAK_ID")[0] != '\0')
        return KRY_UPDATE_CHANNEL_FLATPAK;
#ifdef _WIN32
    return KRY_UPDATE_CHANNEL_WINDOWS_PORTABLE;
#else
    {
        char path[1024];

        /* /proc/self/exe (Linux) and /proc/self/file (FreeBSD) survive
         * PATH tricks and cwd changes; neither exists on macOS yet. */
        if(kry_fs_realpath("/proc/self/exe", path, sizeof(path)) ||
           kry_fs_realpath("/proc/self/file", path, sizeof(path))) {
            if(strncmp(path, "/usr/", 5) == 0 || strncmp(path, "/opt/", 5) == 0)
                return KRY_UPDATE_CHANNEL_PACKAGE;
            return KRY_UPDATE_CHANNEL_SOURCE;
        }
    }
    return KRY_UPDATE_CHANNEL_UNKNOWN;
#endif
}

const char *
kry_update_channel_name(KryUpdateChannel channel)
{
    switch(channel) {
    case KRY_UPDATE_CHANNEL_APPIMAGE:         return "AppImage";
    case KRY_UPDATE_CHANNEL_SNAP:             return "Snap";
    case KRY_UPDATE_CHANNEL_FLATPAK:          return "Flatpak";
    case KRY_UPDATE_CHANNEL_WINDOWS_PORTABLE: return "Windows portable";
    case KRY_UPDATE_CHANNEL_PACKAGE:          return "System package";
    case KRY_UPDATE_CHANNEL_SOURCE:           return "Source build";
    case KRY_UPDATE_CHANNEL_UNKNOWN:          break;
    }
    return "Unknown";
}

const char *
kry_update_channel_key(KryUpdateChannel channel)
{
    switch(channel) {
    case KRY_UPDATE_CHANNEL_APPIMAGE:         return "appimage";
    case KRY_UPDATE_CHANNEL_WINDOWS_PORTABLE: return "windows";
    case KRY_UPDATE_CHANNEL_SNAP:
    case KRY_UPDATE_CHANNEL_FLATPAK:
    case KRY_UPDATE_CHANNEL_PACKAGE:
    case KRY_UPDATE_CHANNEL_SOURCE:
    case KRY_UPDATE_CHANNEL_UNKNOWN:          break;
    }
    return NULL;
}

/* --- versions ------------------------------------------------------------ */

static int
parse_version(const char *v, long out[4])
{
    int i;
    int digits = 0;

    out[0] = out[1] = out[2] = out[3] = 0;
    if(v == NULL)
        return 0;
    if(v[0] == 'v' || v[0] == 'V')
        v++;
    for(i = 0; i < 4 && *v != '\0'; i++) {
        long n = 0;

        while(*v >= '0' && *v <= '9') {
            n = n * 10 + (*v - '0');
            v++;
            digits++;
        }
        out[i] = n;
        if(*v != '.')
            break;
        v++;
    }
    return digits;
}

int
kry_update_version_compare(const char *a, const char *b)
{
    long va[4], vb[4];
    int i;

    parse_version(a, va);
    parse_version(b, vb);
    for(i = 0; i < 4; i++) {
        if(va[i] != vb[i])
            return va[i] < vb[i] ? -1 : 1;
    }
    return 0;
}

/* --- appcast ------------------------------------------------------------- */

const KryUpdateChannelInfo *
kry_update_find_channel(const KryUpdateInfo *info, const char *name)
{
    int i;

    if(info == NULL || name == NULL)
        return NULL;
    for(i = 0; i < info->channel_count && i < KRY_UPDATE_MAX_CHANNELS; i++) {
        if(strcmp(info->channels[i].name, name) == 0)
            return &info->channels[i];
    }
    return NULL;
}

static void
parse_channel(const KryJson *obj, KryUpdateChannelInfo *out)
{
    const KryJson *v;

    v = kry_json_get(obj, "url");
    if(v != NULL && kry_json_type(v) == KRY_JSON_STRING)
        copy_str(out->url, sizeof(out->url), kry_json_string(v));
    v = kry_json_get(obj, "sha256");
    if(v != NULL && kry_json_type(v) == KRY_JSON_STRING)
        copy_str(out->sha256, sizeof(out->sha256), kry_json_string(v));
    v = kry_json_get(obj, "size");
    if(v != NULL && kry_json_type(v) == KRY_JSON_NUMBER)
        out->size = (unsigned long)kry_json_number(v);
}

int
kry_update_appcast_parse(const char *json, KryUpdateInfo *out)
{
    KryJson *root;
    const KryJson *v;
    const KryJson *channels;
    int count;
    int i;

    if(json == NULL || out == NULL)
        return 0;
    memset(out, 0, sizeof(*out));
    root = kry_json_parse(json);
    if(root == NULL)
        return 0;
    v = kry_json_get(root, "version");
    if(v == NULL || kry_json_type(v) != KRY_JSON_STRING ||
       kry_json_string(v)[0] == '\0') {
        kry_json_free(root);
        return 0;
    }
    copy_str(out->version, sizeof(out->version), kry_json_string(v));
    v = kry_json_get(root, "date");
    if(v != NULL && kry_json_type(v) == KRY_JSON_STRING)
        copy_str(out->date, sizeof(out->date), kry_json_string(v));
    v = kry_json_get(root, "notes");
    if(v != NULL && kry_json_type(v) == KRY_JSON_STRING)
        copy_str(out->notes, sizeof(out->notes), kry_json_string(v));
    v = kry_json_get(root, "notes_url");
    if(v != NULL && kry_json_type(v) == KRY_JSON_STRING)
        copy_str(out->notes_url, sizeof(out->notes_url), kry_json_string(v));

    channels = kry_json_get(root, "channels");
    count = channels != NULL ? kry_json_count(channels) : 0;
    for(i = 0; i < count && out->channel_count < KRY_UPDATE_MAX_CHANNELS;
        i++) {
        const KryJson *entry = kry_json_at(channels, i);
        const char *name = kry_json_key(channels, i);

        if(entry == NULL || name == NULL ||
           kry_json_type(entry) != KRY_JSON_OBJECT)
            continue;
        copy_str(out->channels[out->channel_count].name,
                 sizeof(out->channels[0].name), name);
        parse_channel(entry, &out->channels[out->channel_count]);
        out->channel_count++;
    }
    kry_json_free(root);
    return 1;
}

/* --- async check --------------------------------------------------------- */

struct KryUpdateCheck {
    KryHttpRequest *req;   /* NULL once resolved or never started */
    char current_version[32];
    int resolved;
    KryUpdateStatus status;
    KryUpdateInfo info;
    char error[128];
};

KryUpdateCheck *
kry_update_check(const char *appcast_url, const char *current_version)
{
    KryUpdateCheck *c;

    if(appcast_url == NULL || current_version == NULL)
        return NULL;
    c = calloc(1, sizeof(*c));
    if(c == NULL)
        return NULL;
    copy_str(c->current_version, sizeof(c->current_version), current_version);
    c->status = KRY_UPDATE_PENDING;
    c->req = kry_http_get(appcast_url, 30);
    if(c->req == NULL) {
        free(c);
        return NULL;
    }
    return c;
}

KryUpdateStatus
kry_update_poll(KryUpdateCheck *check)
{
    KryHttpStatus s;
    const char *body;

    if(check == NULL)
        return KRY_UPDATE_FAILED;
    if(check->resolved)
        return check->status;
    s = kry_http_poll(check->req);
    if(s == KRY_HTTP_PENDING || s == KRY_HTTP_RUNNING)
        return KRY_UPDATE_PENDING;

    body = kry_http_response(check->req);
    if(s == KRY_HTTP_DONE) {
        if(kry_update_appcast_parse(body, &check->info)) {
            check->status = kry_update_version_compare(check->info.version,
                                                       check->current_version) > 0
                              ? KRY_UPDATE_AVAILABLE
                              : KRY_UPDATE_UP_TO_DATE;
        } else {
            copy_str(check->error, sizeof(check->error),
                     body != NULL && body[0] != '\0'
                       ? "invalid appcast" : "empty appcast");
            check->status = KRY_UPDATE_FAILED;
        }
    } else {
        copy_str(check->error, sizeof(check->error),
                 body != NULL ? body : "request failed");
        check->status = KRY_UPDATE_FAILED;
    }
    kry_http_free(check->req);   /* body is fully copied; release the worker */
    check->req = NULL;
    check->resolved = 1;
    return check->status;
}

const KryUpdateInfo *
kry_update_info(KryUpdateCheck *check)
{
    if(check == NULL || !check->resolved)
        return NULL;
    if(check->status != KRY_UPDATE_AVAILABLE &&
       check->status != KRY_UPDATE_UP_TO_DATE)
        return NULL;
    return &check->info;
}

const char *
kry_update_error(KryUpdateCheck *check)
{
    if(check == NULL || check->status != KRY_UPDATE_FAILED)
        return NULL;
    return check->error;
}

void
kry_update_free(KryUpdateCheck *check)
{
    if(check == NULL)
        return;
    kry_http_free(check->req);
    free(check);
}

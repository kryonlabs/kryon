/*
 * kry_update.c - desktop update checks for the Kry standard library.
 * Appcast fetching rides kry_http; parsing rides kry_json; artifact
 * downloads stream through kry_http_download and are verified with
 * kry_sha256 before any apply step touches the installation.
 */
#include "kry_update.h"
#include "kry_filesystem.h"
#include "kry_http.h"
#include "kry_json.h"
#include "kry_sha256.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifdef _WIN32
#include <windows.h>
#include <direct.h>
#include <io.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#endif

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
            if(strncmp(path, "/usr/bin/", 9) == 0 ||
               strncmp(path, "/usr/local/", 11) == 0 ||
               strncmp(path, "/opt/", 5) == 0)
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
    case KRY_UPDATE_CHANNEL_APPIMAGE:
        /* AppImages are per-arch, so the appcast key carries the arch:
         * "appimage-amd64", "appimage-arm64". Windows zips bundle every
         * arch exe, so that key stays bare. */
#if defined(__aarch64__) || defined(_M_ARM64)
        return "appimage-arm64";
#elif defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
        return "appimage-amd64";
#else
        return "appimage";
#endif
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

/* --- self-update --------------------------------------------------------- */

static int
ensure_dir(const char *path)
{
#ifdef _WIN32
    /* recursive _mkdir chain (kry_fs_mkdir_p is POSIX-only today) */
    char partial[MAX_PATH];
    size_t i;
    size_t len;

    len = strlen(path);
    if(len == 0 || len >= sizeof(partial))
        return 0;
    memcpy(partial, path, len + 1);
    /* walk parent prefixes; anything at or before index 2 is a drive root
     * ("C:\") which _mkdir cannot create */
    for(i = 3; i < len; i++) {
        if(partial[i] != '\\' && partial[i] != '/')
            continue;
        partial[i] = '\0';
        if(_mkdir(partial) != 0 && errno != EEXIST)
            return 0;
        partial[i] = path[i];
    }
    if(_mkdir(partial) != 0 && errno != EEXIST)
        return 0;
    return 1;
#else
    return kry_fs_mkdir_p(path) == 0;
#endif
}

int
kry_update_download_dir(const char *app_name, char *out, int cap)
{
    const char *base;

    if(app_name == NULL || app_name[0] == '\0' || out == NULL || cap <= 0)
        return 0;
#ifdef _WIN32
    {
        char root[MAX_PATH];

        base = getenv("LOCALAPPDATA");
        if(base == NULL || base[0] == '\0')
            return 0;
        snprintf(root, sizeof(root), "%s", base);
        base = root;
        if(snprintf(out, (size_t)cap, "%s\\%s\\updates", base, app_name) >= cap)
            return 0;
    }
#else
    base = getenv("XDG_DATA_HOME");
    if(base != NULL && base[0] != '\0') {
        if(snprintf(out, (size_t)cap, "%s/%s/updates", base, app_name) >= cap)
            return 0;
    } else {
        const char *home = getenv("HOME");

        if(home == NULL || home[0] == '\0')
            return 0;
        if(snprintf(out, (size_t)cap, "%s/.local/share/%s/updates",
                    home, app_name) >= cap)
            return 0;
    }
#endif
    return ensure_dir(out);
}

/* Last URL path segment (query string stripped). A name with no path, an
 * empty result, or a ".." is rejected — the appcast is remote input. */
static const char *
url_file_name(const char *url)
{
    const char *slash;
    const char *name;
    size_t len;

    if(url == NULL)
        return NULL;
    slash = strrchr(url, '/');
    if(slash == NULL)
        return NULL;
    name = slash + 1;
    len = strcspn(name, "?#");
    if(len == 0 || len >= 128)
        return NULL;
    if(len == 2 && name[0] == '.' && name[1] == '.')
        return NULL;
    return name;
}

struct KryUpdateDownload {
    KryHttpDownload *http;
    char dest_path[600];
    char expect_sha256[65];
    int resolved;
    KryUpdateDownloadStatus status;
    char error[160];
};

KryUpdateDownload *
kry_update_download_begin(const KryUpdateChannelInfo *entry, const char *dest_dir)
{
    KryUpdateDownload *dl;
    const char *name;

    if(entry == NULL || entry->url[0] == '\0' || dest_dir == NULL)
        return NULL;
    dl = calloc(1, sizeof(*dl));
    if(dl == NULL)
        return NULL;
    name = url_file_name(entry->url);
    if(name == NULL) {
        copy_str(dl->error, sizeof(dl->error), "no file name in URL");
        dl->status = KRY_UPDATE_DL_FAILED;
        dl->resolved = 1;
        return dl;
    }
    if(snprintf(dl->dest_path, sizeof(dl->dest_path), "%s/%.*s", dest_dir,
                (int)strcspn(name, "?#"), name) >= (int)sizeof(dl->dest_path)) {
        copy_str(dl->error, sizeof(dl->error), "destination path too long");
        dl->status = KRY_UPDATE_DL_FAILED;
        dl->resolved = 1;
        return dl;
    }
    copy_str(dl->expect_sha256, sizeof(dl->expect_sha256), entry->sha256);
    if(!ensure_dir(dest_dir)) {
        copy_str(dl->error, sizeof(dl->error), "cannot create download dir");
        dl->status = KRY_UPDATE_DL_FAILED;
        dl->resolved = 1;
        return dl;
    }
    dl->http = kry_http_download(entry->url, dl->dest_path, 900);
    if(dl->http == NULL) {
        copy_str(dl->error, sizeof(dl->error), "download unavailable");
        dl->status = KRY_UPDATE_DL_FAILED;
        dl->resolved = 1;
        return dl;
    }
    dl->status = KRY_UPDATE_DL_PENDING;
    return dl;
}

KryUpdateDownloadStatus
kry_update_download_poll(KryUpdateDownload *dl)
{
    KryHttpStatus s;

    if(dl == NULL)
        return KRY_UPDATE_DL_FAILED;
    if(dl->resolved)
        return dl->status;
    if(dl->http == NULL) {
        dl->resolved = 1;
        dl->status = KRY_UPDATE_DL_FAILED;
        return dl->status;
    }
    s = kry_http_download_poll(dl->http);
    if(s == KRY_HTTP_PENDING)
        return KRY_UPDATE_DL_PENDING;
    if(s == KRY_HTTP_RUNNING)
        return KRY_UPDATE_DL_RUNNING;

    dl->resolved = 1;
    if(s == KRY_HTTP_DONE) {
        if(dl->expect_sha256[0] != '\0') {
            char got[65];

            if(!kry_sha256_file(dl->dest_path, got)) {
                copy_str(dl->error, sizeof(dl->error), "cannot hash download");
                dl->status = KRY_UPDATE_DL_FAILED;
                remove(dl->dest_path);
            } else if(!kry_sha256_hex_equal(got, dl->expect_sha256)) {
                copy_str(dl->error, sizeof(dl->error), "checksum mismatch");
                dl->status = KRY_UPDATE_DL_FAILED;
                remove(dl->dest_path);
            } else {
                dl->status = KRY_UPDATE_DL_DONE;
            }
        } else {
            dl->status = KRY_UPDATE_DL_DONE;
        }
    } else {
        const char *err = kry_http_download_error(dl->http);

        copy_str(dl->error, sizeof(dl->error), err != NULL ? err : "download failed");
        dl->status = KRY_UPDATE_DL_FAILED;
        remove(dl->dest_path);
    }
    return dl->status;
}

double
kry_update_download_progress(const KryUpdateDownload *dl)
{
    return kry_http_download_progress(dl != NULL ? dl->http : NULL);
}

const char *
kry_update_download_error(const KryUpdateDownload *dl)
{
    if(dl == NULL || dl->status != KRY_UPDATE_DL_FAILED)
        return NULL;
    return dl->error;
}

const char *
kry_update_download_path(const KryUpdateDownload *dl)
{
    if(dl == NULL || dl->status != KRY_UPDATE_DL_DONE)
        return NULL;
    return dl->dest_path;
}

void
kry_update_download_free(KryUpdateDownload *dl)
{
    if(dl == NULL)
        return;
    kry_http_download_free(dl->http);
    free(dl);
}

static int
copy_file_mode(const char *src, const char *dst, unsigned mode)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[8192];
    size_t got;

    if(in == NULL)
        return 0;
    out = fopen(dst, "wb");
    if(out == NULL) {
        fclose(in);
        return 0;
    }
    while((got = fread(buf, 1, sizeof(buf), in)) > 0) {
        if(fwrite(buf, 1, got, out) != got) {
            fclose(in);
            fclose(out);
            remove(dst);
            return 0;
        }
    }
    if(ferror(in)) {
        fclose(in);
        fclose(out);
        remove(dst);
        return 0;
    }
    fclose(in);
    if(fclose(out) != 0) {
        remove(dst);
        return 0;
    }
#ifdef _WIN32
    (void)mode;
#else
    if(mode != 0 && chmod(dst, (mode_t)mode) != 0) {
        remove(dst);
        return 0;
    }
#endif
    return 1;
}

static void
split_path(const char *path, char *dir, int dir_cap, char *name, int name_cap)
{
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *bslash = strrchr(path, '\\');
    const char *sep = slash != NULL && (bslash == NULL || slash > bslash)
                        ? slash : bslash;
#else
    const char *sep = slash;
#endif

    if(sep == NULL) {
        copy_str(dir, (size_t)dir_cap, ".");
        copy_str(name, (size_t)name_cap, path);
        return;
    }
    snprintf(dir, (size_t)dir_cap, "%.*s", (int)(sep - path), path);
    copy_str(name, (size_t)name_cap, sep + 1);
}

int
kry_update_appimage_stage(const char *downloaded_path, const char *appimage_path)
{
#ifndef _WIN32
    char target_dir[512];
    char name[160];
    char staged[700];

    if(downloaded_path == NULL || appimage_path == NULL ||
       downloaded_path[0] == '\0' || appimage_path[0] == '\0')
        return 0;
    if(access(downloaded_path, R_OK) != 0)
        return 0;
    split_path(appimage_path, target_dir, sizeof(target_dir), name, sizeof(name));
    if(name[0] == '\0' || target_dir[0] == '\0')
        return 0;
    /* stage inside the AppImage's own directory so the final rename is a
     * same-filesystem atomic replace */
    if(snprintf(staged, sizeof(staged), "%s/.%s.new", target_dir, name) >=
       (int)sizeof(staged))
        return 0;
    if(!copy_file_mode(downloaded_path, staged, 0755))
        return 0;
    if(rename(staged, appimage_path) != 0) {
        remove(staged);
        return 0;
    }
    return 1;
#else
    (void)downloaded_path; (void)appimage_path;
    return 0;
#endif
}

KryUpdateApplyResult
kry_update_appimage_apply(const char *downloaded_path)
{
#ifndef _WIN32
    const char *appimage = getenv("APPIMAGE");

    if(appimage == NULL || appimage[0] == '\0')
        return KRY_UPDATE_APPLY_NOT_APPLICABLE;
    if(!kry_update_appimage_stage(downloaded_path, appimage))
        return KRY_UPDATE_APPLY_FAILED;
    execl(appimage, appimage, (char *)NULL);
    return KRY_UPDATE_APPLY_FAILED;   /* exec only returns on error */
#else
    (void)downloaded_path;
    return KRY_UPDATE_APPLY_NOT_APPLICABLE;
#endif
}

#ifdef _WIN32

static int
windows_spawn_detached(const char *cmdline)
{
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    memset(&si, 0, sizeof(si));
    si.cb = sizeof(si);
    memset(&pi, 0, sizeof(pi));
    if(!CreateProcessA(NULL, (LPSTR)cmdline, NULL, NULL, FALSE,
                       CREATE_NO_WINDOW | DETACHED_PROCESS, NULL, NULL,
                       &si, &pi))
        return 0;
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return 1;
}

KryUpdateApplyResult
kry_update_windows_stage_swap(const char *new_dir)
{
    char exe_path[MAX_PATH];
    char old_dir[MAX_PATH];
    char old_bak[MAX_PATH + 8];
    char exe_name[MAX_PATH];
    char script_path[MAX_PATH];
    char temp_dir[MAX_PATH];
    char cmdline[MAX_PATH * 2];
    FILE *f;
    const char *slash;
    DWORD len;

    if(new_dir == NULL || new_dir[0] == '\0')
        return KRY_UPDATE_APPLY_NOT_APPLICABLE;
    len = GetModuleFileNameA(NULL, exe_path, sizeof(exe_path));
    if(len == 0 || len >= sizeof(exe_path))
        return KRY_UPDATE_APPLY_FAILED;
    slash = strrchr(exe_path, '\\');
    if(slash == NULL)
        return KRY_UPDATE_APPLY_FAILED;
    snprintf(old_dir, sizeof(old_dir), "%.*s", (int)(slash - exe_path), exe_path);
    copy_str(exe_name, sizeof(exe_name), slash + 1);
    if(GetTempPathA(sizeof(temp_dir), temp_dir) == 0)
        return KRY_UPDATE_APPLY_FAILED;
    if(snprintf(script_path, sizeof(script_path), "%sinbe-update.cmd",
                temp_dir) >= (int)sizeof(script_path))
        return KRY_UPDATE_APPLY_FAILED;
    snprintf(old_bak, sizeof(old_bak), "%s.old", old_dir);

    f = fopen(script_path, "wb");
    if(f == NULL)
        return KRY_UPDATE_APPLY_FAILED;
    /* CRLF line endings: cmd.exe batch files should carry them */
    fprintf(f, "@echo off\r\n");
    fprintf(f, "powershell -NoProfile -Command \"while (Get-Process -Id %lu "
               "-ErrorAction SilentlyContinue) { Start-Sleep -Milliseconds 300 }\"\r\n",
            (unsigned long)GetCurrentProcessId());
    fprintf(f, "move /y \"%s\" \"%s\" >nul || exit /b 1\r\n", old_dir, old_bak);
    fprintf(f, "move /y \"%s\" \"%s\" >nul || exit /b 1\r\n", new_dir, old_dir);
    fprintf(f, "start \"\" \"%s\\%s\"\r\n", old_dir, exe_name);
    fprintf(f, "rd /s /q \"%s\" >nul 2>&1\r\n", old_bak);
    fclose(f);

    snprintf(cmdline, sizeof(cmdline), "cmd.exe /c \"\"%s\"\"", script_path);
    if(!windows_spawn_detached(cmdline)) {
        remove(script_path);
        return KRY_UPDATE_APPLY_FAILED;
    }
    return KRY_UPDATE_APPLY_RESTARTING;
}

#else /* !_WIN32 */

KryUpdateApplyResult
kry_update_windows_stage_swap(const char *new_dir)
{
    (void)new_dir;
    return KRY_UPDATE_APPLY_NOT_APPLICABLE;
}

#endif /* _WIN32 */

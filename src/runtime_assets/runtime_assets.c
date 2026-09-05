#include "runtime_assets.h"
#include "platform.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#if defined(_WIN32)
#include <direct.h>
#include <windows.h>
#include <wininet.h>
#define RUNTIME_ASSET_MKDIR(path) _mkdir(path)
#else
#include <errno.h>
#include <unistd.h>
#define RUNTIME_ASSET_MKDIR(path) mkdir(path, 0755)
#endif

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#include <emscripten/fetch.h>

EM_ASYNC_JS(int, runtime_assets_web_init, (const char *root_ptr), {
    const root = UTF8ToString(root_ptr);

    try {
        FS.mkdirTree('/persistent_data');
        if(!Module.__kryonRuntimeAssetsMounted) {
            FS.mount(IDBFS, {}, '/persistent_data');
            Module.__kryonRuntimeAssetsMounted = true;
            Module.__kryonRuntimeAssetsSync = new Promise((resolve, reject) => {
                FS.syncfs(true, (err) => err ? reject(err) : resolve());
            });
        }
        await Module.__kryonRuntimeAssetsSync;
        FS.mkdirTree(root);
        return 1;
    } catch(e) {
        console.error('kryon runtime asset init failed', e);
        return 0;
    }
});
#endif

#if defined(HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
#include <curl/curl.h>
#endif

#if !defined(__EMSCRIPTEN__) && !defined(_WIN32) && !defined(HAS_LIBCURL) && !ANDROID_BUILD
#error "Kryon runtime assets require Emscripten fetch, Windows WinINet, libcurl, or an Android app backend"
#endif

static RuntimeAssetDownloadBackend g_download_backend = NULL;

typedef struct RuntimeAssetDownloadState {
    KryThread thread;
    KryMutex mutex;
    int started;
    int abandoned;
    RuntimeAssetStatus status;
    char url[1024];
    char path[512];
    char error[256];
    long http_status;
    size_t bytes;
    size_t total_bytes;
} RuntimeAssetDownloadState;

static RuntimeAssetDownloadState *
runtime_asset_state_new(const char *url, const char *path)
{
    RuntimeAssetDownloadState *state = calloc(1, sizeof(*state));

    if(state == NULL)
        return NULL;
    KryMutexInit(&state->mutex);
    state->status = RUNTIME_ASSET_DOWNLOADING;
    snprintf(state->url, sizeof(state->url), "%s", url != NULL ? url : "");
    snprintf(state->path, sizeof(state->path), "%s", path != NULL ? path : "");
    return state;
}

static void
runtime_asset_state_copy(RuntimeAssetDownload *download,
                         const RuntimeAssetDownloadState *state)
{
    if(download == NULL || state == NULL)
        return;
    download->status = state->status;
    snprintf(download->error, sizeof(download->error), "%s", state->error);
    download->http_status = state->http_status;
    download->bytes = state->bytes;
    download->total_bytes = state->total_bytes;
}

static void
runtime_asset_state_progress(RuntimeAssetDownloadState *state, size_t bytes,
                             size_t total_bytes)
{
    if(state == NULL)
        return;
    KryMutexLock(&state->mutex);
    state->bytes = bytes;
    state->total_bytes = total_bytes;
    KryMutexUnlock(&state->mutex);
}

static void
runtime_asset_state_finish(RuntimeAssetDownloadState *state,
                           RuntimeAssetStatus status, const char *error)
{
    if(state == NULL)
        return;
    KryMutexLock(&state->mutex);
    state->status = status;
    if(error != NULL)
        snprintf(state->error, sizeof(state->error), "%s", error);
    KryMutexUnlock(&state->mutex);
}

static int
path_is_dir(const char *path)
{
    struct stat st;
    if(path == NULL || path[0] == '\0')
        return 0;
    return stat(path, &st) == 0 && (st.st_mode & S_IFDIR) != 0;
}

int
EnsureRuntimeAssetDir(const char *path)
{
    char tmp[512];
    char *p;

    if(path == NULL || path[0] == '\0')
        return 0;
    if(path_is_dir(path))
        return 1;

    snprintf(tmp, sizeof(tmp), "%s", path);
    p = tmp;
    if(p[0] == '/')
        p++;

    while((p = strchr(p, '/')) != NULL) {
        *p = '\0';
        if(tmp[0] != '\0' && !path_is_dir(tmp)) {
            if(RUNTIME_ASSET_MKDIR(tmp) != 0 && !path_is_dir(tmp))
                return 0;
        }
        *p = '/';
        p++;
    }

    if(RUNTIME_ASSET_MKDIR(tmp) != 0 && !path_is_dir(tmp))
        return 0;
    return 1;
}

int
GetRuntimeAssetCacheRoot(const char *app_id, char *out, size_t out_size)
{
    const char *id = (app_id != NULL && app_id[0] != '\0') ? app_id : "kryon";

    if(out == NULL || out_size == 0)
        return 0;

#if defined(__EMSCRIPTEN__)
    snprintf(out, out_size, "/persistent_data/%s", id);
#elif defined(_WIN32)
    {
        const char *local = getenv("LOCALAPPDATA");
        if(local != NULL && local[0] != '\0')
            snprintf(out, out_size, "%s/%s/runtime-assets", local, id);
        else
            snprintf(out, out_size, "%s/runtime-assets", id);
    }
#else
    {
        const char *xdg = getenv("XDG_DATA_HOME");
        const char *home = getenv("HOME");
        if(xdg != NULL && xdg[0] != '\0')
            snprintf(out, out_size, "%s/%s/runtime-assets", xdg, id);
        else if(home != NULL && home[0] != '\0')
            snprintf(out, out_size, "%s/.local/share/%s/runtime-assets", home, id);
        else
            snprintf(out, out_size, ".local/%s/runtime-assets", id);
    }
#endif

    return EnsureRuntimeAssetDir(out);
}

int
InitRuntimeAssets(const char *app_id)
{
    char root[512];

#if defined(__EMSCRIPTEN__)
    const char *id = (app_id != NULL && app_id[0] != '\0') ? app_id : "kryon";
    snprintf(root, sizeof(root), "/persistent_data/%s", id);
    return runtime_assets_web_init(root);
#else
    if(!GetRuntimeAssetCacheRoot(app_id, root, sizeof(root)))
        return 0;
#endif

    return 1;
}

int
SyncRuntimeAssets(void)
{
#if defined(__EMSCRIPTEN__)
    EM_ASM({
        if(typeof FS !== 'undefined' && FS.syncfs) {
            FS.syncfs(false, function(err) {
                if(err) console.error('kryon runtime asset save failed', err);
            });
        }
    });
#endif
    return 1;
}

const char *
GetRuntimeAssetStatusText(RuntimeAssetStatus status)
{
    switch(status) {
    case RUNTIME_ASSET_IDLE: return "idle";
    case RUNTIME_ASSET_DOWNLOADING: return "downloading";
    case RUNTIME_ASSET_READY: return "ready";
    case RUNTIME_ASSET_ERROR: return "error";
    default: return "unknown";
    }
}

void
SetRuntimeAssetDownloadBackend(RuntimeAssetDownloadBackend backend)
{
    g_download_backend = backend;
}

#if defined(__EMSCRIPTEN__)
static void
fetch_done(emscripten_fetch_t *fetch)
{
    RuntimeAssetDownloadState *state = (RuntimeAssetDownloadState *)fetch->userData;
    FILE *file;

    if(state == NULL) {
        emscripten_fetch_close(fetch);
        return;
    }

    KryMutexLock(&state->mutex);
    state->http_status = fetch->status;
    state->bytes = (size_t)fetch->numBytes;
    KryMutexUnlock(&state->mutex);
    file = fopen(state->path, "wb");
    if(file == NULL) {
        char msg[320];

        snprintf(msg, sizeof(msg), "failed to open %s", state->path);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
        emscripten_fetch_close(fetch);
        KryMutexLock(&state->mutex);
        if(state->abandoned) {
            KryMutexUnlock(&state->mutex);
            free(state);
        } else {
            KryMutexUnlock(&state->mutex);
        }
        return;
    }

    if(fwrite(fetch->data, 1, (size_t)fetch->numBytes, file) != (size_t)fetch->numBytes) {
        char msg[320];

        snprintf(msg, sizeof(msg), "failed to write %s", state->path);
        fclose(file);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
        emscripten_fetch_close(fetch);
        KryMutexLock(&state->mutex);
        if(state->abandoned) {
            KryMutexUnlock(&state->mutex);
            free(state);
        } else {
            KryMutexUnlock(&state->mutex);
        }
        return;
    }

    fclose(file);
    runtime_asset_state_finish(state, RUNTIME_ASSET_READY, NULL);
    SyncRuntimeAssets();
    emscripten_fetch_close(fetch);
    KryMutexLock(&state->mutex);
    if(state->abandoned) {
        KryMutexUnlock(&state->mutex);
        free(state);
    } else {
        KryMutexUnlock(&state->mutex);
    }
}

static void
fetch_failed(emscripten_fetch_t *fetch)
{
    RuntimeAssetDownloadState *state = (RuntimeAssetDownloadState *)fetch->userData;

    if(state != NULL) {
        char msg[64];

        KryMutexLock(&state->mutex);
        state->http_status = fetch->status;
        KryMutexUnlock(&state->mutex);
        snprintf(msg, sizeof(msg), "HTTP %d", fetch->status);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
    }
    emscripten_fetch_close(fetch);
    if(state != NULL) {
        KryMutexLock(&state->mutex);
        if(state->abandoned) {
            KryMutexUnlock(&state->mutex);
            free(state);
        } else {
            KryMutexUnlock(&state->mutex);
        }
    }
}
#endif

#if defined(_WIN32) && !defined(__EMSCRIPTEN__)
static void
windows_set_last_error(RuntimeAssetDownloadState *state, const char *prefix)
{
    DWORD err = GetLastError();
    char msg[256];

    if(state == NULL)
        return;
    snprintf(msg, sizeof(msg), "%s (%lu)",
             prefix != NULL ? prefix : "Windows download failed",
             (unsigned long)err);
    runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
}

static void *
windows_download_thread_main(void *user_data)
{
    RuntimeAssetDownloadState *state = (RuntimeAssetDownloadState *)user_data;
    HINTERNET internet = NULL;
    HINTERNET request = NULL;
    FILE *file = NULL;
    char status_buf[32];
    DWORD status_len = sizeof(status_buf);
    DWORD status_index = 0;
    char length_buf[64];
    DWORD length_len = sizeof(length_buf);
    DWORD length_index = 0;
    BYTE buffer[32768];
    DWORD bytes_read = 0;
    BOOL read_ok;

    if(state == NULL)
        return NULL;

    file = fopen(state->path, "wb");
    if(file == NULL) {
        char msg[256];

        snprintf(msg, sizeof(msg), "failed to open %.200s", state->path);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
        return NULL;
    }

    internet = InternetOpenA("kryon-runtime-assets/1",
                             INTERNET_OPEN_TYPE_PRECONFIG,
                             NULL, NULL, 0);
    if(internet == NULL) {
        fclose(file);
        windows_set_last_error(state, "failed to initialize Windows networking");
        return NULL;
    }

    request = InternetOpenUrlA(internet, state->url, NULL, 0,
                               INTERNET_FLAG_RELOAD |
                               INTERNET_FLAG_NO_CACHE_WRITE |
                               INTERNET_FLAG_PRAGMA_NOCACHE,
                               0);
    if(request == NULL) {
        fclose(file);
        InternetCloseHandle(internet);
        remove(state->path);
        windows_set_last_error(state, "Windows download failed");
        return NULL;
    }

    if(HttpQueryInfoA(request, HTTP_QUERY_STATUS_CODE, status_buf, &status_len, &status_index)) {
        status_buf[sizeof(status_buf) - 1] = '\0';
        KryMutexLock(&state->mutex);
        state->http_status = strtol(status_buf, NULL, 10);
        KryMutexUnlock(&state->mutex);
    }
    if(state->http_status < 200 || state->http_status >= 300) {
        char msg[64];

        snprintf(msg, sizeof(msg), "HTTP %ld", state->http_status);
        fclose(file);
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        remove(state->path);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
        return NULL;
    }

    if(HttpQueryInfoA(request, HTTP_QUERY_CONTENT_LENGTH, length_buf, &length_len, &length_index)) {
        length_buf[sizeof(length_buf) - 1] = '\0';
        KryMutexLock(&state->mutex);
        state->total_bytes = (size_t)strtoull(length_buf, NULL, 10);
        KryMutexUnlock(&state->mutex);
    }

    while((read_ok = InternetReadFile(request, buffer, sizeof(buffer), &bytes_read)) && bytes_read > 0) {
        if(fwrite(buffer, 1, bytes_read, file) != bytes_read) {
            char msg[256];

            snprintf(msg, sizeof(msg), "failed to write %.200s", state->path);
            fclose(file);
            InternetCloseHandle(request);
            InternetCloseHandle(internet);
            remove(state->path);
            runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
            return NULL;
        }
        KryMutexLock(&state->mutex);
        state->bytes += (size_t)bytes_read;
        KryMutexUnlock(&state->mutex);
    }

    if(!read_ok) {
        fclose(file);
        InternetCloseHandle(request);
        InternetCloseHandle(internet);
        remove(state->path);
        windows_set_last_error(state, "Windows download failed");
        return NULL;
    }

    fclose(file);
    InternetCloseHandle(request);
    InternetCloseHandle(internet);
    KryMutexLock(&state->mutex);
    if(state->total_bytes == 0)
        state->total_bytes = state->bytes;
    KryMutexUnlock(&state->mutex);
    runtime_asset_state_finish(state, RUNTIME_ASSET_READY, NULL);
    return NULL;
}
#endif

#if defined(HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
static size_t
curl_write_file(void *ptr, size_t size, size_t nmemb, void *stream)
{
    return fwrite(ptr, size, nmemb, (FILE *)stream);
}

static int
curl_progress(void *clientp, curl_off_t dltotal, curl_off_t dlnow,
              curl_off_t ultotal, curl_off_t ulnow)
{
    RuntimeAssetDownloadState *state = (RuntimeAssetDownloadState *)clientp;
    (void)ultotal;
    (void)ulnow;

    if(state == NULL)
        return 0;

    runtime_asset_state_progress(state, dlnow > 0 ? (size_t)dlnow : 0,
                                 dltotal > 0 ? (size_t)dltotal : 0);
    return 0;
}

static void *
curl_thread_main(void *user_data)
{
    RuntimeAssetDownloadState *state = (RuntimeAssetDownloadState *)user_data;
    CURL *curl;
    FILE *file;
    CURLcode res;
    curl_off_t downloaded = 0;

    if(state == NULL)
        return NULL;

    file = fopen(state->path, "wb");
    if(file == NULL) {
        char msg[256];

        snprintf(msg, sizeof(msg), "failed to open %.200s", state->path);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR, msg);
        return NULL;
    }

    curl = curl_easy_init();
    if(curl == NULL) {
        fclose(file);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR,
                                   "failed to initialize curl");
        return NULL;
    }

    curl_easy_setopt(curl, CURLOPT_URL, state->url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_file);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kryon-runtime-assets/1");
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, curl_progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, state);

    res = curl_easy_perform(curl);
    KryMutexLock(&state->mutex);
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &state->http_status);
    KryMutexUnlock(&state->mutex);
    if(curl_easy_getinfo(curl, CURLINFO_SIZE_DOWNLOAD_T, &downloaded) == CURLE_OK &&
       downloaded > 0)
        runtime_asset_state_progress(state, (size_t)downloaded,
                                     state->total_bytes);
    KryMutexLock(&state->mutex);
    if(state->total_bytes == 0 && downloaded > 0)
        state->total_bytes = (size_t)downloaded;
    KryMutexUnlock(&state->mutex);
    curl_easy_cleanup(curl);
    fclose(file);

    if(res != CURLE_OK) {
        remove(state->path);
        runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR,
                                   curl_easy_strerror(res));
        return NULL;
    }

    runtime_asset_state_finish(state, RUNTIME_ASSET_READY, NULL);
    return NULL;
}
#endif

int
DownloadRuntimeAsset(RuntimeAssetDownload *download,
                             const char *url,
                             const char *path)
{
    char dir[512];
    char *slash;

    if(download == NULL || url == NULL || url[0] == '\0' || path == NULL || path[0] == '\0')
        return 0;
    if(download->platform != NULL) {
        if(PollRuntimeAssetDownload(download) == RUNTIME_ASSET_DOWNLOADING)
            return 0;
        FreeRuntimeAssetDownload(download);
    } else if(download->status == RUNTIME_ASSET_DOWNLOADING) {
        return 0;
    }

    memset(download, 0, sizeof(*download));
    snprintf(download->url, sizeof(download->url), "%s", url);
    snprintf(download->path, sizeof(download->path), "%s", path);

    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if(slash != NULL) {
        *slash = '\0';
        if(!EnsureRuntimeAssetDir(dir)) {
            snprintf(download->error, sizeof(download->error), "failed to create %.200s", dir);
            download->status = RUNTIME_ASSET_ERROR;
            return 0;
        }
    }

    download->status = RUNTIME_ASSET_DOWNLOADING;

    if(g_download_backend != NULL) {
        if(g_download_backend(download, download->url, download->path))
            return 1;
        if(download->status == RUNTIME_ASSET_DOWNLOADING) {
            snprintf(download->error, sizeof(download->error), "runtime asset backend failed");
            download->status = RUNTIME_ASSET_ERROR;
        }
        return 0;
    }

#if defined(__EMSCRIPTEN__)
    {
        emscripten_fetch_attr_t attr;
        RuntimeAssetDownloadState *state = runtime_asset_state_new(url, path);
        if(state == NULL) {
            snprintf(download->error, sizeof(download->error), "out of memory");
            download->status = RUNTIME_ASSET_ERROR;
            return 0;
        }
        download->platform = state;
        emscripten_fetch_attr_init(&attr);
        strcpy(attr.requestMethod, "GET");
        attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY;
        attr.userData = state;
        attr.onsuccess = fetch_done;
        attr.onerror = fetch_failed;
        emscripten_fetch(&attr, state->url);
        return 1;
    }
#elif defined(_WIN32)
    {
        RuntimeAssetDownloadState *state = runtime_asset_state_new(url, path);
        if(state == NULL) {
            snprintf(download->error, sizeof(download->error), "out of memory");
            download->status = RUNTIME_ASSET_ERROR;
            return 0;
        }
        download->platform = state;
        if(!KryThreadStart(&state->thread, windows_download_thread_main, state)) {
            download->platform = NULL;
            runtime_asset_state_finish(state, RUNTIME_ASSET_ERROR,
                                       "failed to create download thread");
            runtime_asset_state_copy(download, state);
            free(state);
            return 0;
        }
        state->started = 1;
        return 1;
    }
#elif defined(HAS_LIBCURL)
    {
        RuntimeAssetDownloadState *state = runtime_asset_state_new(url, path);
        if(state == NULL) {
            snprintf(download->error, sizeof(download->error), "out of memory");
            download->status = RUNTIME_ASSET_ERROR;
            return 0;
        }
        download->platform = state;
        if(!KryThreadStart(&state->thread, curl_thread_main, state)) {
            download->platform = NULL;
            snprintf(download->error, sizeof(download->error), "failed to create download thread");
            download->status = RUNTIME_ASSET_ERROR;
            free(state);
            return 0;
        }
        state->started = 1;
        return 1;
    }
#else
    snprintf(download->error, sizeof(download->error), "runtime asset download backend is not initialized");
    download->status = RUNTIME_ASSET_ERROR;
    return 0;
#endif
}

RuntimeAssetStatus
PollRuntimeAssetDownload(RuntimeAssetDownload *download)
{
    RuntimeAssetDownloadState *state;

    if(download == NULL)
        return RUNTIME_ASSET_ERROR;
    state = (RuntimeAssetDownloadState *)download->platform;
    if(state == NULL)
        return download->status;
    KryMutexLock(&state->mutex);
    runtime_asset_state_copy(download, state);
    KryMutexUnlock(&state->mutex);
    return download->status;
}

void
FreeRuntimeAssetDownload(RuntimeAssetDownload *download)
{
    RuntimeAssetDownloadState *state;
    RuntimeAssetStatus status;

    if(download == NULL)
        return;
    state = (RuntimeAssetDownloadState *)download->platform;
    if(state == NULL)
        return;
#if defined(__EMSCRIPTEN__)
    status = PollRuntimeAssetDownload(download);
    if(status == RUNTIME_ASSET_DOWNLOADING) {
        KryMutexLock(&state->mutex);
        state->abandoned = 1;
        KryMutexUnlock(&state->mutex);
        download->platform = NULL;
        return;
    }
#else
    if(state->started)
        KryThreadJoin(&state->thread);
    status = PollRuntimeAssetDownload(download);
    (void)status;
#endif
    download->platform = NULL;
    free(state);
}

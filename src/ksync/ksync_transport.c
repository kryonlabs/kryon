#if defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#define NOGDI
#define NOUSER
#define MMNOSOUND
#define NOMINMAX
#include <windows.h>
#endif

#include "ksync_sync.h"
#include "platform.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#if ANDROID_BUILD
#include <android_native_app_glue.h>
#include <jni.h>
extern struct android_app *GetAndroidApp(void);
#elif defined(__EMSCRIPTEN__)
#include <emscripten.h>
#else
#include <curl/curl.h>
#if LIBCURL_VERSION_NUM < 0x075600
#error "Kryon Ksync transport requires libcurl 7.86.0 or newer with websocket support"
#endif
#endif

#define KSYNC_SYNC_PATH "/api/v1/sync"
#define KSYNC_CHALLENGE_PATH "/api/v1/sync/challenge"
#define KSYNC_LOGIN_PATH "/api/v1/sync/login"
#define KSYNC_SIGNATURE_CONTEXT "ksync-sync-v1"
#define KSYNC_USER_HEADER "X-Ksync-User"
#define KSYNC_SIGNATURE_HEADER "X-Ksync-Signature"
#define KSYNC_WEB_RESPONSE_MAX (4 * 1024 * 1024)

static const char *
transport_signature_context(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->signature_context != NULL &&
                   cfg->signature_context[0] != '\0'
               ? cfg->signature_context
               : KSYNC_SIGNATURE_CONTEXT;
}

static const char *
transport_user_header_name(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->user_header_name != NULL &&
                   cfg->user_header_name[0] != '\0'
               ? cfg->user_header_name
               : KSYNC_USER_HEADER;
}

static const char *
transport_signature_header_name(const KsyncSyncConfig *cfg)
{
    return cfg != NULL && cfg->signature_header_name != NULL &&
                   cfg->signature_header_name[0] != '\0'
               ? cfg->signature_header_name
               : KSYNC_SIGNATURE_HEADER;
}

static int
transport_build_message(const KsyncSyncConfig *cfg, const char *method,
                        const char *path, const char *nonce_hex,
                        const char *body, char *out, size_t out_size)
{
    char body_hash[KSYNC_PUBLIC_ID_HEX_SIZE];
    int len;

    if(method == NULL || path == NULL || nonce_hex == NULL || body == NULL ||
       out == NULL || out_size == 0)
        return 0;
    KsyncSha256Hex((const uint8_t *)body, strlen(body), body_hash);
    if(body_hash[0] == '\0')
        return 0;
    len = snprintf(out, out_size, "%s\n%s\n%s\n%s\n%s\n",
                   transport_signature_context(cfg), method, path, body_hash,
                   nonce_hex);
    return len > 0 && (size_t)len < out_size;
}

#if !ANDROID_BUILD && !defined(__EMSCRIPTEN__)
static size_t
transport_write_callback(void *ptr, size_t size, size_t nmemb, void *userdata)
{
    KsyncSyncBuffer *buffer = (KsyncSyncBuffer *)userdata;
    size_t bytes = size * nmemb;

    if(buffer == NULL || bytes == 0)
        return bytes;
    return AppendKsyncSyncBuffer(buffer, ptr, bytes) ? bytes : 0;
}

int
KsyncDefaultHttpRequest(const char *method, const char *url, const char *body,
                        const char *const *headers, int header_count,
                        KsyncSyncBuffer *response, long *status, void *user)
{
    CURL *curl;
    CURLcode res;
    struct curl_slist *curl_headers = NULL;

    (void)user;
    if(status != NULL)
        *status = 0;
    if(method == NULL || url == NULL)
        return 0;
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl == NULL)
        return 0;
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kryon-ksync/1");
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, transport_write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, response);
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL)
            curl_headers = curl_slist_append(curl_headers, headers[i]);
    }
    if(curl_headers != NULL)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    if(strcmp(method, "POST") == 0) {
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         (long)(body != NULL ? strlen(body) : 0));
    } else if(strcmp(method, "DELETE") == 0) {
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "DELETE");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body != NULL ? body : "");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                         (long)(body != NULL ? strlen(body) : 0));
    }
    res = curl_easy_perform(curl);
    if(status != NULL)
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, status);
    curl_slist_free_all(curl_headers);
    curl_easy_cleanup(curl);
    return res == CURLE_OK;
}
#elif ANDROID_BUILD
static int
transport_buffer_set(KsyncSyncBuffer *response, const char *text)
{
    size_t len;

    if(response == NULL)
        return 0;
    len = text != NULL ? strlen(text) : 0;
    response->data = (char *)malloc(len + 1);
    if(response->data == NULL)
        return 0;
    if(len > 0)
        memcpy(response->data, text, len);
    response->data[len] = '\0';
    response->len = len;
    response->cap = len + 1;
    return 1;
}

static int
transport_android_call(const char *java_method, const char *url,
                       const char *method, const char *body,
                       const char *const *headers, int header_count,
                       KsyncSyncBuffer *response, long *status)
{
    struct android_app *app = GetAndroidApp();
    JavaVM *jvm;
    JNIEnv *env = NULL;
    jobject activity;
    jclass activity_class;
    jclass string_class = NULL;
    jmethodID method_id;
    jstring jmethod = NULL;
    jstring jurl = NULL;
    jstring jbody = NULL;
    jobjectArray jheaders = NULL;
    jstring result = NULL;
    const char *result_text = NULL;
    const char *newline;
    int attached = 0;
    int ok = 0;

    if(status != NULL)
        *status = 0;
    if(app == NULL || app->activity == NULL || app->activity->vm == NULL ||
       app->activity->clazz == NULL || java_method == NULL || url == NULL)
        return 0;

    jvm = app->activity->vm;
    activity = app->activity->clazz;
    if((*jvm)->GetEnv(jvm, (void **)&env, JNI_VERSION_1_6) != JNI_OK) {
        if((*jvm)->AttachCurrentThread(jvm, &env, NULL) != JNI_OK || env == NULL)
            return 0;
        attached = 1;
    }

    activity_class = (*env)->GetObjectClass(env, activity);
    if(activity_class == NULL)
        goto done;
    if(method != NULL) {
        method_id = (*env)->GetMethodID(env, activity_class, java_method,
                                        "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
    } else {
        method_id = (*env)->GetMethodID(env, activity_class, java_method,
                                        "(Ljava/lang/String;[Ljava/lang/String;)Ljava/lang/String;");
    }
    if(method_id == NULL)
        goto done;

    jurl = (*env)->NewStringUTF(env, url);
    string_class = (*env)->FindClass(env, "java/lang/String");
    if(jurl == NULL || string_class == NULL)
        goto done;
    if(method != NULL) {
        jmethod = (*env)->NewStringUTF(env, method);
        jbody = (*env)->NewStringUTF(env, body != NULL ? body : "");
        if(jmethod == NULL || jbody == NULL)
            goto done;
    }
    jheaders = (*env)->NewObjectArray(env, header_count, string_class, NULL);
    if(jheaders == NULL)
        goto done;
    for(int i = 0; i < header_count; i++) {
        jstring value = (*env)->NewStringUTF(env, headers[i] != NULL ? headers[i] : "");
        if(value == NULL)
            goto done;
        (*env)->SetObjectArrayElement(env, jheaders, i, value);
        (*env)->DeleteLocalRef(env, value);
    }

    if(method != NULL) {
        result = (jstring)(*env)->CallObjectMethod(env, activity, method_id,
                                                  jmethod, jurl, jbody, jheaders);
    } else {
        result = (jstring)(*env)->CallObjectMethod(env, activity, method_id,
                                                  jurl, jheaders);
    }
    if((*env)->ExceptionCheck(env) || result == NULL)
        goto done;
    result_text = (*env)->GetStringUTFChars(env, result, NULL);
    if(result_text == NULL)
        goto done;
    newline = strchr(result_text, '\n');
    if(newline == NULL)
        goto done;
    if(status != NULL)
        *status = strtol(result_text, NULL, 10);
    ok = transport_buffer_set(response, newline + 1);

done:
    if(result_text != NULL && result != NULL)
        (*env)->ReleaseStringUTFChars(env, result, result_text);
    if(result != NULL)
        (*env)->DeleteLocalRef(env, result);
    if(jheaders != NULL)
        (*env)->DeleteLocalRef(env, jheaders);
    if(jbody != NULL)
        (*env)->DeleteLocalRef(env, jbody);
    if(jurl != NULL)
        (*env)->DeleteLocalRef(env, jurl);
    if(jmethod != NULL)
        (*env)->DeleteLocalRef(env, jmethod);
    if((*env)->ExceptionCheck(env)) {
        (*env)->ExceptionDescribe(env);
        (*env)->ExceptionClear(env);
    }
    if(attached)
        (*jvm)->DetachCurrentThread(jvm);
    return ok;
}

int
KsyncDefaultHttpRequest(const char *method, const char *url, const char *body,
                        const char *const *headers, int header_count,
                        KsyncSyncBuffer *response, long *status, void *user)
{
    (void)user;
    return transport_android_call("syncHttpRequest", url, method, body, headers,
                                  header_count, response, status);
}
#elif defined(__EMSCRIPTEN__)
EM_ASYNC_JS(int, ksync_web_http_request_js,
            (const char *method_ptr, const char *url_ptr, const char *body_ptr,
             const char *headers_ptr, char *response_ptr, int response_size,
             long *status_ptr), {
    const method = UTF8ToString(method_ptr);
    const url = UTF8ToString(url_ptr);
    const body = body_ptr ? UTF8ToString(body_ptr) : "";
    const headerLines = headers_ptr ? UTF8ToString(headers_ptr) : "";
    const headers = {};

    for(const line of headerLines.split("\n")) {
        if(!line) continue;
        const colon = line.indexOf(":");
        if(colon <= 0) continue;
        headers[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
    }

    try {
        const response = await fetch(url, {
            method,
            headers,
            body: method === "GET" ? undefined : body,
            credentials: "omit",
            redirect: "manual"
        });
        const text = await response.text();
        setValue(status_ptr, response.status, "i32");
        stringToUTF8(text, response_ptr, response_size);
        return 1;
    } catch(e) {
        console.error("Ksync HTTP failed:", e);
        setValue(status_ptr, 0, "i32");
        stringToUTF8("", response_ptr, response_size);
        return 0;
    }
});

EM_JS(int, ksync_web_fetch_start_js,
      (int request_id, const char *method_ptr, const char *url_ptr,
       const char *body_ptr, const char *headers_ptr), {
    const method = UTF8ToString(method_ptr);
    const url = UTF8ToString(url_ptr);
    const body = body_ptr ? UTF8ToString(body_ptr) : "";
    const headerLines = headers_ptr ? UTF8ToString(headers_ptr) : "";
    const headers = {};

    if(!Module.__ksyncFetches)
        Module.__ksyncFetches = {};
    Module.__ksyncFetches[request_id] = {state: 0, status: 0, text: ""};

    for(const line of headerLines.split("\n")) {
        if(!line) continue;
        const colon = line.indexOf(":");
        if(colon <= 0) continue;
        headers[line.slice(0, colon).trim()] = line.slice(colon + 1).trim();
    }

    fetch(url, {
        method,
        headers,
        body: method === "GET" ? undefined : body,
        credentials: "omit",
        redirect: "manual"
    }).then(async response => {
        const text = await response.text();
        Module.__ksyncFetches[request_id] = {state: 1, status: response.status, text};
    }).catch(error => {
        console.error("Ksync HTTP failed:", error);
        Module.__ksyncFetches[request_id] = {state: 2, status: 0, text: ""};
    });
    return 1;
});

EM_JS(int, ksync_web_fetch_poll_js,
      (int request_id, char *response_ptr, int response_size, long *status_ptr), {
    const requests = Module.__ksyncFetches || {};
    const request = requests[request_id];
    if(!request)
        return 2;
    if(request.state === 0)
        return 0;
    setValue(status_ptr, request.status || 0, "i32");
    stringToUTF8(request.text || "", response_ptr, response_size);
    delete requests[request_id];
    return request.state === 1 ? 1 : 2;
});

EM_JS(int, ksync_websocket_start_js, (const char *url_ptr, const char *token_ptr), {
    const url = UTF8ToString(url_ptr);
    const token = UTF8ToString(token_ptr);
    const now = Date.now();
    function scheduleRetry() {
        const failures = Math.min((Module.__ksyncWebSocketFailures || 0) + 1, 8);
        Module.__ksyncWebSocketFailures = failures;
        Module.__ksyncWebSocketRetryAt = Date.now() +
            Math.min(300000, 2000 * Math.pow(2, failures - 1));
    }
    function logSocketError(event) {
        const logAt = Module.__ksyncWebSocketLogAt || 0;
        if(Date.now() < logAt)
            return;
        Module.__ksyncWebSocketLogAt = Date.now() + 60000;
        console.warn("Ksync WebSocket unavailable; remote sync events will retry.", event);
    }
    if(Module.__ksyncWebSocket &&
       Module.__ksyncWebSocketUrl === url &&
       (Module.__ksyncWebSocket.readyState === WebSocket.OPEN ||
        Module.__ksyncWebSocket.readyState === WebSocket.CONNECTING))
        return 1;
    if(Module.__ksyncWebSocketRetryAt && now < Module.__ksyncWebSocketRetryAt)
        return 1;
    if(Module.__ksyncWebSocket) {
        try { Module.__ksyncWebSocket.close(); } catch(e) {}
        Module.__ksyncWebSocket = null;
    }
    Module.__ksyncWebSocketUrl = url;
    try {
        const ws = new WebSocket(url, ["ksync-sync-v1", "bearer." + token]);
        Module.__ksyncWebSocket = ws;
        ws.onopen = function() {
            Module.__ksyncWebSocketRetryAt = 0;
            Module.__ksyncWebSocketFailures = 0;
            Module.__ksyncWebSocketLogAt = 0;
            console.info("Ksync WebSocket connected");
        };
        ws.onmessage = function(event) {
            try {
                const message = JSON.parse(String(event.data || ""));
                if(message.type === "sync_ready" || message.type === "sync_changed")
                    Module.__ksyncWebSocketEvent = 1;
            } catch(e) {
                if(String(event.data || "").indexOf("sync_changed") >= 0)
                    Module.__ksyncWebSocketEvent = 1;
            }
        };
        ws.onclose = function(event) {
            if(Module.__ksyncWebSocket === ws)
                Module.__ksyncWebSocket = null;
            scheduleRetry();
            if(ws.__ksyncHadError)
                logSocketError(event);
        };
        ws.onerror = function(event) {
            ws.__ksyncHadError = true;
        };
        return 1;
    } catch(e) {
        scheduleRetry();
        logSocketError(e);
        return 0;
    }
});

EM_JS(int, ksync_websocket_poll_js, (void), {
    const event = Module.__ksyncWebSocketEvent ? 1 : 0;
    Module.__ksyncWebSocketEvent = 0;
    return event;
});

int
KsyncDefaultHttpRequest(const char *method, const char *url, const char *body,
                        const char *const *headers, int header_count,
                        KsyncSyncBuffer *response, long *status, void *user)
{
    KsyncSyncBuffer header_blob = {0};
    char *response_text;
    int ok;

    (void)user;
    if(status != NULL)
        *status = 0;
    if(response == NULL || method == NULL || url == NULL)
        return 0;
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL &&
           (!AppendKsyncSyncBuffer(&header_blob, headers[i], strlen(headers[i])) ||
            !AppendKsyncSyncBuffer(&header_blob, "\n", 1))) {
            FreeKsyncSyncBuffer(&header_blob);
            return 0;
        }
    }

    response_text = (char *)calloc(1, KSYNC_WEB_RESPONSE_MAX);
    if(response_text == NULL) {
        FreeKsyncSyncBuffer(&header_blob);
        return 0;
    }
    ok = ksync_web_http_request_js(method, url, body != NULL ? body : "",
                                   header_blob.data != NULL ? header_blob.data : "",
                                   response_text, KSYNC_WEB_RESPONSE_MAX, status);
    FreeKsyncSyncBuffer(&header_blob);
    if(!ok) {
        free(response_text);
        return 0;
    }
    response->data = response_text;
    response->len = strlen(response_text);
    response->cap = KSYNC_WEB_RESPONSE_MAX;
    return 1;
}
#endif

static int
transport_load_valid_auth_token(const KsyncSyncConfig *cfg, char *out, size_t out_size)
{
    const char *token;
    const char *expires_text;
    char token_copy[4096];
    long long expires_at;

    if(cfg == NULL || cfg->get_text == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    token = cfg->get_text("sync_auth_token", cfg->user);
    snprintf(token_copy, sizeof(token_copy), "%s", token != NULL ? token : "");
    expires_text = cfg->get_text("sync_auth_token_expires_at", cfg->user);
    expires_at = expires_text != NULL ? atoll(expires_text) : 0;
    if(token_copy[0] == '\0' || expires_at <= (long long)time(NULL))
        return 0;
    snprintf(out, out_size, "%s", token_copy);
    return out[0] != '\0';
}

KsyncSyncResult
KsyncRemoteEventWait(const KsyncSyncConfig *cfg, const char *path)
{
#if ANDROID_BUILD
    char token[4096];
    char ws_url[6000];
    char auth_header[4200];
    const char *headers[1];
    KsyncSyncBuffer response = {0};
    long status = 0;

    if(cfg == NULL || !IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
    if(!transport_load_valid_auth_token(cfg, token, sizeof(token)))
        return KSYNC_SYNC_AUTH_FAILED;
    if(!JoinKsyncSyncWebSocketURL(ws_url, sizeof(ws_url), cfg->base_url, path))
        return KSYNC_SYNC_INVALID_URL;
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers[0] = auth_header;
    if(!transport_android_call("syncWebSocketWait", ws_url, NULL, NULL, headers, 1,
                               &response, &status)) {
        if(status == 401)
            ClearKsyncSyncAuthToken(cfg);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(status != 101) {
        if(status == 401)
            ClearKsyncSyncAuthToken(cfg);
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    if(response.data != NULL && strstr(response.data, "\"type\":\"sync_changed\"") != NULL) {
        FreeKsyncSyncBuffer(&response);
        return KSYNC_SYNC_OK;
    }
    FreeKsyncSyncBuffer(&response);
    return KSYNC_SYNC_REQUEST_FAILED;
#elif !defined(__EMSCRIPTEN__)
    char token[4096];
    char ws_url[6000];
    CURL *curl;
    struct curl_slist *curl_headers = NULL;
    CURLcode code;
    long status = 0;
    char auth_header[4200];
    char message[2048];
    size_t message_len = 0;

    if(cfg == NULL || !IsKsyncSyncURLValid(cfg->base_url))
        return KSYNC_SYNC_INVALID_URL;
    if(!HasKsyncAccountValues(cfg->account))
        return KSYNC_SYNC_NO_ACCOUNT;
    if(!transport_load_valid_auth_token(cfg, token, sizeof(token)))
        return KSYNC_SYNC_AUTH_FAILED;
    if(!JoinKsyncSyncWebSocketURL(ws_url, sizeof(ws_url), cfg->base_url, path))
        return KSYNC_SYNC_INVALID_URL;
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);

    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();
    if(curl == NULL)
        return KSYNC_SYNC_REQUEST_FAILED;
    curl_headers = curl_slist_append(curl_headers, auth_header);
    curl_easy_setopt(curl, CURLOPT_URL, ws_url);
    curl_easy_setopt(curl, CURLOPT_CONNECT_ONLY, 2L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kryon-ksync/1");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, curl_headers);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 15L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0L);

    code = curl_easy_perform(curl);
    if(code != CURLE_OK) {
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
        if(status == 401)
            ClearKsyncSyncAuthToken(cfg);
        curl_slist_free_all(curl_headers);
        curl_easy_cleanup(curl);
        return KSYNC_SYNC_REQUEST_FAILED;
    }
    curl_slist_free_all(curl_headers);

    for(;;) {
        char buffer[512];
        size_t got = 0;
        const struct curl_ws_frame *meta = NULL;

        code = curl_ws_recv(curl, buffer, sizeof(buffer) - 1, &got, &meta);
        if(code == CURLE_AGAIN) {
#if defined(_WIN32)
            Sleep(100);
#else
            struct timespec delay;
            delay.tv_sec = 0;
            delay.tv_nsec = 100000000L;
            nanosleep(&delay, NULL);
#endif
            continue;
        }
        if(code != CURLE_OK) {
            curl_easy_cleanup(curl);
            return KSYNC_SYNC_REQUEST_FAILED;
        }
        if(meta != NULL && (meta->flags & CURLWS_CLOSE)) {
            curl_easy_cleanup(curl);
            return KSYNC_SYNC_REQUEST_FAILED;
        }
        if(meta != NULL && !(meta->flags & CURLWS_TEXT))
            continue;
        if(got > 0) {
            if(got > sizeof(message) - 1 - message_len)
                got = sizeof(message) - 1 - message_len;
            memcpy(message + message_len, buffer, got);
            message_len += got;
            message[message_len] = '\0';
        }
        if(meta == NULL || meta->bytesleft != 0)
            continue;
        if(strstr(message, "\"type\":\"sync_changed\"") != NULL) {
            curl_easy_cleanup(curl);
            return KSYNC_SYNC_OK;
        }
        message_len = 0;
        message[0] = '\0';
    }
#else
    (void)cfg;
    (void)path;
    return KSYNC_SYNC_REQUEST_FAILED;
#endif
}

#if defined(__EMSCRIPTEN__)
typedef enum KsyncWebSyncState {
    KSYNC_WEB_SYNC_IDLE,
    KSYNC_WEB_SYNC_WAIT_CHALLENGE,
    KSYNC_WEB_SYNC_WAIT_LOGIN,
    KSYNC_WEB_SYNC_WAIT_SYNC
} KsyncWebSyncState;

typedef struct KsyncWebSyncJob {
    KsyncWebSyncState state;
    int request_id;
    KsyncSyncConfig cfg;
    char base_url[512];
    char client_id[128];
    char signature_context[64];
    char user_header_name[64];
    char signature_header_name[64];
    KsyncAccount account;
    KsyncSyncBuffer login_body;
    char *payload;
    char *response_text;
    long status;
    int retried_auth;
} KsyncWebSyncJob;

static KsyncWebSyncJob g_web_sync;
static int g_web_sync_next_request_id = 1;

static void
web_sync_reset(void)
{
    FreeKsyncSyncBuffer(&g_web_sync.login_body);
    if(g_web_sync.payload != NULL && g_web_sync.cfg.free_payload != NULL)
        g_web_sync.cfg.free_payload(g_web_sync.payload, g_web_sync.cfg.user);
    free(g_web_sync.response_text);
    memset(&g_web_sync, 0, sizeof(g_web_sync));
}

static int
web_sync_start_fetch(const char *method, const char *url, const char *body,
                     const char *const *headers, int header_count,
                     KsyncWebSyncState wait_state)
{
    KsyncSyncBuffer header_blob = {0};

    free(g_web_sync.response_text);
    g_web_sync.response_text = NULL;
    g_web_sync.status = 0;
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL &&
           (!AppendKsyncSyncBuffer(&header_blob, headers[i], strlen(headers[i])) ||
            !AppendKsyncSyncBuffer(&header_blob, "\n", 1))) {
            FreeKsyncSyncBuffer(&header_blob);
            return 0;
        }
    }
    g_web_sync.response_text = (char *)calloc(1, KSYNC_WEB_RESPONSE_MAX);
    if(g_web_sync.response_text == NULL) {
        FreeKsyncSyncBuffer(&header_blob);
        return 0;
    }
    g_web_sync.request_id = g_web_sync_next_request_id++;
    if(g_web_sync_next_request_id <= 0)
        g_web_sync_next_request_id = 1;
    g_web_sync.state = wait_state;
    if(!ksync_web_fetch_start_js(g_web_sync.request_id, method, url,
                                 body != NULL ? body : "",
                                 header_blob.data != NULL ? header_blob.data : "")) {
        FreeKsyncSyncBuffer(&header_blob);
        return 0;
    }
    FreeKsyncSyncBuffer(&header_blob);
    return 1;
}

static int
web_sync_poll_fetch(KsyncSyncBuffer *response)
{
    int poll;

    if(response == NULL || g_web_sync.response_text == NULL)
        return 2;
    poll = ksync_web_fetch_poll_js(g_web_sync.request_id,
                                   g_web_sync.response_text,
                                   KSYNC_WEB_RESPONSE_MAX,
                                   &g_web_sync.status);
    if(poll != 1)
        return poll;
    response->data = g_web_sync.response_text;
    response->len = strlen(g_web_sync.response_text);
    response->cap = KSYNC_WEB_RESPONSE_MAX;
    g_web_sync.response_text = NULL;
    return 1;
}

static int
web_sync_start_challenge(void)
{
    char url[768];

    if(!JoinKsyncSyncURL(url, sizeof(url), g_web_sync.cfg.base_url,
                         KSYNC_CHALLENGE_PATH))
        return 0;
    if(strlen(url) + strlen(g_web_sync.account.public_id) + 10 >= sizeof(url))
        return 0;
    strncat(url, "?user_id=", sizeof(url) - strlen(url) - 1);
    strncat(url, g_web_sync.account.public_id, sizeof(url) - strlen(url) - 1);
    return web_sync_start_fetch("GET", url, NULL, NULL, 0,
                                KSYNC_WEB_SYNC_WAIT_CHALLENGE);
}

static int
web_sync_start_login(const char *nonce_hex)
{
    char message[256];
    char signature_hex[KSYNC_SIGNATURE_HEX_SIZE];
    char url[768];
    char user_header[128];
    char signature_header[KSYNC_SIGNATURE_HEX_SIZE + 64];
    const char *headers[3];

    if(!transport_build_message(&g_web_sync.cfg, "POST", KSYNC_LOGIN_PATH,
                                nonce_hex, g_web_sync.login_body.data,
                                message, sizeof(message)))
        return 0;
    if(!SignKsyncAccountHex(&g_web_sync.account, (const uint8_t *)message,
                            strlen(message), signature_hex,
                            sizeof(signature_hex)))
        return 0;
    if(!JoinKsyncSyncURL(url, sizeof(url), g_web_sync.cfg.base_url,
                         KSYNC_LOGIN_PATH))
        return 0;
    snprintf(user_header, sizeof(user_header), "%s: %s",
             transport_user_header_name(&g_web_sync.cfg),
             g_web_sync.account.public_id);
    snprintf(signature_header, sizeof(signature_header), "%s: %s",
             transport_signature_header_name(&g_web_sync.cfg), signature_hex);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = signature_header;
    return web_sync_start_fetch("POST", url, g_web_sync.login_body.data,
                                headers, 3, KSYNC_WEB_SYNC_WAIT_LOGIN);
}

static int
web_sync_start_sync(void)
{
    char token[4096];
    char url[768];
    char user_header[128];
    char auth_header[4200];
    const char *headers[3];

    if(!transport_load_valid_auth_token(&g_web_sync.cfg, token, sizeof(token)))
        return 0;
    if(g_web_sync.payload == NULL) {
        g_web_sync.payload = g_web_sync.cfg.build_payload(
            g_web_sync.account.public_id, g_web_sync.account.public_key_hex,
            g_web_sync.cfg.user);
        if(g_web_sync.payload == NULL)
            return 0;
    }
    if(!JoinKsyncSyncURL(url, sizeof(url), g_web_sync.cfg.base_url,
                         KSYNC_SYNC_PATH))
        return 0;
    snprintf(user_header, sizeof(user_header), "%s: %s",
             transport_user_header_name(&g_web_sync.cfg),
             g_web_sync.account.public_id);
    snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", token);
    headers[0] = "Content-Type: application/json";
    headers[1] = user_header;
    headers[2] = auth_header;
    return web_sync_start_fetch("POST", url, g_web_sync.payload, headers, 3,
                                KSYNC_WEB_SYNC_WAIT_SYNC);
}

static int
web_sync_build_login_body(void)
{
    if(g_web_sync.login_body.data != NULL)
        return 1;
    return AppendKsyncSyncBuffer(&g_web_sync.login_body, "{\"user_id_hash\":",
                                 strlen("{\"user_id_hash\":")) &&
           AppendKsyncSyncBufferJSONString(&g_web_sync.login_body,
                                           g_web_sync.account.public_id) &&
           AppendKsyncSyncBuffer(&g_web_sync.login_body, ",\"client_id\":",
                                 strlen(",\"client_id\":")) &&
           AppendKsyncSyncBufferJSONString(&g_web_sync.login_body,
                                           g_web_sync.cfg.client_id) &&
           AppendKsyncSyncBuffer(&g_web_sync.login_body, ",\"public_key\":",
                                 strlen(",\"public_key\":")) &&
           AppendKsyncSyncBufferJSONString(&g_web_sync.login_body,
                                           g_web_sync.account.public_key_hex) &&
           AppendKsyncSyncBuffer(&g_web_sync.login_body, "}", 1);
}

int
KsyncWebSyncStart(const KsyncSyncConfig *cfg)
{
    char token[4096];

    if(g_web_sync.state != KSYNC_WEB_SYNC_IDLE || cfg == NULL)
        return 0;
    if(!IsKsyncSyncURLValid(cfg->base_url) || !HasKsyncAccountValues(cfg->account) ||
       cfg->get_text == NULL || cfg->set_text == NULL ||
       cfg->build_payload == NULL || cfg->free_payload == NULL ||
       cfg->apply_response == NULL)
        return 0;

    memset(&g_web_sync, 0, sizeof(g_web_sync));
    g_web_sync.cfg = *cfg;
    g_web_sync.account = *cfg->account;
    snprintf(g_web_sync.base_url, sizeof(g_web_sync.base_url), "%s", cfg->base_url);
    snprintf(g_web_sync.client_id, sizeof(g_web_sync.client_id), "%s", cfg->client_id);
    snprintf(g_web_sync.signature_context, sizeof(g_web_sync.signature_context),
             "%s", transport_signature_context(cfg));
    snprintf(g_web_sync.user_header_name, sizeof(g_web_sync.user_header_name),
             "%s", transport_user_header_name(cfg));
    snprintf(g_web_sync.signature_header_name, sizeof(g_web_sync.signature_header_name),
             "%s", transport_signature_header_name(cfg));
    g_web_sync.cfg.base_url = g_web_sync.base_url;
    g_web_sync.cfg.client_id = g_web_sync.client_id;
    g_web_sync.cfg.signature_context = g_web_sync.signature_context;
    g_web_sync.cfg.user_header_name = g_web_sync.user_header_name;
    g_web_sync.cfg.signature_header_name = g_web_sync.signature_header_name;
    g_web_sync.cfg.account = &g_web_sync.account;

    if(transport_load_valid_auth_token(&g_web_sync.cfg, token, sizeof(token)))
        return web_sync_start_sync();
    if(!web_sync_build_login_body()) {
        web_sync_reset();
        return 0;
    }
    if(!web_sync_start_challenge()) {
        web_sync_reset();
        return 0;
    }
    return 1;
}

int
KsyncWebSyncPoll(KsyncSyncResult *result, int *changed)
{
    KsyncSyncBuffer response = {0};
    int poll;

    if(result != NULL)
        *result = KSYNC_SYNC_OK;
    if(changed != NULL)
        *changed = 0;
    if(g_web_sync.state == KSYNC_WEB_SYNC_IDLE)
        return 0;
    poll = web_sync_poll_fetch(&response);
    if(poll == 0)
        return 0;
    if(poll == 2) {
        if(result != NULL)
            *result = KSYNC_SYNC_REQUEST_FAILED;
        web_sync_reset();
        return 1;
    }

    if(g_web_sync.state == KSYNC_WEB_SYNC_WAIT_CHALLENGE) {
        char nonce_hex[65];
        if(g_web_sync.status != 200 ||
           !FindKsyncSyncJSONString(response.data, "nonce", nonce_hex,
                                    sizeof(nonce_hex)) ||
           strlen(nonce_hex) != 64) {
            if(g_web_sync.cfg.log_http_failure != NULL)
                g_web_sync.cfg.log_http_failure("challenge", g_web_sync.status,
                                                response.data, g_web_sync.cfg.user);
            if(result != NULL)
                *result = g_web_sync.status == 401 ? KSYNC_SYNC_AUTH_FAILED
                                                   : KSYNC_SYNC_CHALLENGE_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        FreeKsyncSyncBuffer(&response);
        if(!web_sync_start_login(nonce_hex)) {
            if(result != NULL)
                *result = KSYNC_SYNC_SIGN_FAILED;
            web_sync_reset();
            return 1;
        }
        return 0;
    }

    if(g_web_sync.state == KSYNC_WEB_SYNC_WAIT_LOGIN) {
        char token[4096];
        long long expires_in;
        long long expires_at;

        if(g_web_sync.status == 401) {
            if(g_web_sync.cfg.log_http_failure != NULL)
                g_web_sync.cfg.log_http_failure("login auth", g_web_sync.status,
                                                response.data, g_web_sync.cfg.user);
            if(result != NULL)
                *result = KSYNC_SYNC_AUTH_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        if(g_web_sync.status < 200 || g_web_sync.status >= 300) {
            if(g_web_sync.cfg.log_http_failure != NULL)
                g_web_sync.cfg.log_http_failure("login", g_web_sync.status,
                                                response.data, g_web_sync.cfg.user);
            if(result != NULL)
                *result = KSYNC_SYNC_REQUEST_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        expires_in = FindKsyncSyncJSONInt64(response.data, "expires_in_seconds", 3600);
        if(!FindKsyncSyncJSONString(response.data, "auth_token", token, sizeof(token))) {
            if(result != NULL)
                *result = KSYNC_SYNC_PAYLOAD_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        expires_at = (long long)time(NULL) + expires_in - 30;
        if(expires_at < (long long)time(NULL))
            expires_at = (long long)time(NULL);
        {
            char text[32];
            snprintf(text, sizeof(text), "%lld", expires_at);
            g_web_sync.cfg.set_text("sync_auth_token", token, g_web_sync.cfg.user);
            g_web_sync.cfg.set_text("sync_auth_token_expires_at", text,
                                    g_web_sync.cfg.user);
        }
        FreeKsyncSyncBuffer(&response);
        if(!web_sync_start_sync()) {
            if(result != NULL)
                *result = KSYNC_SYNC_PAYLOAD_FAILED;
            web_sync_reset();
            return 1;
        }
        return 0;
    }

    if(g_web_sync.state == KSYNC_WEB_SYNC_WAIT_SYNC) {
        if(g_web_sync.status == 401) {
            ClearKsyncSyncAuthToken(&g_web_sync.cfg);
            FreeKsyncSyncBuffer(&response);
            if(!g_web_sync.retried_auth) {
                g_web_sync.retried_auth = 1;
                if(web_sync_build_login_body() && web_sync_start_challenge())
                    return 0;
            }
            if(result != NULL)
                *result = KSYNC_SYNC_AUTH_FAILED;
            web_sync_reset();
            return 1;
        }
        if(g_web_sync.status < 200 || g_web_sync.status >= 300) {
            if(g_web_sync.cfg.log_http_failure != NULL)
                g_web_sync.cfg.log_http_failure("sync", g_web_sync.status,
                                                response.data, g_web_sync.cfg.user);
            if(result != NULL)
                *result = KSYNC_SYNC_REQUEST_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        if(!g_web_sync.cfg.apply_response(response.data, g_web_sync.cfg.user)) {
            if(result != NULL)
                *result = KSYNC_SYNC_PAYLOAD_FAILED;
            FreeKsyncSyncBuffer(&response);
            web_sync_reset();
            return 1;
        }
        if(changed != NULL)
            *changed = 1;
        FreeKsyncSyncBuffer(&response);
        if(g_web_sync.cfg.purge_synced_deleted != NULL)
            g_web_sync.cfg.purge_synced_deleted(g_web_sync.cfg.user);
        if(result != NULL)
            *result = KSYNC_SYNC_OK;
        web_sync_reset();
        return 1;
    }

    web_sync_reset();
    if(result != NULL)
        *result = KSYNC_SYNC_REQUEST_FAILED;
    return 1;
}

int
KsyncWebRemoteEventsStart(const KsyncSyncConfig *cfg, const char *path)
{
    char token[4096];
    char ws_url[6000];

    if(cfg == NULL || !IsKsyncSyncURLValid(cfg->base_url))
        return 0;
    if(!HasKsyncAccountValues(cfg->account))
        return 0;
    if(!transport_load_valid_auth_token(cfg, token, sizeof(token)))
        return 0;
    if(!JoinKsyncSyncWebSocketURL(ws_url, sizeof(ws_url), cfg->base_url, path))
        return 0;
    return ksync_websocket_start_js(ws_url, token);
}

int
KsyncWebRemoteEventsPoll(void)
{
    return ksync_websocket_poll_js();
}
#endif

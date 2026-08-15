/*
 * kry_http.c - async HTTP for the Kry standard library (libcurl easy on a
 * worker thread). Without HAS_LIBCURL every entry point degrades to an
 * unavailable stub, mirroring kry_term/kry_process on Windows.
 */
#include "kry_http.h"
#include "platform.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#if defined(HAS_LIBCURL) && !defined(__EMSCRIPTEN__)
#include <curl/curl.h>
#endif

struct KryHttpRequest {
    KryThread thread;
    KryMutex mutex;
    int started;
    int finished;
    /* immutable after construction */
    char *url;
    char *authorization;
    char *body;
    long timeout_s;
    /* written by the worker under mutex */
    KryHttpStatus state;
    int status_code;
    char *response;
};

#if defined(HAS_LIBCURL) && !defined(__EMSCRIPTEN__)

static size_t
write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    KryHttpRequest *r = userdata;
    size_t n = size * nmemb;
    size_t have = r->response != NULL ? strlen(r->response) : 0;
    char *next = realloc(r->response, have + n + 1);

    if(next == NULL)
        return 0;   /* out of memory: curl aborts the transfer */
    r->response = next;
    memcpy(r->response + have, ptr, n);
    r->response[have + n] = '\0';
    return n;
}

static void *
worker(void *userdata)
{
    KryHttpRequest *r = userdata;
    CURL *curl = curl_easy_init();
    struct curl_slist *headers = NULL;
    CURLcode rc;

    if(curl == NULL) {
        KryMutexLock(&r->mutex);
        r->state = KRY_HTTP_FAILED;
        r->response = strdup("curl_easy_init failed");
        r->finished = 1;
        KryMutexUnlock(&r->mutex);
        return NULL;
    }
    if(r->body != NULL) {
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, r->body);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(r->body));
    } else {
        curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    }
    if(r->authorization != NULL && r->authorization[0] != '\0') {
        char auth[1024];

        snprintf(auth, sizeof(auth), "Authorization: Bearer %s",
                 r->authorization);
        headers = curl_slist_append(headers, auth);
    }
    curl_easy_setopt(curl, CURLOPT_URL, r->url);
    if(headers != NULL)
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, r->timeout_s);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, r);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kryon/1");
    rc = curl_easy_perform(curl);
    KryMutexLock(&r->mutex);
    if(rc == CURLE_OK) {
        long code = 0;

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        r->status_code = (int)code;
        /* code 0 = non-HTTP scheme (file://); curl itself already
         * validated the transfer. */
        r->state = code == 0 || (code >= 200 && code < 300)
                     ? KRY_HTTP_DONE : KRY_HTTP_FAILED;
        if(r->state == KRY_HTTP_FAILED && (r->response == NULL ||
                                           r->response[0] == '\0')) {
            char msg[64];

            snprintf(msg, sizeof(msg), "HTTP %ld", code);
            r->response = strdup(msg);
        }
    } else {
        const char *err = curl_easy_strerror(rc);
        char msg[128];

        snprintf(msg, sizeof(msg), "curl error %d: %s", (int)rc,
                 err != NULL ? err : "?");
        r->response = strdup(msg);
        r->state = KRY_HTTP_FAILED;
    }
    r->finished = 1;
    KryMutexUnlock(&r->mutex);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    return NULL;
}

static KryHttpRequest *
request_new(const char *url, const char *authorization, const char *body,
            long timeout_s)
{
    KryHttpRequest *r = calloc(1, sizeof(*r));

    if(r == NULL)
        return NULL;
    r->url = strdup(url);
    if(r->url == NULL) {
        free(r);
        return NULL;
    }
    if(authorization != NULL && authorization[0] != '\0')
        r->authorization = strdup(authorization);
    if(body != NULL)
        r->body = strdup(body);
    r->timeout_s = timeout_s > 0 ? timeout_s : 60;
    r->state = KRY_HTTP_PENDING;
    KryMutexInit(&r->mutex);
    if(!KryThreadStart(&r->thread, worker, r)) {
        free(r->url);
        free(r->authorization);
        free(r->body);
        free(r);
        return NULL;
    }
    r->started = 1;
    return r;
}

KryHttpRequest *
kry_http_post_json(const char *url, const char *authorization,
                   const char *json_body, int timeout_s)
{
    if(url == NULL || json_body == NULL)
        return NULL;
    return request_new(url, authorization, json_body, timeout_s);
}

KryHttpRequest *
kry_http_get(const char *url, int timeout_s)
{
    if(url == NULL)
        return NULL;
    return request_new(url, NULL, NULL, timeout_s);
}

KryHttpStatus
kry_http_poll(KryHttpRequest *r)
{
    KryHttpStatus s;

    if(r == NULL)
        return KRY_HTTP_FAILED;
    KryMutexLock(&r->mutex);
    s = r->finished ? r->state : KRY_HTTP_RUNNING;
    KryMutexUnlock(&r->mutex);
    return s;
}

int
kry_http_status_code(KryHttpRequest *r)
{
    int code;

    if(r == NULL)
        return 0;
    KryMutexLock(&r->mutex);
    code = r->finished ? r->status_code : 0;
    KryMutexUnlock(&r->mutex);
    return code;
}

const char *
kry_http_response(KryHttpRequest *r)
{
    const char *body = NULL;

    if(r == NULL)
        return NULL;
    KryMutexLock(&r->mutex);
    if(r->finished)
        body = r->response != NULL ? r->response : "";
    KryMutexUnlock(&r->mutex);
    return body;
}

void
kry_http_free(KryHttpRequest *r)
{
    if(r == NULL)
        return;
    if(r->started)
        KryThreadJoin(&r->thread);
    free(r->url);
    free(r->authorization);
    free(r->body);
    free(r->response);
    free(r);
}

#else /* no libcurl */

KryHttpRequest *kry_http_post_json(const char *url, const char *authorization,
                                   const char *json_body, int timeout_s)
{
    (void)url; (void)authorization; (void)json_body; (void)timeout_s;
    return NULL;
}

KryHttpRequest *kry_http_get(const char *url, int timeout_s)
{
    (void)url; (void)timeout_s;
    return NULL;
}

KryHttpStatus kry_http_poll(KryHttpRequest *r) { (void)r; return KRY_HTTP_FAILED; }
int kry_http_status_code(KryHttpRequest *r) { (void)r; return 0; }
const char *kry_http_response(const KryHttpRequest *r) { (void)r; return NULL; }
void kry_http_free(KryHttpRequest *r) { (void)r; }

#endif

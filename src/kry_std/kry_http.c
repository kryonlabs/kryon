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
    size_t have;
    char *next;

    KryMutexLock(&r->mutex);
    have = r->response != NULL ? strlen(r->response) : 0;
    next = realloc(r->response, have + n + 1);
    if(next == NULL) {
        KryMutexUnlock(&r->mutex);
        return 0;   /* out of memory: curl aborts the transfer */
    }
    r->response = next;
    memcpy(r->response + have, ptr, n);
    r->response[have + n] = '\0';
    KryMutexUnlock(&r->mutex);
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

/* Streaming peek: copies whatever of the body has arrived so far (up to
 * size-1 bytes) and returns the total available. Callers poll this while
 * kry_http_poll reports RUNNING to consume a transfer incrementally. */
size_t
kry_http_partial(KryHttpRequest *r, char *buf, size_t size)
{
    size_t avail = 0;

    if(r == NULL)
        return 0;
    KryMutexLock(&r->mutex);
    if(r->response != NULL) {
        avail = strlen(r->response);
        if(buf != NULL && size > 0) {
            size_t take = avail < size ? avail : size - 1;

            memcpy(buf, r->response, take);
            buf[take] = '\0';
        }
    }
    KryMutexUnlock(&r->mutex);
    return avail;
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

/* --- streaming download -------------------------------------------------- */

struct KryHttpDownload {
    KryThread thread;
    KryMutex mutex;
    int started;
    int finished;
    /* immutable after construction */
    char *url;
    char *dest_path;
    long timeout_s;
    FILE *file;            /* opened by the constructor, closed by the worker */
    /* written by the worker under mutex */
    KryHttpStatus state;
    char *error;
    unsigned long bytes_done;
    double total;          /* bytes; -1 until known */
    CURL *curl;            /* worker-local, published for download_write_cb */
};

static size_t
download_write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    KryHttpDownload *d = userdata;
    size_t n = size * nmemb;
    size_t wrote;

    wrote = fwrite(ptr, 1, n, d->file);
    KryMutexLock(&d->mutex);
    d->bytes_done += wrote;
    if(d->total < 0.0) {
        curl_off_t len = -1;

        if(curl_easy_getinfo(d->curl, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T,
                             &len) == CURLE_OK && len > 0)
            d->total = (double)len;
    }
    KryMutexUnlock(&d->mutex);
    if(wrote != n)
        return 0;   /* disk full / IO error: curl aborts the transfer */
    return n;
}

static void *
download_worker(void *userdata)
{
    KryHttpDownload *d = userdata;
    CURL *curl = curl_easy_init();
    CURLcode rc;

    KryMutexLock(&d->mutex);
    d->curl = curl;
    KryMutexUnlock(&d->mutex);
    if(curl == NULL) {
        KryMutexLock(&d->mutex);
        d->error = strdup("curl_easy_init failed");
        d->state = KRY_HTTP_FAILED;
        d->finished = 1;
        KryMutexUnlock(&d->mutex);
        fclose(d->file);
        d->file = NULL;
        return NULL;
    }
    curl_easy_setopt(curl, CURLOPT_URL, d->url);
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, d->timeout_s);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, download_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, d);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "kryon/1");
    rc = curl_easy_perform(curl);
    KryMutexLock(&d->mutex);
    if(rc == CURLE_OK) {
        long code = 0;
        int flushed;

        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
        /* code 0 = non-HTTP scheme (file://); curl validated the transfer */
        flushed = fflush(d->file) == 0;
        if(fclose(d->file) != 0)
            flushed = 0;
        d->file = NULL;
        if((code == 0 || (code >= 200 && code < 300)) && flushed) {
            d->state = KRY_HTTP_DONE;
        } else if(code != 0 && (code < 200 || code >= 300)) {
            char msg[64];

            snprintf(msg, sizeof(msg), "HTTP %ld", code);
            d->error = strdup(msg);
            d->state = KRY_HTTP_FAILED;
        } else {
            d->error = strdup("disk write error");
            d->state = KRY_HTTP_FAILED;
        }
    } else {
        const char *err = curl_easy_strerror(rc);
        char msg[128];

        snprintf(msg, sizeof(msg), "curl error %d: %s", (int)rc,
                 err != NULL ? err : "?");
        d->error = strdup(msg);
        d->state = KRY_HTTP_FAILED;
        fclose(d->file);
        d->file = NULL;
    }
    d->finished = 1;
    KryMutexUnlock(&d->mutex);
    curl_easy_cleanup(curl);
    return NULL;
}

KryHttpDownload *
kry_http_download(const char *url, const char *dest_path, int timeout_s)
{
    KryHttpDownload *d;

    if(url == NULL || dest_path == NULL)
        return NULL;
    d = calloc(1, sizeof(*d));
    if(d == NULL)
        return NULL;
    d->url = strdup(url);
    d->dest_path = strdup(dest_path);
    if(d->url == NULL || d->dest_path == NULL) {
        free(d->url);
        free(d->dest_path);
        free(d);
        return NULL;
    }
    d->file = fopen(dest_path, "wb");
    if(d->file == NULL) {
        free(d->url);
        free(d->dest_path);
        free(d);
        return NULL;
    }
    d->timeout_s = timeout_s > 0 ? timeout_s : 600;
    d->state = KRY_HTTP_PENDING;
    d->total = -1.0;
    KryMutexInit(&d->mutex);
    if(!KryThreadStart(&d->thread, download_worker, d)) {
        fclose(d->file);
        free(d->url);
        free(d->dest_path);
        free(d);
        return NULL;
    }
    d->started = 1;
    return d;
}

KryHttpStatus
kry_http_download_poll(KryHttpDownload *d)
{
    KryHttpStatus s;

    if(d == NULL)
        return KRY_HTTP_FAILED;
    KryMutexLock(&d->mutex);
    s = d->finished ? d->state : KRY_HTTP_RUNNING;
    KryMutexUnlock(&d->mutex);
    return s;
}

double
kry_http_download_progress(KryHttpDownload *d)
{
    double fraction;

    if(d == NULL)
        return -1.0;
    KryMutexLock(&d->mutex);
    fraction = d->total > 0.0 && d->bytes_done <= (unsigned long)d->total
                 ? (double)d->bytes_done / d->total
                 : -1.0;
    KryMutexUnlock(&d->mutex);
    return fraction;
}

unsigned long
kry_http_download_bytes(KryHttpDownload *d)
{
    unsigned long bytes;

    if(d == NULL)
        return 0;
    KryMutexLock(&d->mutex);
    bytes = d->bytes_done;
    KryMutexUnlock(&d->mutex);
    return bytes;
}

const char *
kry_http_download_error(KryHttpDownload *d)
{
    const char *error = NULL;

    if(d == NULL)
        return NULL;
    KryMutexLock(&d->mutex);
    if(d->finished && d->state == KRY_HTTP_FAILED)
        error = d->error != NULL ? d->error : "download failed";
    KryMutexUnlock(&d->mutex);
    return error;
}

void
kry_http_download_free(KryHttpDownload *d)
{
    if(d == NULL)
        return;
    if(d->started)
        KryThreadJoin(&d->thread);
    if(d->file != NULL)
        fclose(d->file);
    free(d->url);
    free(d->dest_path);
    free(d->error);
    free(d);
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
const char *kry_http_response(KryHttpRequest *r) { (void)r; return NULL; }
size_t kry_http_partial(KryHttpRequest *r, char *buf, size_t size) { (void)r; (void)buf; (void)size; return 0; }
void kry_http_free(KryHttpRequest *r) { (void)r; }

KryHttpDownload *kry_http_download(const char *url, const char *dest_path,
                                   int timeout_s)
{
    (void)url; (void)dest_path; (void)timeout_s;
    return NULL;
}
KryHttpStatus kry_http_download_poll(KryHttpDownload *d) { (void)d; return KRY_HTTP_FAILED; }
double kry_http_download_progress(KryHttpDownload *d) { (void)d; return -1.0; }
unsigned long kry_http_download_bytes(KryHttpDownload *d) { (void)d; return 0; }
const char *kry_http_download_error(KryHttpDownload *d) { (void)d; return NULL; }
void kry_http_download_free(KryHttpDownload *d) { (void)d; }

#endif

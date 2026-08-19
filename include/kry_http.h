/*
 * kry_http.h - Kry standard library: asynchronous HTTP client.
 *
 * A thin libcurl-easy wrapper (built when HAS_LIBCURL is defined; otherwise
 * every entry point is an unavailable stub returning NULL/KRY_HTTP_FAILED).
 * Requests run on a worker thread so a UI frame loop can poll without
 * blocking — the pattern LLM and other network calls need, where a round
 * trip can take tens of seconds.
 */
#ifndef KRYON_KRY_HTTP_H
#define KRYON_KRY_HTTP_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryHttpRequest KryHttpRequest;

typedef enum {
    KRY_HTTP_PENDING,   /* not started yet */
    KRY_HTTP_RUNNING,   /* worker thread inside libcurl */
    KRY_HTTP_DONE,      /* 2xx response; body ready */
    KRY_HTTP_FAILED,    /* transport error or non-2xx status */
} KryHttpStatus;

/* POST a JSON body (`Content-Type: application/json`). `authorization` is
 * sent as a Bearer token when non-NULL and non-empty. `timeout_s` bounds
 * the whole transfer. Returns NULL when the platform has no client. */
KryHttpRequest *kry_http_post_json(const char *url, const char *authorization,
                                   const char *json_body, int timeout_s);

/* GET a URL (also serves the file:// scheme, which tests use to stay
 * offline-safe). Returns NULL when unavailable. */
KryHttpRequest *kry_http_get(const char *url, int timeout_s);

/* Current state. Cheap; call every frame. Terminal states never change. */
KryHttpStatus kry_http_poll(KryHttpRequest *request);

/* HTTP status code once DONE/FAILED (0 while PENDING/RUNNING). */
int kry_http_status_code(KryHttpRequest *request);

/* Response body when DONE, a diagnostic ("curl error 7: ...") when FAILED,
 * NULL otherwise. Owned by the request; valid until kry_http_free. */
const char *kry_http_response(KryHttpRequest *request);
size_t kry_http_partial(KryHttpRequest *request, char *buf, size_t size);

/* Release the request. Joins the worker thread, so freeing a request that
 * is still RUNNING blocks up to the remaining timeout — poll to a terminal
 * state first. NULL is allowed. */
void kry_http_free(KryHttpRequest *request);

/* --- streaming download to a file -------------------------------------- */

typedef struct KryHttpDownload KryHttpDownload;

/* GET `url`, streaming the body into `dest_path` (truncated on start)
 * instead of buffering it in memory — the path large artifacts need.
 * Same lifecycle as KryHttpRequest: poll, then free. A FAILED download
 * leaves a partial file at `dest_path`; the caller decides what to do
 * with it. Returns NULL when the platform has no client or the file
 * cannot be opened for writing. */
KryHttpDownload *kry_http_download(const char *url, const char *dest_path,
                                   int timeout_s);

KryHttpStatus kry_http_download_poll(KryHttpDownload *download);

/* 0..1 fraction of the transfer, or -1.0 while the total size is unknown
 * (no Content-Length yet, or a non-HTTP scheme). */
double kry_http_download_progress(KryHttpDownload *download);

/* Bytes written so far. */
unsigned long kry_http_download_bytes(KryHttpDownload *download);

/* Diagnostic when FAILED, NULL otherwise. Owned by the download. */
const char *kry_http_download_error(KryHttpDownload *download);

/* Release the download (joins the worker). NULL is allowed. */
void kry_http_download_free(KryHttpDownload *download);

#ifdef __cplusplus
}
#endif

#endif

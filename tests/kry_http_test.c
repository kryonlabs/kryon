/*
 * kry_http_test.c - async HTTP client tests.
 *
 * Hermetic core: a GET against file:// exercises the worker thread, write
 * callback, and poll lifecycle without a network. A live HTTPS round trip
 * runs only when KRYON_HTTP_TEST_URL is set in the environment.
 */
#include "kry_http.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static KryHttpStatus
poll_until_terminal(KryHttpRequest *r, int spins)
{
    KryHttpStatus s = kry_http_poll(r);
    int i;

    for(i = 0; i < spins && s != KRY_HTTP_DONE && s != KRY_HTTP_FAILED; i++) {
        struct timespec ts = {0, 10 * 1000 * 1000};

        nanosleep(&ts, NULL);
        s = kry_http_poll(r);
    }
    return s;
}

static void
test_file_get(void)
{
    char path[256];
    char url[300];
    FILE *f;
    KryHttpRequest *r;
    KryHttpStatus s;

    snprintf(path, sizeof(path), "/tmp/kry_http_test.%d.txt", (int)getpid());
    f = fopen(path, "wb");
    if(f == NULL)
        return;
    fprintf(f, "hello from the filesystem");
    fclose(f);
    snprintf(url, sizeof(url), "file://%s", path);

    r = kry_http_get(url, 10);
    if(r == NULL) {
        printf("kry_http unavailable (no libcurl); skipping\n");
        remove(path);
        return;
    }
    s = poll_until_terminal(r, 500);
    CHECK(s == KRY_HTTP_DONE);
    CHECK(kry_http_response(r) != NULL);
    if(kry_http_response(r) != NULL)
        CHECK(strcmp(kry_http_response(r), "hello from the filesystem") == 0);
    kry_http_free(r);

    r = kry_http_get("file:///tmp/definitely-not-here-kryon", 5);
    if(r != NULL) {
        s = poll_until_terminal(r, 200);
        CHECK(s == KRY_HTTP_FAILED);
        CHECK(kry_http_response(r) != NULL);
        kry_http_free(r);
    }
    remove(path);
}

static void
test_get_with_headers_file(void)
{
    char path[256];
    char url[300];
    char token_buf[64];
    const char *headers[2];
    FILE *f;
    KryHttpRequest *r;
    KryHttpStatus s;

    snprintf(path, sizeof(path), "/tmp/kry_http_test.%d.hdr.txt", (int)getpid());
    f = fopen(path, "wb");
    if(f == NULL)
        return;
    fprintf(f, "with headers");
    fclose(f);
    snprintf(url, sizeof(url), "file://%s", path);

    snprintf(token_buf, sizeof(token_buf), "X-Test-Token: abc123");
    headers[0] = token_buf;
    headers[1] = NULL;
    r = kry_http_get_with_headers(url, 10, headers, 1);
    if(r == NULL) {
        printf("kry_http unavailable (no libcurl); skipping header test\n");
        remove(path);
        return;
    }
    /* Scribble the caller's storage: the request must own its own copies
     * before the worker thread reads them. */
    memset(token_buf, 'x', sizeof(token_buf));
    s = poll_until_terminal(r, 500);
    CHECK(s == KRY_HTTP_DONE);
    if(kry_http_response(r) != NULL)
        CHECK(strcmp(kry_http_response(r), "with headers") == 0);
    kry_http_free(r);
    remove(path);
}

static void
test_bad_url_fails(void)
{
    KryHttpRequest *r = kry_http_get("not-a-url", 5);

    if(r == NULL)
        return;
    CHECK(poll_until_terminal(r, 300) == KRY_HTTP_FAILED);
    CHECK(kry_http_response(r) != NULL);
    kry_http_free(r);
}

static void
test_live_gated(void)
{
    const char *url = getenv("KRYON_HTTP_TEST_URL");
    KryHttpRequest *r;
    KryHttpStatus s;

    if(url == NULL || url[0] == '\0')
        return;
    r = kry_http_get(url, 30);
    if(r == NULL) {
        printf("kry_http unavailable; live test skipped\n");
        return;
    }
    s = poll_until_terminal(r, 6000);
    if(s != KRY_HTTP_DONE) {
        printf("live test failed: %s\n",
               kry_http_response(r) != NULL ? kry_http_response(r) : "?");
        failures++;
    }
    kry_http_free(r);
}

int
main(void)
{
    test_file_get();
    test_get_with_headers_file();
    test_bad_url_fails();
    test_live_gated();
    if(failures == 0)
        printf("kry_http tests passed\n");
    return failures == 0 ? 0 : 1;
}

/*
 * kry_zai.c - z.ai GLM chat client for the Kry standard library.
 *
 * Wire format follows the OpenAI-compatible /chat/completions endpoint.
 * The default base URL is the GLM Coding Plan endpoint: Coding Plan keys
 * are not visible to the general API and answer "insufficient balance"
 * there (see https://docs.z.ai/guides/develop/http/introduction).
 */
#include "kry_zai.h"
#include "kry_http.h"
#include "kry_json.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define KRY_ZAI_DEFAULT_BASE "https://api.z.ai/api/coding/paas/v4"
#define KRY_ZAI_DEFAULT_MODEL "glm-4.6"

struct KryZaiRequest {
    KryHttpRequest *http;
    char *request_body;
    char *text;          /* assistant answer, set on first read */
};

static char g_key_override[256];

void
kry_zai_set_key(const char *api_key)
{
    if(api_key == NULL)
        api_key = "";
    snprintf(g_key_override, sizeof(g_key_override), "%s", api_key);
}

static const char *
active_key(void)
{
    if(g_key_override[0] != '\0')
        return g_key_override;
    return getenv("ZAI_API_KEY");
}

int
kry_zai_configured(void)
{
    const char *key = active_key();

    return key != NULL && key[0] != '\0';
}

static void
build_body(KryJsonBuf *b, const KryZaiMessage *messages, int count)
{
    int i;

    kry_json_buf_raw(b, "{\"model\":");
    kry_json_buf_str(b, getenv("ZAI_MODEL") != NULL && getenv("ZAI_MODEL")[0] != '\0'
                        ? getenv("ZAI_MODEL") : KRY_ZAI_DEFAULT_MODEL);
    kry_json_buf_raw(b, ",\"messages\":[");
    for(i = 0; i < count; i++) {
        if(i > 0)
            kry_json_buf_raw(b, ",");
        kry_json_buf_raw(b, "{\"role\":");
        kry_json_buf_str(b, messages[i].role);
        kry_json_buf_raw(b, ",\"content\":");
        kry_json_buf_str(b, messages[i].content);
        kry_json_buf_raw(b, "}");
    }
    kry_json_buf_raw(b, "],\"thinking\":{\"type\":\"disabled\"}}");
}

KryZaiRequest *
kry_zai_chat(const KryZaiMessage *messages, int count, int timeout_s)
{
    const char *key = active_key();
    const char *base = getenv("ZAI_BASE_URL");
    KryZaiRequest *r;
    KryJsonBuf body = {0};
    char url[1024];

    if(key == NULL || key[0] == '\0' || messages == NULL || count <= 0)
        return NULL;
    if(base == NULL || base[0] == '\0')
        base = KRY_ZAI_DEFAULT_BASE;
    build_body(&body, messages, count);
    r = calloc(1, sizeof(*r));
    if(r == NULL) {
        kry_json_buf_free(&body);
        return NULL;
    }
    r->request_body = strdup(kry_json_buf_finish(&body));
    kry_json_buf_free(&body);
    snprintf(url, sizeof(url), "%s/chat/completions", base);
    r->http = kry_http_post_json(url, key, r->request_body,
                                 timeout_s > 0 ? timeout_s : 180);
    if(r->http == NULL) {
        free(r->request_body);
        free(r);
        return NULL;
    }
    return r;
}

KryZaiStatus
kry_zai_poll(KryZaiRequest *r)
{
    if(r == NULL)
        return KRY_ZAI_FAILED;
    switch(kry_http_poll(r->http)) {
    case KRY_HTTP_DONE: return KRY_ZAI_DONE;
    case KRY_HTTP_FAILED: return KRY_ZAI_FAILED;
    default: return KRY_ZAI_RUNNING;
    }
}

/* Extract choices[0].message.content into *out (malloc'd). Returns 0 on
 * parse failure with *why pointing at a static diagnostic. */
static int
extract_text(const char *response, char **out, const char **why)
{
    KryJson *root = kry_json_parse(response);
    KryJson *content;
    const char *text;

    *out = NULL;
    if(root == NULL) {
        *why = "response is not valid JSON";
        return 0;
    }
    content = kry_json_get(root, "choices");
    content = kry_json_at(content, 0);
    content = kry_json_get(content, "message");
    content = kry_json_get(content, "content");
    text = kry_json_string(content);
    if(text == NULL) {
        kry_json_free(root);
        *why = "no choices[0].message.content";
        return 0;
    }
    *out = strdup(text);
    kry_json_free(root);
    return *out != NULL;
}

const char *
kry_zai_text(KryZaiRequest *r)
{
    char *text;
    const char *why;

    if(r == NULL || kry_http_poll(r->http) != KRY_HTTP_DONE)
        return NULL;
    if(r->text != NULL)
        return r->text;
    if(extract_text(kry_http_response(r->http), &text, &why)) {
        r->text = text;
        return r->text;
    }
    return NULL;
}

const char *
kry_zai_error(KryZaiRequest *r)
{
    static char msg[256];
    const char *response;

    if(r == NULL || kry_http_poll(r->http) != KRY_HTTP_FAILED)
        return NULL;
    response = kry_http_response(r->http);
    snprintf(msg, sizeof(msg), "%s",
             response != NULL ? response : "request failed");
    return msg;
}

const char *
kry_zai_request_body(KryZaiRequest *r)
{
    return r != NULL ? r->request_body : NULL;
}

void
kry_zai_free(KryZaiRequest *r)
{
    if(r == NULL)
        return;
    kry_http_free(r->http);
    free(r->request_body);
    free(r->text);
    free(r);
}

/*
 * kry_zai.h - Kry standard library: z.ai GLM chat client.
 *
 * Thin OpenAI-compatible chat-completions client over kry_http + kry_json.
 * Configuration comes from the environment (ZAI_API_KEY, optional
 * ZAI_BASE_URL, ZAI_MODEL) with an in-process override; the default
 * endpoint is the GLM Coding Plan one, distinct from the general API.
 * Calls are asynchronous: start, poll, read, free.
 */
#ifndef KRYON_KRY_ZAI_H
#define KRYON_KRY_ZAI_H

#ifdef __cplusplus
extern "C" {
#endif

typedef struct KryZaiRequest KryZaiRequest;

typedef enum {
    KRY_ZAI_PENDING,
    KRY_ZAI_RUNNING,
    KRY_ZAI_DONE,     /* assistant text ready via kry_zai_text */
    KRY_ZAI_FAILED,   /* diagnostic via kry_zai_error */
} KryZaiStatus;

typedef struct {
    const char *role;     /* "system" | "user" | "assistant" */
    const char *content;
} KryZaiMessage;

/* Override the API key for the process (wins over ZAI_API_KEY). Pass an
 * empty string to clear. */
void kry_zai_set_key(const char *api_key);

/* 1 when a key is configured (override or env) and the HTTP client is
 * available. */
int kry_zai_configured(void);

/* Start a chat completion (thinking disabled for fast answers). Returns
 * NULL when unconfigured or unavailable. */
KryZaiRequest *kry_zai_chat(const KryZaiMessage *messages, int count,
                            int timeout_s);

KryZaiStatus kry_zai_poll(KryZaiRequest *request);

/* Assistant message text when DONE; NULL otherwise. Owned by the request. */
const char *kry_zai_text(KryZaiRequest *request);

/* Diagnostic when FAILED ("HTTP 401", "curl error ...", parse errors);
 * NULL otherwise. */
const char *kry_zai_error(KryZaiRequest *request);

/* The request body that was sent (for logging/tests). NULL if none. */
const char *kry_zai_request_body(KryZaiRequest *request);

void kry_zai_free(KryZaiRequest *request);

#ifdef __cplusplus
}
#endif

#endif

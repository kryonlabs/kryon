#include "flint_lyra_sync.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct TestCtx {
    char token[64];
    char expires[32];
    char last_method[16];
    char last_url[256];
    char last_body[256];
    char last_auth[128];
    int apply_called;
    int purge_called;
} TestCtx;

static int failures = 0;

static void
check(int condition, const char *message)
{
    if(!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static const char *
test_get_text(const char *key, void *user)
{
    TestCtx *ctx = user;
    if(strcmp(key, "sync_auth_token") == 0)
        return ctx->token;
    if(strcmp(key, "sync_auth_token_expires_at") == 0)
        return ctx->expires;
    return "";
}

static void
test_set_text(const char *key, const char *value, void *user)
{
    TestCtx *ctx = user;
    if(strcmp(key, "sync_auth_token") == 0)
        snprintf(ctx->token, sizeof(ctx->token), "%s", value != NULL ? value : "");
    if(strcmp(key, "sync_auth_token_expires_at") == 0)
        snprintf(ctx->expires, sizeof(ctx->expires), "%s", value != NULL ? value : "");
}

static int
test_http(const char *method, const char *url, const char *body,
          const char *const *headers, int header_count,
          FlintLyraSyncBuffer *response, long *status, void *user)
{
    TestCtx *ctx = user;
    snprintf(ctx->last_method, sizeof(ctx->last_method), "%s", method != NULL ? method : "");
    snprintf(ctx->last_url, sizeof(ctx->last_url), "%s", url != NULL ? url : "");
    snprintf(ctx->last_body, sizeof(ctx->last_body), "%s", body != NULL ? body : "");
    ctx->last_auth[0] = '\0';
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL && strncmp(headers[i], "Authorization:", 14) == 0)
            snprintf(ctx->last_auth, sizeof(ctx->last_auth), "%s", headers[i]);
    }
    if(status != NULL)
        *status = 200;
    return flint_lyra_sync_buffer_append(response, "{\"status\":\"ok\",\"changes\":{}}",
                                         strlen("{\"status\":\"ok\",\"changes\":{}}"));
}

static char *
test_build_payload(const char *user_id_hash, const char *public_key_hex, void *user)
{
    (void)public_key_hex;
    (void)user;
    char *payload = malloc(96);
    if(payload != NULL)
        snprintf(payload, 96, "{\"user_id_hash\":\"%s\",\"client_id\":\"client-123\"}",
                 user_id_hash);
    return payload;
}

static void
test_free_payload(char *payload, void *user)
{
    (void)user;
    free(payload);
}

static int
test_apply_response(const char *response_json, void *user)
{
    TestCtx *ctx = user;
    ctx->apply_called++;
    return response_json != NULL && strstr(response_json, "\"status\":\"ok\"") != NULL;
}

static void
test_purge(void *user)
{
    TestCtx *ctx = user;
    ctx->purge_called++;
}

static void
fill_account(FlintLyraAccount *account)
{
    memset(account, 0, sizeof(*account));
    memset(account->public_id, 'a', 64);
    account->public_id[64] = '\0';
    snprintf(account->public_key_hex, sizeof(account->public_key_hex), "pk");
    snprintf(account->private_key_hex, sizeof(account->private_key_hex), "sk");
}

static void
test_url_helpers(void)
{
    char out[128];

    check(flint_lyra_sync_url_valid("https://api.example.test"), "https url valid");
    check(flint_lyra_sync_url_valid("localhost:8080"), "localhost shorthand valid");
    check(!flint_lyra_sync_url_valid("http://example.test"), "plain remote http rejected");
    check(flint_lyra_sync_normalize_url("localhost:8080", out, sizeof(out)) &&
              strcmp(out, "http://localhost:8080") == 0,
          "normalize localhost");
    check(flint_lyra_sync_join_url(out, sizeof(out), "https://api.example.test/",
                                   "/api/v1/sync") &&
              strcmp(out, "https://api.example.test/api/v1/sync") == 0,
          "join trims trailing slash");
    check(flint_lyra_sync_join_ws_url(out, sizeof(out), "https://api.example.test",
                                      "/api/v1/sync/ws") &&
              strcmp(out, "wss://api.example.test/api/v1/sync/ws") == 0,
          "join websocket https");
}

static void
test_json_helpers(void)
{
    FlintLyraSyncBuffer buffer = {0};
    char value[32];

    check(flint_lyra_sync_buffer_append_json_string(&buffer, "a\"b\n") &&
              strcmp(buffer.data, "\"a\\\"b\\n\"") == 0,
          "json string escaping");
    flint_lyra_sync_buffer_free(&buffer);
    check(flint_lyra_sync_find_json_string("{\"auth_token\":\"tok\"}", "auth_token",
                                           value, sizeof(value)) &&
              strcmp(value, "tok") == 0,
          "json string find");
    check(flint_lyra_sync_find_json_int64("{\"expires_in_seconds\":42}",
                                          "expires_in_seconds", 0) == 42,
          "json int find");
}

static void
test_sync_run_with_valid_token(void)
{
    TestCtx ctx = {0};
    FlintLyraAccount account;
    FlintLyraSyncConfig cfg;
    FlintLyraSyncResult result;

    fill_account(&account);
    snprintf(ctx.token, sizeof(ctx.token), "saved-token");
    snprintf(ctx.expires, sizeof(ctx.expires), "%lld", (long long)time(NULL) + 3600);
    memset(&cfg, 0, sizeof(cfg));
    cfg.base_url = "https://api.example.test";
    cfg.account = &account;
    cfg.client_id = "client-123";
    cfg.http_request = test_http;
    cfg.get_text = test_get_text;
    cfg.set_text = test_set_text;
    cfg.build_payload = test_build_payload;
    cfg.free_payload = test_free_payload;
    cfg.apply_response = test_apply_response;
    cfg.purge_synced_deleted = test_purge;
    cfg.user = &ctx;

    result = flint_lyra_sync_run(&cfg);
    check(result == FLINT_LYRA_SYNC_OK, "sync run returns ok");
    check(strcmp(ctx.last_method, "POST") == 0, "sync uses post");
    check(strcmp(ctx.last_url, "https://api.example.test/api/v1/sync") == 0,
          "sync posts to sync path");
    check(strcmp(ctx.last_auth, "Authorization: Bearer saved-token") == 0,
          "sync sends bearer token");
    check(ctx.apply_called == 1, "sync applies response");
    check(ctx.purge_called == 1, "sync purges after success");
}

int
main(void)
{
    test_url_helpers();
    test_json_helpers();
    test_sync_run_with_valid_token();
    return failures == 0 ? 0 : 1;
}

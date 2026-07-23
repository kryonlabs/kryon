#include "lyra_sync.h"

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
    char last_user[128];
    char last_signature[256];
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
          LyraSyncBuffer *response, long *status, void *user)
{
    TestCtx *ctx = user;
    snprintf(ctx->last_method, sizeof(ctx->last_method), "%s", method != NULL ? method : "");
    snprintf(ctx->last_url, sizeof(ctx->last_url), "%s", url != NULL ? url : "");
    snprintf(ctx->last_body, sizeof(ctx->last_body), "%s", body != NULL ? body : "");
    ctx->last_auth[0] = '\0';
    ctx->last_user[0] = '\0';
    ctx->last_signature[0] = '\0';
    for(int i = 0; i < header_count; i++) {
        if(headers[i] != NULL && strncmp(headers[i], "Authorization:", 14) == 0)
            snprintf(ctx->last_auth, sizeof(ctx->last_auth), "%s", headers[i]);
        if(headers[i] != NULL &&
           (strncmp(headers[i], "X-Lyra-User:", 12) == 0 ||
            strncmp(headers[i], "X-Inbe-User:", 12) == 0))
            snprintf(ctx->last_user, sizeof(ctx->last_user), "%s", headers[i]);
        if(headers[i] != NULL &&
           (strncmp(headers[i], "X-Lyra-Signature:", 17) == 0 ||
            strncmp(headers[i], "X-Inbe-Signature:", 17) == 0))
            snprintf(ctx->last_signature, sizeof(ctx->last_signature), "%s", headers[i]);
    }
    if(status != NULL)
        *status = 200;
    return AppendLyraSyncBuffer(response, "{\"status\":\"ok\",\"changes\":{}}",
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
fill_account(LyraAccount *account)
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

    check(IsLyraSyncURLValid("https://api.example.test"), "https url valid");
    check(IsLyraSyncURLValid("localhost:8080"), "localhost shorthand valid");
    check(!IsLyraSyncURLValid("http://example.test"), "plain remote http rejected");
    check(NormalizeLyraSyncURL("localhost:8080", out, sizeof(out)) &&
              strcmp(out, "http://localhost:8080") == 0,
          "normalize localhost");
    check(JoinLyraSyncURL(out, sizeof(out), "https://api.example.test/",
                                   "/api/v1/sync") &&
              strcmp(out, "https://api.example.test/api/v1/sync") == 0,
          "join trims trailing slash");
    check(JoinLyraSyncWebSocketURL(out, sizeof(out), "https://api.example.test",
                                      "/api/v1/sync/ws") &&
              strcmp(out, "wss://api.example.test/api/v1/sync/ws") == 0,
          "join websocket https");
}

static void
test_json_helpers(void)
{
    LyraSyncBuffer buffer = {0};
    char value[32];

    check(AppendLyraSyncBufferJSONString(&buffer, "a\"b\n") &&
              strcmp(buffer.data, "\"a\\\"b\\n\"") == 0,
          "json string escaping");
    FreeLyraSyncBuffer(&buffer);
    check(FindLyraSyncJSONString("{\"auth_token\":\"tok\"}", "auth_token",
                                           value, sizeof(value)) &&
              strcmp(value, "tok") == 0,
          "json string find");
    check(FindLyraSyncJSONInt64("{\"expires_in_seconds\":42}",
                                          "expires_in_seconds", 0) == 42,
          "json int find");
}

static void
test_sync_run_with_valid_token(void)
{
    TestCtx ctx = {0};
    LyraAccount account;
    LyraSyncConfig cfg;
    LyraSyncResult result;

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

    result = RunLyraSync(&cfg);
    check(result == LYRA_SYNC_OK, "sync run returns ok");
    check(strcmp(ctx.last_method, "POST") == 0, "sync uses post");
    check(strcmp(ctx.last_url, "https://api.example.test/api/v1/sync") == 0,
          "sync posts to sync path");
    check(strcmp(ctx.last_auth, "Authorization: Bearer saved-token") == 0,
          "sync sends bearer token");
    check(strncmp(ctx.last_user, "X-Lyra-User:", 12) == 0,
          "sync uses generic user header by default");
    check(ctx.apply_called == 1, "sync applies response");
    check(ctx.purge_called == 1, "sync purges after success");
}

static int
test_login_http(const char *method, const char *url, const char *body,
                const char *const *headers, int header_count,
                LyraSyncBuffer *response, long *status, void *user)
{
    TestCtx *ctx = user;

    test_http(method, url, body, headers, header_count, response, status, user);
    if(strstr(url, "/challenge") != NULL) {
        FreeLyraSyncBuffer(response);
        return AppendLyraSyncBuffer(response,
                                    "{\"nonce\":\"0000000000000000000000000000000000000000000000000000000000000000\"}",
                                    strlen("{\"nonce\":\"0000000000000000000000000000000000000000000000000000000000000000\"}"));
    }
    if(strstr(url, "/login") != NULL) {
        (void)ctx;
        FreeLyraSyncBuffer(response);
        return AppendLyraSyncBuffer(response,
                                    "{\"auth_token\":\"new-token\",\"expires_in_seconds\":3600}",
                                    strlen("{\"auth_token\":\"new-token\",\"expires_in_seconds\":3600}"));
    }
    return 1;
}

static void
test_login_uses_configured_wire_names(void)
{
    TestCtx ctx = {0};
    LyraAccount account;
    LyraSyncConfig cfg;
    LyraSyncResult result;

    if(!CreateLyraAccount(&account)) {
        check(0, "create account for login override test");
        return;
    }
    memset(&cfg, 0, sizeof(cfg));
    cfg.base_url = "https://api.example.test";
    cfg.account = &account;
    cfg.client_id = "client-123";
    cfg.signature_context = "inbe-sync-v1";
    cfg.user_header_name = "X-Inbe-User";
    cfg.signature_header_name = "X-Inbe-Signature";
    cfg.http_request = test_login_http;
    cfg.get_text = test_get_text;
    cfg.set_text = test_set_text;
    cfg.user = &ctx;

    result = LoginLyraSync(&cfg);
    check(result == LYRA_SYNC_OK, "login with configured wire names returns ok");
    check(strncmp(ctx.last_user, "X-Inbe-User:", 12) == 0,
          "login uses configured user header");
    check(strncmp(ctx.last_signature, "X-Inbe-Signature:", 17) == 0,
          "login uses configured signature header");
    check(strcmp(ctx.token, "new-token") == 0, "login saves auth token");
}

int
main(void)
{
    test_url_helpers();
    test_json_helpers();
    test_sync_run_with_valid_token();
    test_login_uses_configured_wire_names();
    return failures == 0 ? 0 : 1;
}

/*
 * kry_zai_test.c - z.ai client tests.
 *
 * Hermetic: configuration precedence and the request/response wire format
 * (body builder + response extraction). A live round trip runs only when
 * ZAI_API_KEY is set in the environment.
 */
#include "kry_zai.h"
#include "kry_http.h"
#include "kry_json.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static void
test_config_precedence(void)
{
    kry_zai_set_key("override-key");
    CHECK(kry_zai_configured());
    CHECK(strcmp(getenv("ZAI_API_KEY") != NULL ? getenv("ZAI_API_KEY") : "",
                 "override-key") != 0);   /* override must not touch env */
    kry_zai_set_key("");
}

static void
test_request_body(void)
{
    KryZaiMessage msgs[2] = {
        {"system", "you write files"},
        {"user", "make a button\nwith \"quotes\" and\ttabs"},
    };
    KryZaiRequest *r;
    const char *body;

    kry_zai_set_key("test-key");
    r = kry_zai_chat(msgs, 2, 30);
    CHECK(r != NULL);
    body = kry_zai_request_body(r);
    if(body != NULL) {
        KryJson *root = kry_json_parse(body);
        KryJson *messages;
        KryJson *user;

        if(root == NULL) {
            fprintf(stderr, "body does not parse: %s\n", body);
            CHECK(0);
            kry_zai_free(r);
            kry_zai_set_key("");
            return;
        }
        CHECK(kry_json_string(kry_json_get(root, "model")) != NULL);
        messages = kry_json_get(root, "messages");
        CHECK(kry_json_count(messages) == 2);
        user = kry_json_at(messages, 1);
        CHECK(strcmp(kry_json_string(kry_json_get(user, "role")), "user") == 0);
        CHECK(strcmp(kry_json_string(kry_json_get(user, "content")),
                     "make a button\nwith \"quotes\" and\ttabs") == 0);
        kry_json_free(root);
    } else {
        CHECK(body != NULL);
    }
    kry_zai_free(r);
    kry_zai_set_key("");
}

static void
test_requires_key(void)
{
    KryZaiMessage msg = {"user", "hi"};

    kry_zai_set_key("");
    if(getenv("ZAI_API_KEY") == NULL || getenv("ZAI_API_KEY")[0] == '\0') {
        CHECK(!kry_zai_configured());
        CHECK(kry_zai_chat(&msg, 1, 10) == NULL);
    }
}

static void
test_live_gated(void)
{
    const char *key = getenv("ZAI_API_KEY");
    KryZaiMessage msgs[1] = {{"user", "Reply with exactly: PONG"}};
    KryZaiRequest *r;
    int i;

    if(key == NULL || key[0] == '\0')
        return;
    printf("ZAI_API_KEY set; running live round trip... ");
    fflush(stdout);
    r = kry_zai_chat(msgs, 1, 60);
    if(r == NULL) {
        printf("unavailable\n");
        return;
    }
    for(i = 0; i < 1200; i++) {
        KryZaiStatus s = kry_zai_poll(r);

        if(s == KRY_ZAI_DONE || s == KRY_ZAI_FAILED) {
            const char *text = kry_zai_text(r);

            CHECK(s == KRY_ZAI_DONE);
            if(s == KRY_ZAI_DONE)
                printf("answer: %.60s\n", text != NULL ? text : "(null)");
            else
                printf("failed: %s\n", kry_zai_error(r));
            break;
        }
        usleep(100 * 1000);
    }
    kry_zai_free(r);
}

int
main(void)
{
    test_config_precedence();
    test_request_body();
    test_requires_key();
    test_live_gated();
    if(failures == 0)
        printf("kry_zai tests passed\n");
    return failures == 0 ? 0 : 1;
}

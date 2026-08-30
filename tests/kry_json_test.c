/*
 * kry_json_test.c - parser/emitter unit tests (hermetic, no network).
 */
#include "kry_json.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

static void
test_parse_object(void)
{
    const char *src = "{\"model\":\"glm-4.6\",\"n\":3,\"ok\":true,"
                      "\"none\":null,\"list\":[1,\"two\",{\"deep\":-2.5}]}";
    KryJson *root = kry_json_parse(src);
    KryJson *list;
    KryJson *deep;

    CHECK(root != NULL);
    CHECK(kry_json_count(root) == 5);
    CHECK(strcmp(kry_json_string(kry_json_get(root, "model")), "glm-4.6") == 0);
    CHECK(kry_json_number(kry_json_get(root, "n")) == 3.0);
    CHECK(kry_json_bool(kry_json_get(root, "ok")) == 1);
    CHECK(kry_json_get(root, "none")->type == KRY_JSON_NULL);
    list = kry_json_get(root, "list");
    CHECK(list != NULL && list->type == KRY_JSON_ARRAY);
    CHECK(kry_json_count(list) == 3);
    CHECK(kry_json_number(kry_json_at(list, 0)) == 1.0);
    CHECK(strcmp(kry_json_string(kry_json_at(list, 1)), "two") == 0);
    deep = kry_json_at(list, 2);
    CHECK(deep != NULL && deep->type == KRY_JSON_OBJECT);
    CHECK(kry_json_number(kry_json_get(deep, "deep")) == -2.5);
    CHECK(kry_json_get(root, "missing") == NULL);
    CHECK(kry_json_key(root, 0) != NULL && strcmp(kry_json_key(root, 0), "model") == 0);
    kry_json_free(root);
}

static void
test_string_escapes(void)
{
    KryJson *root = kry_json_parse("{\"s\":\"a\\nb\\t\\\"q\\\"\\\\\\u0041\\u00e9\"}");
    const char *s;

    CHECK(root != NULL);
    s = kry_json_string(kry_json_get(root, "s"));
    CHECK(s != NULL && strcmp(s, "a\nb\t\"q\"\\A\xC3\xA9") == 0);
    kry_json_free(root);

    /* surrogate pair: U+1F600 */
    root = kry_json_parse("{\"e\":\"\\uD83D\\uDE00\"}");
    s = kry_json_string(kry_json_get(root, "e"));
    CHECK(s != NULL && (unsigned char)s[0] == 0xF0);
    CHECK(s != NULL && strlen(s) == 4);
    kry_json_free(root);
}

static void
test_reject_malformed(void)
{
    CHECK(kry_json_parse("") == NULL);
    CHECK(kry_json_parse("{") == NULL);
    CHECK(kry_json_parse("{\"a\"}") == NULL);
    CHECK(kry_json_parse("{\"a\":}") == NULL);
    CHECK(kry_json_parse("[1,]") == NULL);
    CHECK(kry_json_parse("\"unterminated") == NULL);
    CHECK(kry_json_parse("1.2.3") == NULL);
    CHECK(kry_json_parse("{\"a\":1} trailing") == NULL);
}

static void
test_emit(void)
{
    KryJsonBuf b = {0};

    kry_json_buf_raw(&b, "{\"role\":");
    kry_json_buf_str(&b, "system\nwith \"quotes\"\tand \x01");
    kry_json_buf_raw(&b, ",\"temp\":");
    kry_json_buf_num(&b, 0.5);
    kry_json_buf_raw(&b, ",\"n\":");
    kry_json_buf_num(&b, 3);
    kry_json_buf_raw(&b, "}");
    {
        const char *out = kry_json_buf_finish(&b);
        KryJson *back = kry_json_parse(out);
        const char *s;

        CHECK(back != NULL);
        s = kry_json_string(kry_json_get(back, "role"));
        CHECK(s != NULL && strcmp(s, "system\nwith \"quotes\"\tand \x01") == 0);
        CHECK(kry_json_number(kry_json_get(back, "temp")) == 0.5);
        CHECK(kry_json_number(kry_json_get(back, "n")) == 3.0);
        kry_json_free(back);
    }
    kry_json_buf_free(&b);
}

static void
test_emit_overflow_guard(void)
{
    KryJsonBuf b = {0};

    b.len = ULONG_MAX - 1;
    b.cap = ULONG_MAX - 1;
    kry_json_buf_raw(&b, "x");
    CHECK(b.buf == NULL);
    CHECK(b.len == ULONG_MAX - 1);
    CHECK(b.cap == ULONG_MAX - 1);
    kry_json_buf_free(&b);
}

static void
test_depth_cap(void)
{
    char hostile[512];
    int i;

    for(i = 0; i < 100; i++)
        hostile[i] = '[';
    hostile[100] = '\0';
    CHECK(kry_json_parse(hostile) == NULL);
}

int
main(void)
{
    test_parse_object();
    test_string_escapes();
    test_reject_malformed();
    test_emit();
    test_emit_overflow_guard();
    test_depth_cap();
    if(failures == 0)
        printf("kry_json tests passed\n");
    return failures == 0 ? 0 : 1;
}

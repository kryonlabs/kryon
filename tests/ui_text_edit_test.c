/* Unit tests for the pure UTF-8 codec and text-buffer helpers in
 * ui_text_edit.c. Like transition_test.c, this compiles the implementation
 * directly (no GUI/raylib link): GetCodepointNext and ui_clampi are provided
 * here as faithful stubs. */

#include <stdio.h>
#include <string.h>

/* Declarations under test (declared in ui_internal.h, but we avoid pulling in
 * the whole UI header chain here; these are the only symbols we exercise). */
int ui_utf8_next_offset(const char *text, int offset);
int ui_utf8_prev_offset(const char *text, int offset);
int ui_utf8_codepoint_count(const char *text);
int ui_utf8_encode(int codepoint, char out[5]);
int ui_text_delete_range(char *text, size_t text_size, int *cursor,
                         int start, int end);
int ui_text_insert_ascii(char *text, size_t text_size, int *cursor, char ch,
                         int max_codepoints);
int ui_text_insert_codepoint(char *text, size_t text_size, int *cursor,
                             int codepoint, int max_codepoints);

static int failures = 0;

static void
check_true(const char *name, int ok)
{
    if(!ok) {
        fprintf(stderr, "FAIL: %s\n", name);
        failures++;
    }
}

static void
check_int(const char *name, int actual, int expected)
{
    if(actual != expected) {
        fprintf(stderr, "FAIL: %s got %d want %d\n", name, actual, expected);
        failures++;
    }
}

static void
check_str(const char *name, const char *actual, const char *expected)
{
    if(strcmp(actual, expected) != 0) {
        fprintf(stderr, "FAIL: %s got \"%s\" want \"%s\"\n", name, actual, expected);
        failures++;
    }
}

/* Faithful UTF-8 decoder matching raylib's GetCodepointNext: writes the byte
 * length of the codepoint starting at text into *codepointSize and returns the
 * codepoint value ('?' on invalid input). */
int
GetCodepointNext(const char *text, int *codepointSize)
{
    int len = 0;
    int codepoint = 0x3f; /* '?' */
    unsigned char c = (unsigned char)text[0];

    if(c == '\0') {
        if(codepointSize != NULL)
            *codepointSize = 0;
        return 0;
    }

    if(c < 0x80) {
        codepoint = c;
        len = 1;
    } else if((c & 0xe0) == 0xc0) {
        codepoint = (c & 0x1f) << 6;
        codepoint |= (unsigned char)text[1] & 0x3f;
        len = 2;
    } else if((c & 0xf0) == 0xe0) {
        codepoint = (c & 0x0f) << 12;
        codepoint |= ((unsigned char)text[1] & 0x3f) << 6;
        codepoint |= (unsigned char)text[2] & 0x3f;
        len = 3;
    } else if((c & 0xf8) == 0xf0) {
        codepoint = (c & 0x07) << 18;
        codepoint |= ((unsigned char)text[1] & 0x3f) << 12;
        codepoint |= ((unsigned char)text[2] & 0x3f) << 6;
        codepoint |= (unsigned char)text[3] & 0x3f;
        len = 4;
    } else {
        len = 1;
    }

    if(codepointSize != NULL)
        *codepointSize = len;
    return codepoint;
}

/* ui_clampi is the other external symbol ui_text_edit.c needs. */
int
ui_clampi(int value, int min_value, int max_value)
{
    if(value < min_value)
        return min_value;
    if(value > max_value)
        return max_value;
    return value;
}

static void
test_utf8_next_offset(void)
{
    /* "aé€" = 'a'(1) + é(2) + €(3) bytes */
    const char *s = "a\xc3\xa9\xe2\x82\xac";
    check_int("ascii next", ui_utf8_next_offset(s, 0), 1);
    check_int("2-byte next", ui_utf8_next_offset(s, 1), 3);
    check_int("3-byte next", ui_utf8_next_offset(s, 3), 6);
    check_int("at end returns len", ui_utf8_next_offset(s, 6), 6);
    check_int("null text returns 0", ui_utf8_next_offset(NULL, 0), 0);
}

static void
test_utf8_prev_offset(void)
{
    const char *s = "a\xc3\xa9\xe2\x82\xac";
    check_int("prev from end to €", ui_utf8_prev_offset(s, 6), 3);
    check_int("prev to é", ui_utf8_prev_offset(s, 3), 1);
    check_int("prev to a", ui_utf8_prev_offset(s, 1), 0);
    check_int("prev at 0 stays 0", ui_utf8_prev_offset(s, 0), 0);
}

static void
test_utf8_codepoint_count(void)
{
    check_int("count ascii", ui_utf8_codepoint_count("hello"), 5);
    check_int("count multibyte", ui_utf8_codepoint_count("a\xc3\xa9\xe2\x82\xac"), 3);
    check_int("count empty", ui_utf8_codepoint_count(""), 0);
    check_int("count null", ui_utf8_codepoint_count(NULL), 0);
}

static void
test_utf8_encode(void)
{
    char out[5];

    check_int("encode ascii len", ui_utf8_encode((int)'A', out), 1);
    check_str("encode ascii bytes", out, "A");

    check_int("encode é len", ui_utf8_encode(0xe9, out), 2);
    check_true("encode é bytes", out[0] == '\xc3' && out[1] == '\xa9' && out[2] == '\0');

    check_int("encode € len", ui_utf8_encode(0x20ac, out), 3);
    check_true("encode € bytes",
               out[0] == '\xe2' && out[1] == '\x82' && out[2] == '\xac' && out[3] == '\0');

    check_int("encode emoji len", ui_utf8_encode(0x1f600, out), 4);
    check_true("encode emoji bytes",
               out[0] == '\xf0' && out[1] == '\x9f' && out[2] == '\x98' && out[3] == '\x80'
               && out[4] == '\0');
}

static void
test_insert_ascii(void)
{
    char buf[16] = "ac";
    int cursor = 1;
    check_true("insert 'b' between a and c", ui_text_insert_ascii(buf, sizeof(buf), &cursor, 'b', 0));
    check_str("insert ascii result", buf, "abc");
    check_int("insert ascii cursor", cursor, 2);

    /* Buffer-full guard. */
    char tiny[3] = "a";
    int c2 = 1;
    check_true("insert into tiny buf ok", ui_text_insert_ascii(tiny, sizeof(tiny), &c2, 'b', 0));
    check_true("insert into full buf rejected", !ui_text_insert_ascii(tiny, sizeof(tiny), &c2, 'c', 0));
    check_str("tiny buf unchanged after reject", tiny, "ab");

    /* max_codepoints cap. */
    char capped[16] = "ab";
    int c3 = 2;
    check_true("insert blocked by max_codepoints",
               !ui_text_insert_ascii(capped, sizeof(capped), &c3, 'c', 2));
    check_str("capped buf unchanged", capped, "ab");
}

static void
test_insert_codepoint(void)
{
    char buf[16] = "";
    int cursor = 0;

    check_true("insert é", ui_text_insert_codepoint(buf, sizeof(buf), &cursor, 0xe9, 0));
    check_str("after é", buf, "\xc3\xa9");
    check_int("cursor after é", cursor, 2);

    check_true("insert €", ui_text_insert_codepoint(buf, sizeof(buf), &cursor, 0x20ac, 0));
    check_str("after €", buf, "\xc3\xa9\xe2\x82\xac");

    /* Control codepoints (<32) rejected. */
    int before = cursor;
    check_true("control codepoint rejected",
               !ui_text_insert_codepoint(buf, sizeof(buf), &cursor, 10, 0));
    check_int("cursor unchanged on reject", cursor, before);
}

static void
test_delete_range(void)
{
    char buf[16] = "hello";
    int cursor = 4;

    check_true("delete 'el'", ui_text_delete_range(buf, sizeof(buf), &cursor, 1, 3));
    check_str("after delete", buf, "hlo");
    check_int("cursor after delete", cursor, 1);

    /* end <= start is a no-op. */
    char buf2[16] = "abc";
    int c2 = 1;
    check_true("delete empty range rejected", !ui_text_delete_range(buf2, sizeof(buf2), &c2, 1, 1));
    check_str("unchanged on empty delete", buf2, "abc");
}

int
main(void)
{
    test_utf8_next_offset();
    test_utf8_prev_offset();
    test_utf8_codepoint_count();
    test_utf8_encode();
    test_insert_ascii();
    test_insert_codepoint();
    test_delete_range();
    if(failures != 0) {
        fprintf(stderr, "%d text-edit test(s) failed\n", failures);
        return 1;
    }
    printf("ui_text_edit tests passed\n");
    return 0;
}

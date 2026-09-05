/*
 * kry_zlib_test.c - zlib container unit tests (hermetic, no network).
 *
 * ZL_DYNAMIC and ZL_FIXED are real zlib streams (level 9 and level 1 from
 * CPython's zlib) so the full decoder paths are covered without spawning
 * external tools.
 */
#include "kry_zlib.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures;

#define CHECK(cond) do { \
    if(!(cond)) { \
        failures++; \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
    } \
} while(0)

#define ZL_DATA_LEN 1800u
#define ZL_SENTENCE_LEN 45u
static const char ZL_SENTENCE[] = "The quick brown fox jumps over the lazy dog. ";
static const unsigned char ZL_DYNAMIC[] = {
    120, 218, 11, 201, 72, 85, 40, 44, 205, 76, 206, 86, 72, 42, 202, 47,
    207, 83, 72, 203, 175, 80, 200, 42, 205, 45, 40, 86, 200, 47, 75, 45,
    82, 40, 1, 74, 231, 36, 86, 85, 42, 164, 228, 167, 235, 41, 132, 140,
    42, 30, 85, 60, 170, 120, 84, 241, 168, 226, 81, 197, 195, 75, 49, 0,
    136, 10, 134, 55};
static const unsigned char ZL_FIXED[] = {
    120, 1, 11, 201, 72, 85, 40, 44, 205, 76, 206, 86, 72, 42, 202, 47,
    207, 83, 72, 203, 175, 80, 200, 42, 205, 45, 40, 86, 200, 47, 75, 45,
    82, 40, 1, 74, 231, 36, 86, 85, 42, 164, 228, 167, 235, 41, 132, 140,
    42, 30, 13, 141, 209, 180, 49, 154, 83, 70, 139, 130, 209, 130, 113,
    120, 85, 19, 0, 136, 10, 134, 55};

static void
fill_repeated(char *buf, unsigned long len)
{
    unsigned long i;

    for(i = 0; i < len; i++)
        buf[i] = (char)('a' + (i % 26));
}

static void
test_detect(void)
{
    static const unsigned char zl[] = {0x78, 0x01, 0x03, 0x00};
    static const unsigned char gz[] = {0x1F, 0x8B, 0x08, 0x00};

    CHECK(kry_zlib_is(zl, sizeof(zl)) == 1);
    CHECK(kry_zlib_is(gz, sizeof(gz)) == 0); /* gzip is not zlib */
    CHECK(kry_zlib_is(NULL, 0) == 0);
    CHECK(kry_zlib_is(zl, 1) == 0);
    /* method not deflate */
    CHECK(kry_zlib_is((const unsigned char *)"\x79\x00", 2) == 0);
    /* FCHECK fails (0x7800 % 31 != 0) */
    CHECK(kry_zlib_is((const unsigned char *)"\x78\x00", 2) == 0);
    /* FDICT set (FCHECK passes for 0x7820) */
    CHECK(kry_zlib_is((const unsigned char *)"\x78\x20", 2) == 0);
}

static void
test_inflate_reference_streams(void)
{
    char expected[ZL_DATA_LEN + 1];
    unsigned long reps;
    unsigned char *out;
    unsigned long out_len = 0;

    for(reps = 0; reps < ZL_DATA_LEN / ZL_SENTENCE_LEN; reps++)
        memcpy(expected + reps * ZL_SENTENCE_LEN, ZL_SENTENCE,
               ZL_SENTENCE_LEN);
    expected[ZL_DATA_LEN] = '\0';

    out = kry_zlib_decompress(ZL_DYNAMIC, sizeof(ZL_DYNAMIC), &out_len);
    CHECK(out != NULL);
    CHECK(out_len == ZL_DATA_LEN);
    if(out != NULL && out_len == ZL_DATA_LEN)
        CHECK(memcmp(out, expected, ZL_DATA_LEN) == 0);
    CHECK(out != NULL && out[ZL_DATA_LEN] == '\0');
    free(out);

    out = kry_zlib_decompress(ZL_FIXED, sizeof(ZL_FIXED), &out_len);
    CHECK(out != NULL);
    CHECK(out_len == ZL_DATA_LEN);
    if(out != NULL && out_len == ZL_DATA_LEN)
        CHECK(memcmp(out, expected, ZL_DATA_LEN) == 0);
    free(out);
}

static void
test_round_trip_sizes(void)
{
    static const unsigned long sizes[] = {0, 1, 2, 100, 65535, 65536,
                                          65537, 200000};
    unsigned long i;

    for(i = 0; i < sizeof(sizes) / sizeof(sizes[0]); i++) {
        unsigned long n = sizes[i];
        char *plain = malloc(n != 0 ? n : 1);
        unsigned char *packed;
        unsigned char *plain2;
        unsigned long packed_len = 0;
        unsigned long back_len = 0;

        CHECK(plain != NULL);
        fill_repeated(plain, n);
        packed = kry_zlib_compress((const unsigned char *)plain, n,
                                   &packed_len);
        CHECK(packed != NULL);
        CHECK(kry_zlib_is(packed, packed_len) == 1);
        plain2 = kry_zlib_decompress(packed, packed_len, &back_len);
        CHECK(plain2 != NULL);
        CHECK(back_len == n);
        if(plain2 != NULL && back_len == n && n != 0)
            CHECK(memcmp(plain2, plain, n) == 0);
        if(plain2 != NULL && n == 0)
            CHECK(plain2[0] == '\0');
        free(plain2);
        free(packed);
        free(plain);
    }
}

static void
test_binary_payload(void)
{
    unsigned char raw[70000];
    unsigned long i;
    unsigned char *packed;
    unsigned char *back;
    unsigned long packed_len = 0;
    unsigned long back_len = 0;

    for(i = 0; i < sizeof(raw); i++)
        raw[i] = (unsigned char)(i * 7 + (i >> 9));
    packed = kry_zlib_compress(raw, sizeof(raw), &packed_len);
    CHECK(packed != NULL);
    back = kry_zlib_decompress(packed, packed_len, &back_len);
    CHECK(back != NULL);
    CHECK(back_len == sizeof(raw));
    if(back != NULL && back_len == sizeof(raw))
        CHECK(memcmp(back, raw, sizeof(raw)) == 0);
    free(back);
    free(packed);
}

static void
test_corrupt_input(void)
{
    unsigned char *packed;
    unsigned long packed_len = 0;

    packed = kry_zlib_compress((const unsigned char *)"abcdef", 6,
                               &packed_len);
    CHECK(packed != NULL);

    CHECK(kry_zlib_decompress(NULL, 10, NULL) == NULL);
    CHECK(kry_zlib_decompress((const unsigned char *)"not zlib", 8,
                              NULL) == NULL);
    CHECK(kry_zlib_decompress(packed, 3, NULL) == NULL);

    /* flip a payload byte: Adler-32 must reject */
    packed[packed_len - 5] ^= 0x40;
    CHECK(kry_zlib_decompress(packed, packed_len, NULL) == NULL);
    packed[packed_len - 5] ^= 0x40;

    /* flip an Adler-32 byte */
    packed[packed_len - 1] ^= 0x01;
    CHECK(kry_zlib_decompress(packed, packed_len, NULL) == NULL);
    packed[packed_len - 1] ^= 0x01;

    /* truncated trailer */
    CHECK(kry_zlib_decompress(packed, packed_len - 3, NULL) == NULL);

    /* sane again */
    {
        unsigned char *ok = kry_zlib_decompress(packed, packed_len, NULL);

        CHECK(ok != NULL && strcmp((char *)ok, "abcdef") == 0);
        free(ok);
    }
    free(packed);
}

static void
test_adler32_vectors(void)
{
    CHECK(kry_zlib_adler32((const unsigned char *)"", 0) == 0x1u);
    CHECK(kry_zlib_adler32((const unsigned char *)"a", 1) == 0x620062ul);
    CHECK(kry_zlib_adler32((const unsigned char *)"123456789", 9) ==
          0x91E01DEul);
    CHECK(kry_zlib_adler32((const unsigned char *)"Wikipedia", 9) ==
          0x11E60398ul);
}

int
main(void)
{
    test_detect();
    test_inflate_reference_streams();
    test_round_trip_sizes();
    test_binary_payload();
    test_corrupt_input();
    test_adler32_vectors();
    if(failures != 0) {
        fprintf(stderr, "kry_zlib: %d failure(s)\n", failures);
        return 1;
    }
    printf("kry_zlib ok\n");
    return 0;
}

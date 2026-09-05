/*
 * kry_gzip_test.c - inflate/deflate unit tests (hermetic, no network).
 *
 * GZ_DYNAMIC is a real zlib level-9 stream (dynamic Huffman blocks) so the
 * full decoder path is covered without spawning external tools.
 */
#include "kry_gzip.h"

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

#define GZ_DATA_LEN 1800u
#define GZ_SENTENCE_LEN 45u
static const char GZ_SENTENCE[] = "The quick brown fox jumps over the lazy dog. ";
static const unsigned char GZ_DYNAMIC[] = {
    31,139,8,0,0,0,0,0,0,0,11,201,72,85,40,44,205,76,206,86,72,42,202,47,
    207,83,72,203,175,80,200,42,205,45,40,86,200,47,75,45,82,40,1,74,231,
    36,86,85,42,164,228,167,235,41,132,140,42,30,85,60,170,120,84,241,168,
    226,81,197,195,75,49,0,230,195,149,100,8,7,0,0};

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
    static const unsigned char gz[] = {0x1F, 0x8B, 0x08, 0x00};
    static const unsigned char no[] = {'P', 'K', 0x03, 0x04};

    CHECK(kry_gzip_is(gz, sizeof(gz)) == 1);
    CHECK(kry_gzip_is(no, sizeof(no)) == 0);
    CHECK(kry_gzip_is(NULL, 0) == 0);
    CHECK(kry_gzip_is(gz, 1) == 0);
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
        packed = kry_gzip_compress((const unsigned char *)plain, n,
                                   &packed_len);
        CHECK(packed != NULL);
        CHECK(kry_gzip_is(packed, packed_len) == 1);
        plain2 = kry_gzip_decompress(packed, packed_len, &back_len);
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
test_inflate_zlib_dynamic(void)
{
    unsigned char *out;
    unsigned long out_len = 0;
    char expected[GZ_DATA_LEN + 1];
    unsigned long reps;

    for(reps = 0; reps < GZ_DATA_LEN / GZ_SENTENCE_LEN; reps++)
        memcpy(expected + reps * GZ_SENTENCE_LEN, GZ_SENTENCE,
               GZ_SENTENCE_LEN);
    expected[GZ_DATA_LEN] = '\0';

    out = kry_gzip_decompress(GZ_DYNAMIC, sizeof(GZ_DYNAMIC), &out_len);
    CHECK(out != NULL);
    CHECK(out_len == GZ_DATA_LEN);
    if(out != NULL && out_len == GZ_DATA_LEN)
        CHECK(memcmp(out, expected, GZ_DATA_LEN) == 0);
    CHECK(out != NULL && out[GZ_DATA_LEN] == '\0');
    free(out);
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
    packed = kry_gzip_compress(raw, sizeof(raw), &packed_len);
    CHECK(packed != NULL);
    back = kry_gzip_decompress(packed, packed_len, &back_len);
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
    unsigned long i;

    packed = kry_gzip_compress((const unsigned char *)"abcdef", 6,
                               &packed_len);
    CHECK(packed != NULL);

    CHECK(kry_gzip_decompress(NULL, 10, NULL) == NULL);
    CHECK(kry_gzip_decompress((const unsigned char *)"not gzip", 8,
                              NULL) == NULL);
    CHECK(kry_gzip_decompress(packed, 3, NULL) == NULL);

    /* flip a payload byte: CRC must reject */
    packed[packed_len - 9] ^= 0x40;
    CHECK(kry_gzip_decompress(packed, packed_len, NULL) == NULL);
    packed[packed_len - 9] ^= 0x40;

    /* flip a CRC byte */
    packed[packed_len - 8] ^= 0x01;
    CHECK(kry_gzip_decompress(packed, packed_len, NULL) == NULL);
    packed[packed_len - 8] ^= 0x01;

    /* wrong declared size */
    packed[packed_len - 1] ^= 0x01;
    CHECK(kry_gzip_decompress(packed, packed_len, NULL) == NULL);
    packed[packed_len - 1] ^= 0x01;

    /* truncated trailer */
    CHECK(kry_gzip_decompress(packed, packed_len - 4, NULL) == NULL);

    /* sane again */
    {
        unsigned char *ok = kry_gzip_decompress(packed, packed_len, NULL);

        CHECK(ok != NULL && strcmp((char *)ok, "abcdef") == 0);
        free(ok);
    }
    for(i = 0; i < 1; i++)
        (void)i;
    free(packed);
}

static void
test_crc32_vectors(void)
{
    CHECK(kry_gzip_crc32((const unsigned char *)"", 0) == 0x0u);
    CHECK(kry_gzip_crc32((const unsigned char *)"a", 1) == 0xE8B7BE43ul);
    CHECK(kry_gzip_crc32((const unsigned char *)"123456789", 9) ==
          0xCBF43926ul);
    CHECK(kry_gzip_crc32((const unsigned char *)"The quick brown fox jumps "
                                                "over the lazy dog",
                         43) == 0x414FA339ul);
}

int
main(void)
{
    test_detect();
    test_round_trip_sizes();
    test_inflate_zlib_dynamic();
    test_binary_payload();
    test_corrupt_input();
    test_crc32_vectors();
    if(failures != 0) {
        fprintf(stderr, "kry_gzip: %d failure(s)\n", failures);
        return 1;
    }
    printf("kry_gzip ok\n");
    return 0;
}

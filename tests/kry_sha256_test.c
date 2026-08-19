/*
 * kry_sha256_test.c - SHA-256 against FIPS 180-4 vectors, plus the file
 * hasher and hex comparison helpers kry_update relies on.
 */
#include "kry_sha256.h"

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
hash_text(const char *text, char out[65])
{
    KrySha256 ctx;
    unsigned char digest[32];

    kry_sha256_init(&ctx);
    kry_sha256_update(&ctx, text, strlen(text));
    kry_sha256_final(&ctx, digest);
    kry_sha256_hex(digest, out);
}

static void
test_vectors(void)
{
    char hex[65];

    hash_text("abc", hex);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223"
                      "b00361a396177a9cb410ff61f20015ad") == 0);

    hash_text("", hex);
    CHECK(strcmp(hex, "e3b0c44298fc1c149afbf4c8996fb924"
                      "27ae41e4649b934ca495991b7852b855") == 0);

    hash_text("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq", hex);
    CHECK(strcmp(hex, "248d6a61d20638b8e5c026930c3e6039"
                      "a33ce45964ff2167f6ecedd419db06c1") == 0);

    /* multi-block + update-in-pieces must equal the one-shot hash */
    {
        KrySha256 ctx;
        unsigned char digest[32];
        const char *text = "The quick brown fox jumps over the lazy dog";
        size_t total = strlen(text);
        size_t off;

        kry_sha256_init(&ctx);
        for(off = 0; off < total; off += 7)
            kry_sha256_update(&ctx, text + off,
                              total - off < 7 ? total - off : 7);
        kry_sha256_final(&ctx, digest);
        kry_sha256_hex(digest, hex);
        CHECK(strcmp(hex, "d7a8fbb307d7809469ca9abcb0082e4f"
                          "8d5651e46d3cdb762d02d0bf37c9e592") == 0);
    }
}

static void
test_file_hash(void)
{
    char path[256];
    char hex[65];
    FILE *f;

    snprintf(path, sizeof(path), "/tmp/kry_sha256_test.%d", (int)getpid());
    f = fopen(path, "wb");
    if(f == NULL)
        return;
    fputs("abc", f);
    fclose(f);

    CHECK(kry_sha256_file(path, hex) == 1);
    CHECK(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223"
                      "b00361a396177a9cb410ff61f20015ad") == 0);
    CHECK(kry_sha256_file("/tmp/definitely-not-here-kryon-sha", hex) == 0);
    remove(path);
}

static void
test_hex_equal(void)
{
    CHECK(kry_sha256_hex_equal("aa", "aa") == 1);
    CHECK(kry_sha256_hex_equal("ABCD", "abcd") == 1);
    CHECK(kry_sha256_hex_equal("abcd", "abce") == 0);
    CHECK(kry_sha256_hex_equal("abc", "abcd") == 0);
    CHECK(kry_sha256_hex_equal("", "") == 1);
    CHECK(kry_sha256_hex_equal(NULL, "aa") == 0);
    CHECK(kry_sha256_hex_equal("aa", NULL) == 0);
}

int
main(void)
{
    test_vectors();
    test_file_hash();
    test_hex_equal();
    if(failures == 0)
        printf("kry_sha256 tests passed\n");
    return failures == 0 ? 0 : 1;
}

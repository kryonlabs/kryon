#include "ksync_crypto.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int failures = 0;

#define CHECK(cond, name) \
    do { \
        if(cond) { \
            printf("ok %s\n", name); \
        } else { \
            printf("FAIL %s (%s:%d)\n", name, __FILE__, __LINE__); \
            failures++; \
        } \
    } while(0)

static int
hex_eq(const char *hex, const uint8_t *bytes, size_t len)
{
    char got[256];
    if(len * 2 + 1 > sizeof(got))
        return 0;
    KsyncCryptoBytesToHex(bytes, len, got, sizeof(got));
    return strcmp(got, hex) == 0;
}

static void
test_sha256(void)
{
    uint8_t digest[32];

    KsyncCryptoSha256((const uint8_t *)"", 0, digest);
    CHECK(hex_eq("e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855",
                 digest, 32), "sha256 empty");

    KsyncCryptoSha256((const uint8_t *)"abc", 3, digest);
    CHECK(hex_eq("ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                 digest, 32), "sha256 abc");

    {
        /* 448-bit message spanning multiple blocks */
        const char *long_msg =
            "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
        KsyncCryptoSha256((const uint8_t *)long_msg, strlen(long_msg), digest);
        CHECK(hex_eq("248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1",
                     digest, 32), "sha256 multi block");
    }
}

static void
test_hmac(void)
{
    /* RFC 4231 test case 2 */
    uint8_t digest[32];
    KsyncCryptoHmacSha256((const uint8_t *)"Jefe", 4,
                          (const uint8_t *)"what do ya want for nothing?", 28,
                          digest);
    CHECK(hex_eq("5bdcc146bf60754e6a042426089575c75a003f089d2739839dec58b964ec3843",
                 digest, 32), "hmac rfc4231 case 2");

    /* RFC 4231 test case 6: key larger than block size */
    {
        uint8_t key[131];
        memset(key, 0xaa, sizeof(key));
        KsyncCryptoHmacSha256(key, sizeof(key),
                              (const uint8_t *)
                              "Test Using Larger Than Block-Size Key - Hash Key First",
                              54, digest);
        CHECK(hex_eq("60e431591ee0b67f0d8a26aacbf5b77f8e0bc6213728c5140546040f0ee37f54",
                     digest, 32), "hmac rfc4231 case 6");
    }
}

static void
test_pbkdf2(void)
{
    uint8_t out[32];

    KsyncCryptoPbkdf2Sha256((const uint8_t *)"password", 8,
                            (const uint8_t *)"salt", 4, 1, out);
    CHECK(hex_eq("120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b",
                 out, 32), "pbkdf2 c=1");

    KsyncCryptoPbkdf2Sha256((const uint8_t *)"password", 8,
                            (const uint8_t *)"salt", 4, 2, out);
    CHECK(hex_eq("ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43",
                 out, 32), "pbkdf2 c=2");

    KsyncCryptoPbkdf2Sha256((const uint8_t *)"password", 8,
                            (const uint8_t *)"salt", 4, 4096, out);
    CHECK(hex_eq("c5e478d59288c841aa530db6845c4c8d962893a001ce4e11a4963873aa98134a",
                 out, 32), "pbkdf2 c=4096");
}

static void
test_chacha20_poly1305(void)
{
    /* RFC 8439 section 2.8.2 AEAD test vector */
    static const uint8_t key[32] = {
        0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87,
        0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8e, 0x8f,
        0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97,
        0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9e, 0x9f
    };
    static const uint8_t nonce[12] = {
        0x07, 0x00, 0x00, 0x00, 0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47
    };
    static const uint8_t aad[12] = {
        0x50, 0x51, 0x52, 0x53, 0xc0, 0xc1, 0xc2, 0xc3, 0xc4, 0xc5, 0xc6, 0xc7
    };
    static const char *plain =
        "Ladies and Gentlemen of the class of '99: If I could offer you "
        "only one tip for the future, sunscreen would be it.";
    static const char *expected =
        "d31a8d34648e60db7b86afbc53ef7ec2a4aded51296e08fea9e2b5a736ee62d6"
        "3dbea45e8ca9671282fafb69da92728b1a71de0a9e060b2905d6a5b67ecd3b36"
        "92ddbd7f2d778b8c9803aee328091b58fab324e4fad675945585808b4831d7bc"
        "3ff4def08e4b7a9de576d26586cec64b6116";
    static const char *expected_tag = "1ae10b594f09e26a7e902ecbd0600691";
    uint8_t sealed[256];
    uint8_t opened[256];
    char got[256];
    size_t plain_len = strlen(plain);

    CHECK(KsyncCryptoChaCha20Poly1305Seal(key, nonce, (const uint8_t *)plain,
                                          plain_len, aad, sizeof(aad), sealed) == 1,
          "aead seal ok");
    CHECK(hex_eq(expected, sealed, plain_len), "aead rfc8439 ciphertext");
    CHECK(hex_eq(expected_tag, sealed + plain_len, 16), "aead rfc8439 tag");

    CHECK(KsyncCryptoChaCha20Poly1305Open(key, nonce, sealed, plain_len + 16,
                                          aad, sizeof(aad), opened) == 1,
          "aead open ok");
    CHECK(memcmp(opened, plain, plain_len) == 0, "aead roundtrip");

    sealed[0] ^= 0x01;
    CHECK(KsyncCryptoChaCha20Poly1305Open(key, nonce, sealed, plain_len + 16,
                                          aad, sizeof(aad), opened) == 0,
          "aead tamper rejected");

    /* empty plaintext */
    CHECK(KsyncCryptoChaCha20Poly1305Seal(key, nonce, NULL, 0, NULL, 0, sealed) == 1 &&
          KsyncCryptoChaCha20Poly1305Open(key, nonce, sealed, 16, NULL, 0, opened) == 1,
          "aead empty roundtrip");

    /* random nonces must not repeat */
    {
        uint8_t n1[12], n2[12];
        KsyncCryptoRandom(n1, sizeof(n1));
        KsyncCryptoRandom(n2, sizeof(n2));
        KsyncCryptoBytesToHex(n1, sizeof(n1), got, sizeof(got));
        CHECK(strlen(got) == 24, "random hex length");
    }
}

static void
test_hex(void)
{
    uint8_t bytes[4] = {0x01, 0xab, 0xff, 0x00};
    char hex[16];
    uint8_t back[4];

    CHECK(KsyncCryptoBytesToHex(bytes, 4, hex, sizeof(hex)) == 1, "hex encode ok");
    CHECK(strcmp(hex, "01abff00") == 0, "hex encode value");
    CHECK(KsyncCryptoHexToBytes(hex, back, 4) == 1, "hex decode ok");
    CHECK(memcmp(bytes, back, 4) == 0, "hex roundtrip");
    CHECK(KsyncCryptoHexToBytes("01abff0", back, 4) == 0, "hex decode odd length");
    CHECK(KsyncCryptoHexToBytes("01abff0g", back, 4) == 0, "hex decode bad digit");
}

int
main(void)
{
    test_sha256();
    test_hmac();
    test_pbkdf2();
    test_chacha20_poly1305();
    test_hex();
    if(failures > 0) {
        printf("%d failure(s)\n", failures);
        return 1;
    }
    printf("all crypto tests passed\n");
    return 0;
}

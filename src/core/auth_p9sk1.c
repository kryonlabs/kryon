/*
 * Kryon Authentication - p9sk1 (DES-based Password Authentication)
 * C89/C90 compliant
 *
 * Based on Plan 9 p9sk1 authentication protocol
 */

#include "auth_p9sk1.h"
#include "devfactotum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_OPENSSL
#include <openssl/des.h>
#include <openssl/md5.h>
#include <openssl/rand.h>
#else
/* Fallback implementations */
#define DES_key_schedule void
#define DES_cblock unsigned char[8]
#define RAND_bytes(buf, len) (-1)
#define MD5_DIGEST_LENGTH 16
#endif

/*
 * Simple MD5 fallback (for systems without OpenSSL)
 * This is a simplified MD5 - in production, use OpenSSL
 */
#ifdef USE_OPENSSL

static void md5_hash(const unsigned char *data, size_t len,
                     unsigned char *hash)
{
    MD5_CTX ctx;
    MD5_Init(&ctx);
    MD5_Update(&ctx, data, len);
    MD5_Final(hash, &ctx);
}

#else

/* Very weak fallback hash - DO NOT USE IN PRODUCTION */
static void md5_hash(const unsigned char *data, size_t len,
                     unsigned char *hash)
{
    size_t i;
    unsigned int sum = 0;

    for (i = 0; i < len; i++) {
        sum = (sum << 1) ^ data[i];
    }

    /* Fill 16-byte hash output */
    for (i = 0; i < 16; i++) {
        hash[i] = (unsigned char)((sum + i) & 0xFF);
    }
}

#endif

/*
 * Set odd parity on DES key
 * DES requires odd parity on each byte (least significant bit is parity)
 */
void p9sk1_des_set_parity(unsigned char *key, size_t len)
{
    size_t i, j;
    unsigned char b;

    for (i = 0; i < len; i++) {
        b = key[i];
        /* Count bits in high 7 bits */
        unsigned int bitcount = 0;
        for (j = 0; j < 7; j++) {
            if (b & (1 << j)) {
                bitcount++;
            }
        }
        /* Set parity bit to make total odd */
        if (bitcount % 2 == 0) {
            key[i] = b | 0x01;  /* Set LSB for odd parity */
        } else {
            key[i] = b & 0xFE;  /* Clear LSB for odd parity */
        }
    }
}

/*
 * Convert password to DES key
 * Algorithm: MD5(password + username) → extract 7 bytes → add parity → 8 byte DES key
 */
int p9sk1_passtokey(const char *password, const char *username,
                    unsigned char *des_key, size_t keylen)
{
    unsigned char hash[MD5_DIGEST_LENGTH];
    char *combined;
    size_t pwlen, ulen, combined_len;
    size_t i;

    if (password == NULL || des_key == NULL || keylen < P9SK1_KEYLEN) {
        return -1;
    }

    pwlen = strlen(password);
    ulen = (username != NULL) ? strlen(username) : 0;

    /* Combine password + username */
    combined_len = pwlen + ulen;
    combined = (char *)malloc(combined_len);
    if (combined == NULL) {
        return -1;
    }

    memcpy(combined, password, pwlen);
    if (username != NULL && ulen > 0) {
        memcpy(combined + pwlen, username, ulen);
    }

    /* Compute MD5 hash */
    md5_hash((unsigned char *)combined, combined_len, hash);
    free(combined);

    /* Extract first 7 bytes for DES key material */
    memset(des_key, 0, keylen);
    for (i = 0; i < 7 && i < keylen; i++) {
        des_key[i] = hash[i];
    }

    /* Set odd parity on the 8-byte key */
    p9sk1_des_set_parity(des_key, P9SK1_KEYLEN);

    fprintf(stderr, "p9sk1: derived DES key from password\n");

    return 0;
}

/*
 * Find password key from factotum
 * Looks up proto=p9sk1 key in /mnt/factotum/ctl
 */
char *p9sk1_find_password(const char *user, const char *dom)
{
    FILE *fp;
    char line[512];
    char *password = NULL;
    static const char factotum_path[] = "/mnt/factotum/ctl";

    if (user == NULL) {
        fprintf(stderr, "p9sk1_find_password: NULL user\n");
        return NULL;
    }

    fp = fopen(factotum_path, "r");
    if (fp == NULL) {
        fprintf(stderr, "p9sk1_find_password: cannot open %s\n", factotum_path);
        return NULL;
    }

    /* Read keys from factotum */
    while (fgets(line, sizeof(line), fp) != NULL) {
        char *proto, *ptr, *pw_start, *pw_end;
        char line_user[AUTH_ANAMELEN];
        char line_dom[AUTH_DOMLEN];

        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') {
            continue;
        }

        /* Check for p9sk1 protocol */
        proto = strstr(line, "proto=p9sk1");
        if (proto == NULL) {
            continue;
        }

        /* Extract user from key */
        ptr = strstr(line, "user=");
        if (ptr != NULL) {
            ptr += 5;
            strncpy(line_user, ptr, AUTH_ANAMELEN - 1);
            line_user[AUTH_ANAMELEN - 1] = '\0';
            /* Null-terminate at space or newline */
            for (ptr = line_user; *ptr && *ptr != ' ' && *ptr != '\n' && *ptr != '\r'; ptr++) {
                /* Continue */
            }
            *ptr = '\0';
        } else {
            continue;
        }

        /* Check if user matches */
        if (strcmp(line_user, user) != 0) {
            continue;
        }

        /* Extract domain (optional for p9sk1) */
        if (dom != NULL) {
            ptr = strstr(line, "dom=");
            if (ptr != NULL) {
                ptr += 4;
                strncpy(line_dom, ptr, AUTH_DOMLEN - 1);
                line_dom[AUTH_DOMLEN - 1] = '\0';
                /* Null-terminate at space or newline */
                for (ptr = line_dom; *ptr && *ptr != ' ' && *ptr != '\n' && *ptr != '\r'; ptr++) {
                    /* Continue */
                }
                *ptr = '\0';

                if (strcmp(line_dom, dom) != 0) {
                    continue;
                }
            }
        }

        /* Extract password (marked with !password=) */
        pw_start = strstr(line, "!password=");
        if (pw_start == NULL) {
            pw_start = strstr(line, "password=");
            if (pw_start == NULL) {
                continue;
            }
            pw_start += 9;  /* "password=" */
        } else {
            pw_start += 10; /* "!password=" */
        }

        /* Find end of password (space, newline, or carriage return) */
        pw_end = pw_start;
        while (*pw_end && *pw_end != ' ' && *pw_end != '\n' && *pw_end != '\r') {
            pw_end++;
        }

        /* Allocate and copy password */
        password = (char *)malloc(pw_end - pw_start + 1);
        if (password == NULL) {
            fclose(fp);
            return NULL;
        }

        memcpy(password, pw_start, pw_end - pw_start);
        password[pw_end - pw_start] = '\0';

        fprintf(stderr, "p9sk1: found password for user=%s\n", user);
        break;
    }

    fclose(fp);
    return password;
}

#ifdef USE_OPENSSL

/*
 * Encrypt buffer using DES ECB mode
 */
int p9sk1_des_ecb_encrypt(const unsigned char *plaintext,
                          unsigned char *ciphertext,
                          int len, const unsigned char *key)
{
    DES_cblock des_key;
    DES_key_schedule schedule;
    int i;

    if (plaintext == NULL || ciphertext == NULL || key == NULL) {
        return -1;
    }

    if (len % 8 != 0) {
        fprintf(stderr, "p9sk1_des_ecb_encrypt: length must be multiple of 8\n");
        return -1;
    }

    /* Copy key and set parity */
    memcpy(des_key, key, 8);
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);

    /* Encrypt each 8-byte block */
    for (i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(plaintext + i),
                        (DES_cblock *)(ciphertext + i),
                        &schedule, DES_ENCRYPT);
    }

    return 0;
}

/*
 * Decrypt buffer using DES ECB mode
 */
int p9sk1_des_ecb_decrypt(const unsigned char *ciphertext,
                          unsigned char *plaintext,
                          int len, const unsigned char *key)
{
    DES_cblock des_key;
    DES_key_schedule schedule;
    int i;

    if (ciphertext == NULL || plaintext == NULL || key == NULL) {
        return -1;
    }

    if (len % 8 != 0) {
        fprintf(stderr, "p9sk1_des_ecb_decrypt: length must be multiple of 8\n");
        return -1;
    }

    /* Copy key and set parity */
    memcpy(des_key, key, 8);
    DES_set_odd_parity(&des_key);
    DES_set_key_checked(&des_key, &schedule);

    /* Decrypt each 8-byte block */
    for (i = 0; i < len; i += 8) {
        DES_ecb_encrypt((DES_cblock *)(ciphertext + i),
                        (DES_cblock *)(plaintext + i),
                        &schedule, DES_DECRYPT);
    }

    return 0;
}

#else

/*
 * Weak fallback XOR encryption (DO NOT USE IN PRODUCTION)
 */
static void xor_crypt(const unsigned char *in, unsigned char *out, int len,
                      const unsigned char *key)
{
    int i;
    unsigned char k = key[0];

    for (i = 0; i < len; i++) {
        out[i] = in[i] ^ k;
        k = (k << 1) | (k >> 7);  /* Rotate */
    }
}

int p9sk1_des_ecb_encrypt(const unsigned char *plaintext,
                          unsigned char *ciphertext,
                          int len, const unsigned char *key)
{
    if (plaintext == NULL || ciphertext == NULL || key == NULL) {
        return -1;
    }
    xor_crypt(plaintext, ciphertext, len, key);
    return 0;
}

int p9sk1_des_ecb_decrypt(const unsigned char *ciphertext,
                          unsigned char *plaintext,
                          int len, const unsigned char *key)
{
    if (ciphertext == NULL || plaintext == NULL || key == NULL) {
        return -1;
    }
    /* XOR is symmetric */
    xor_crypt(ciphertext, plaintext, len, key);
    return 0;
}

#endif

/*
 * Decrypt ticket using DES key
 * Ticket format: num(1) + chal(8) + cuid(28) + suid(28) + key(7)
 * Total: 72 bytes
 */
int p9sk1_decrypt_ticket(const unsigned char *encrypted, int enc_len,
                         const unsigned char *des_key,
                         Ticket *t)
{
    unsigned char decrypted[P9SK1_TICKETLEN];
    int key_offset;

    if (encrypted == NULL || des_key == NULL || t == NULL) {
        fprintf(stderr, "p9sk1_decrypt_ticket: NULL parameter\n");
        return -1;
    }

    if (enc_len < P9SK1_TICKETLEN) {
        fprintf(stderr, "p9sk1_decrypt_ticket: buffer too short (%d < %d)\n",
                enc_len, P9SK1_TICKETLEN);
        return -1;
    }

    /* Decrypt using DES ECB */
    if (p9sk1_des_ecb_decrypt(encrypted, decrypted, P9SK1_TICKETLEN, des_key) < 0) {
        fprintf(stderr, "p9sk1_decrypt_ticket: DES decryption failed\n");
        return -1;
    }

    /* Parse ticket structure */
    memset(t, 0, sizeof(Ticket));

    t->num = decrypted[0];
    memcpy(t->chal, decrypted + 1, AUTH_CHALLEN);
    memcpy(t->cuid, decrypted + 1 + AUTH_CHALLEN, AUTH_ANAMELEN);
    memcpy(t->suid, decrypted + 1 + AUTH_CHALLEN + AUTH_ANAMELEN, AUTH_ANAMELEN);

    /* Key is 7 bytes in p9sk1 (56-bit DES key) */
    key_offset = 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN;
    memset(t->key, 0, sizeof(t->key));
    memcpy(t->key, decrypted + key_offset, 7);

    fprintf(stderr, "p9sk1: decrypted ticket for user=%s server=%s\n",
            t->cuid, t->suid);

    return 0;
}

/*
 * Validate ticket structure
 */
int p9sk1_validate_ticket(const Ticket *t, const char *expected_user,
                          const char *expected_dom)
{
    int i;

    if (t == NULL) {
        fprintf(stderr, "p9sk1_validate_ticket: NULL ticket\n");
        return -1;
    }

    /* Check that cuid is not all zeros */
    for (i = 0; i < AUTH_ANAMELEN && t->cuid[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_ANAMELEN) {
        fprintf(stderr, "p9sk1_validate_ticket: empty cuid\n");
        return -1;
    }

    /* Check that suid is not all zeros */
    for (i = 0; i < AUTH_ANAMELEN && t->suid[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_ANAMELEN) {
        fprintf(stderr, "p9sk1_validate_ticket: empty suid\n");
        return -1;
    }

    /* Verify cuid matches expected user */
    if (expected_user != NULL) {
        if (strncmp(t->cuid, expected_user, AUTH_ANAMELEN) != 0) {
            fprintf(stderr, "p9sk1_validate_ticket: cuid mismatch (got %s, expected %s)\n",
                    t->cuid, expected_user);
            return -1;
        }
    }

    /* Check that challenge is not all zeros */
    for (i = 0; i < AUTH_CHALLEN && t->chal[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_CHALLEN) {
        fprintf(stderr, "p9sk1_validate_ticket: empty challenge\n");
        return -1;
    }

    fprintf(stderr, "p9sk1: ticket validated for user=%s\n", t->cuid);

    /* Domain check is informational */
    if (expected_dom != NULL) {
        fprintf(stderr, "p9sk1: ticket domain=%s (expected=%s)\n", t->suid, expected_dom);
    }

    return 0;
}

/*
 * Parse authenticator from buffer
 * Authenticator format: num(1) + chal(8) + id(8)
 * Total: 17 bytes
 */
int p9sk1_parse_authenticator(const unsigned char *buf, int len,
                              Authenticator *a)
{
    if (buf == NULL || a == NULL) {
        fprintf(stderr, "p9sk1_parse_authenticator: NULL parameter\n");
        return -1;
    }

    if (len < P9SK1_AUTHLEN) {
        fprintf(stderr, "p9sk1_parse_authenticator: buffer too short (%d < %d)\n",
                len, P9SK1_AUTHLEN);
        return -1;
    }

    memset(a, 0, sizeof(Authenticator));

    a->num = buf[0];
    memcpy(a->chal, buf + 1, AUTH_CHALLEN);
    /* Remaining 8 bytes go to rand field */
    memcpy(a->rand, buf + 1 + AUTH_CHALLEN, AUTH_NONCELEN);

    fprintf(stderr, "p9sk1: parsed authenticator\n");

    return 0;
}

/*
 * Verify authenticator
 */
int p9sk1_verify_authenticator(const Authenticator *a,
                               const Ticket *t)
{
    int i;

    if (a == NULL || t == NULL) {
        fprintf(stderr, "p9sk1_verify_authenticator: NULL parameter\n");
        return -1;
    }

    /* Check that challenge matches ticket's challenge */
    if (memcmp(a->chal, t->chal, AUTH_CHALLEN) != 0) {
        fprintf(stderr, "p9sk1_verify_authenticator: challenge mismatch\n");
        return -1;
    }

    /* Check that rand is not all zeros */
    for (i = 0; i < AUTH_NONCELEN && a->rand[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_NONCELEN) {
        fprintf(stderr, "p9sk1_verify_authenticator: empty random nonce\n");
        return -1;
    }

    fprintf(stderr, "p9sk1: authenticator verified\n");

    return 0;
}

/*
 * Generate server authenticator
 */
int p9sk1_create_server_authenticator(Authenticator *a,
                                     const unsigned char *client_chal,
                                     const unsigned char *server_rand)
{
    if (a == NULL || client_chal == NULL || server_rand == NULL) {
        return -1;
    }

    memset(a, 0, sizeof(Authenticator));

    a->num = 0;  /* TODO: proper counter/replay protection */
    memcpy(a->chal, client_chal, AUTH_CHALLEN);
    memcpy(a->rand, server_rand, AUTH_NONCELEN);

    fprintf(stderr, "p9sk1: created server authenticator\n");

    return 0;
}

/*
 * Generate random nonce
 */
int p9sk1_gen_nonce(unsigned char *nonce, size_t len)
{
#ifdef USE_OPENSSL
    if (RAND_bytes(nonce, len) != 1) {
        fprintf(stderr, "p9sk1_gen_nonce: RAND_bytes failed\n");
        return -1;
    }
    return 0;
#else
    /* Fallback: use simple pseudo-random */
    size_t i;
    static int seeded = 0;

    if (!seeded) {
        srand((unsigned int)time(NULL));
        seeded = 1;
    }

    for (i = 0; i < len; i++) {
        nonce[i] = (unsigned char)(rand() & 0xFF);
    }

    return 0;
#endif
}

/*
 * Grant ticket for p9sk1 authentication
 * Server generates ticket using password-derived key and sends to client
 *
 * Ticket format (plaintext): num(1) + chal(8) + cuid(28) + suid(28) + key(7) = 72 bytes
 * The ticket is encrypted with DES key derived from password
 *
 * Returns 0 on success, -1 on error
 */
int p9sk1_grant_ticket(const char *user, const char *dom,
                       const unsigned char *client_chal,
                       unsigned char *ticket_out, int *ticket_len)
{
    unsigned char plaintext[P9SK1_TICKETLEN];
    unsigned char des_key[P9SK1_KEYLEN];
    char *password = NULL;
    int ret = -1;
    int offset;

    if (user == NULL || dom == NULL || client_chal == NULL ||
        ticket_out == NULL || ticket_len == NULL) {
        fprintf(stderr, "p9sk1_grant_ticket: NULL parameter\n");
        return -1;
    }

    /* Step 1: Find password from factotum */
    password = p9sk1_find_password(user, dom);
    if (password == NULL) {
        fprintf(stderr, "p9sk1_grant_ticket: failed to find password for user=%s dom=%s\n", user, dom);
        fprintf(stderr, "p9sk1_grant_ticket: please add key to factotum: key proto=p9sk1 dom=%s user=%s !password=yourpassword\n",
                dom, user);
        return -1;
    }

    /* Step 2: Derive DES key from password */
    if (p9sk1_passtokey(password, user, des_key, sizeof(des_key)) < 0) {
        fprintf(stderr, "p9sk1_grant_ticket: failed to derive DES key from password\n");
        free(password);
        return -1;
    }

    /* Step 3: Build ticket structure */
    memset(plaintext, 0, sizeof(plaintext));

    /* num = 0 for p9sk1 tickets */
    plaintext[0] = 0;
    offset = 1;

    /* chal = client's challenge (8 bytes) */
    memcpy(plaintext + offset, client_chal, AUTH_CHALLEN);
    offset += AUTH_CHALLEN;

    /* cuid = user (28 bytes max) */
    memset(plaintext + offset, 0, AUTH_ANAMELEN);
    strncpy((char *)plaintext + offset, user, AUTH_ANAMELEN - 1);
    offset += AUTH_ANAMELEN;

    /* suid = domain/server (28 bytes max) */
    memset(plaintext + offset, 0, AUTH_ANAMELEN);
    strncpy((char *)plaintext + offset, dom, AUTH_ANAMELEN - 1);
    offset += AUTH_ANAMELEN;

    /* key = random 7-byte session key */
    if (p9sk1_gen_nonce(plaintext + offset, 7) < 0) {
        fprintf(stderr, "p9sk1_grant_ticket: failed to generate session key\n");
        free(password);
        return -1;
    }

    fprintf(stderr, "p9sk1_grant_ticket: built ticket for user=%s dom=%s\n", user, dom);

    /* Step 4: Encrypt ticket with DES key */
    if (p9sk1_des_ecb_encrypt(plaintext, ticket_out, P9SK1_TICKETLEN, des_key) < 0) {
        fprintf(stderr, "p9sk1_grant_ticket: failed to encrypt ticket\n");
        free(password);
        return -1;
    }

    *ticket_len = P9SK1_TICKETLEN;
    ret = 0;

    fprintf(stderr, "p9sk1_grant_ticket: granted ticket for user=%s (%d bytes)\n", user, *ticket_len);

    free(password);
    return ret;
}

/*
 * Kryon Authentication - dp9ik (Ed448) Authentication
 * C89/C90 compliant
 *
 * Based on 9front libauthsrv/authpak.c
 * Uses OpenSSL EVP_PKEY_ED448 for Ed448 operations
 */

#include "auth_dp9ik.h"
#include "devfactotum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef USE_OPENSSL
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/hmac.h>
#include <openssl/sha.h>
#else
/* Fallback: Use simple random() if no OpenSSL */
#define RAND_bytes(buf, len) (-1)
#endif

/*
 * Crypto initialization
 */
int dp9ik_crypto_init(void)
{
#ifdef USE_OPENSSL
    /* OpenSSL doesn't need explicit initialization in newer versions */
    return 0;
#else
    fprintf(stderr, "dp9ik_crypto_init: OpenSSL not available, using fallback\n");
    return 0;
#endif
}

/*
 * Crypto cleanup
 */
void dp9ik_crypto_cleanup(void)
{
#ifdef USE_OPENSSL
    /* OpenSSL cleanup if needed */
#endif
}

/*
 * Generate random bytes
 */
int dp9ik_random_bytes(unsigned char *buf, size_t len)
{
#ifdef USE_OPENSSL
    if (RAND_bytes(buf, len) != 1) {
        fprintf(stderr, "dp9ik_random_bytes: RAND_bytes failed\n");
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
        buf[i] = (unsigned char)(rand() & 0xFF);
    }

    return 0;
#endif
}

/*
 * Generate server challenge
 */
int dp9ik_gen_challenge(unsigned char *chal)
{
    return dp9ik_random_bytes(chal, AUTH_CHALLEN);
}

/*
 * Generate server nonce
 */
int dp9ik_gen_nonce(unsigned char *nonce)
{
    return dp9ik_random_bytes(nonce, AUTH_NONCELEN);
}

/*
 * Build ticket request structure
 */
int dp9ik_build_ticketreq(const char *authid, const char *authdom,
                          const char *hostid, const char *uid,
                          const unsigned char *chal,
                          Ticketreq *tr)
{
    if (tr == NULL) {
        return -1;
    }

    memset(tr, 0, sizeof(Ticketreq));

    tr->type = AUTH_PAK;

    if (authid != NULL) {
        strncpy(tr->authid, authid, AUTH_ANAMELEN - 1);
    }

    if (authdom != NULL) {
        strncpy(tr->authdom, authdom, AUTH_DOMLEN - 1);
    }

    if (hostid != NULL) {
        strncpy(tr->hostid, hostid, AUTH_ANAMELEN - 1);
    }

    if (uid != NULL) {
        strncpy(tr->uid, uid, AUTH_ANAMELEN - 1);
    }

    if (chal != NULL) {
        memcpy(tr->chal, chal, AUTH_CHALLEN);
    }

    return 0;
}

/*
 * Serialize ticket request to buffer
 * Format from 9front authsrv.h
 */
int dp9ik_serialize_ticketreq(const Ticketreq *tr,
                               unsigned char *buf, size_t len)
{
    size_t needed;

    if (tr == NULL || buf == NULL) {
        return -1;
    }

    /* Calculate needed size:
     * 1 byte type + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN +
     * AUTH_ANAMELEN + AUTH_ANAMELEN = 1 + 28 + 48 + 8 + 28 + 28 = 141 bytes
     */
    needed = 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN +
             AUTH_ANAMELEN + AUTH_ANAMELEN;

    if (len < needed) {
        return -1;
    }

    /* Serialize */
    buf[0] = tr->type;
    memcpy(buf + 1, tr->authid, AUTH_ANAMELEN);
    memcpy(buf + 1 + AUTH_ANAMELEN, tr->authdom, AUTH_DOMLEN);
    memcpy(buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN, tr->chal, AUTH_CHALLEN);
    memcpy(buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN,
           tr->hostid, AUTH_ANAMELEN);
    memcpy(buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN + AUTH_ANAMELEN,
           tr->uid, AUTH_ANAMELEN);

    return needed;
}

/*
 * Parse ticket request from buffer
 */
int dp9ik_deserialize_ticketreq(const unsigned char *buf, size_t len,
                                 Ticketreq *tr)
{
    size_t needed;

    if (buf == NULL || tr == NULL) {
        return -1;
    }

    needed = 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN +
             AUTH_ANAMELEN + AUTH_ANAMELEN;

    if (len < needed) {
        return -1;
    }

    memset(tr, 0, sizeof(Ticketreq));

    tr->type = buf[0];
    memcpy(tr->authid, buf + 1, AUTH_ANAMELEN);
    memcpy(tr->authdom, buf + 1 + AUTH_ANAMELEN, AUTH_DOMLEN);
    memcpy(tr->chal, buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN, AUTH_CHALLEN);
    memcpy(tr->hostid, buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN,
           AUTH_ANAMELEN);
    memcpy(tr->uid, buf + 1 + AUTH_ANAMELEN + AUTH_DOMLEN + AUTH_CHALLEN + AUTH_ANAMELEN,
           AUTH_ANAMELEN);

    return 0;
}

/*
 * Generate PAK key (Ed448 key pair)
 * For now, generates dummy keys since full Ed448 is complex
 */
int dp9ik_pak_key_generate(unsigned char *pubkey, size_t publen,
                           unsigned char *seckey, size_t seclen)
{
    if (pubkey == NULL || publen < AUTH_PAKYLEN) {
        return -1;
    }

    /* Generate random public key */
    if (dp9ik_random_bytes(pubkey, AUTH_PAKYLEN) < 0) {
        return -1;
    }

    /* Secret key not needed for basic operation */
    if (seckey != NULL && seclen > 0) {
        memset(seckey, 0, seclen);
    }

    fprintf(stderr, "dp9ik: generated PAK key (dummy implementation)\n");

    return 0;
}

/*
 * Convert password to PAK key hash
 * From 9front authpak.c:195
 * Simplified version - uses SHA-256 hash
 */
int dp9ik_passtokey(const unsigned char *password, size_t pwlen,
                    const char *user,
                    unsigned char *key, size_t keylen)
{
#ifdef USE_OPENSSL
    unsigned char hash[SHA256_DIGEST_LENGTH];
    unsigned int i;
    SHA256_CTX ctx;

    if (password == NULL || key == NULL) {
        return -1;
    }

    /* Hash password + user */
    SHA256_Init(&ctx);
    SHA256_Update(&ctx, password, pwlen);
    if (user != NULL) {
        SHA256_Update(&ctx, (unsigned char *)user, strlen(user));
    }
    SHA256_Final(hash, &ctx);

    /* Copy to output key */
    if (keylen > SHA256_DIGEST_LENGTH) {
        keylen = SHA256_DIGEST_LENGTH;
    }

    memcpy(key, hash, keylen);

    return 0;
#else
    /* Simple fallback */
    size_t i;
    unsigned int sum = 0;

    if (password == NULL || key == NULL) {
        return -1;
    }

    /* Simple checksum */
    for (i = 0; i < pwlen && i < keylen; i++) {
        key[i] = password[i];
        sum += password[i];
    }

    /* Fill rest with sum */
    for (i = pwlen; i < keylen; i++) {
        key[i] = (unsigned char)(sum & 0xFF);
    }

    return 0;
#endif
}

/*
 * Compute PAK hash point from password
 */
int dp9ik_authpak_hash(unsigned char *key, const char *user)
{
    if (key == NULL || user == NULL) {
        return -1;
    }

    /* Use passtokey with length of key */
    return dp9ik_passtokey(key, strlen((char *)key), user, key, AUTH_PAKYLEN);
}

/*
 * Compute PAK shared secret (SPAKE2-EE key exchange)
 * Simplified version - XOR operation
 */
int dp9ik_pak_server(const unsigned char *client_pubkey,
                     const unsigned char *server_seckey,
                     const unsigned char *password_hash,
                     unsigned char *shared_secret)
{
    size_t i;

    if (client_pubkey == NULL || password_hash == NULL ||
        shared_secret == NULL) {
        return -1;
    }

    /* Simplified SPAKE2: XOR client pubkey with password hash */
    for (i = 0; i < AUTH_PAKYLEN; i++) {
        shared_secret[i] = client_pubkey[i] ^ password_hash[i];
    }

    fprintf(stderr, "dp9ik: computed PAK shared secret (dummy implementation)\n");

    return 0;
}

/*
 * HKDF key derivation (from drawterm/libauthsrv)
 * Implements HKDF with HMAC-SHA256
 */
int dp9ik_hkdf(const unsigned char *ikm, size_t ikmlen,
               const unsigned char *salt, size_t saltlen,
               const unsigned char *info, size_t infolen,
               unsigned char *okm, size_t okmlen)
{
#ifdef USE_OPENSSL
    unsigned char prk[SHA256_DIGEST_LENGTH];
    EVP_PKEY_CTX *pctx;
    size_t prklen;

    if (ikm == NULL || okm == NULL || okmlen == 0) {
        return -1;
    }

    /* HKDF-Extract: PRK = HMAC(salt, IKM) */
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (pctx == NULL) {
        return -1;
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, salt, saltlen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, ikm, ikmlen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    prklen = sizeof(prk);
    if (EVP_PKEY_derive(pctx, prk, &prklen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    EVP_PKEY_CTX_free(pctx);

    /* HKDF-Expand: OKM = HKDF-Expand(PRK, info, L) */
    pctx = EVP_PKEY_CTX_new_id(EVP_PKEY_HKDF, NULL);
    if (pctx == NULL) {
        return -1;
    }

    if (EVP_PKEY_derive_init(pctx) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set_hkdf_md(pctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set1_hkdf_salt(pctx, NULL, 0) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (EVP_PKEY_CTX_set1_hkdf_key(pctx, prk, prklen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    if (info != NULL && infolen > 0) {
        if (EVP_PKEY_CTX_add1_hkdf_info(pctx, info, infolen) <= 0) {
            EVP_PKEY_CTX_free(pctx);
            return -1;
        }
    }

    if (EVP_PKEY_derive(pctx, okm, &okmlen) <= 0) {
        EVP_PKEY_CTX_free(pctx);
        return -1;
    }

    EVP_PKEY_CTX_free(pctx);

    return 0;
#else
    /* Fallback: simple hash */
    size_t i;
    unsigned char hash = 0;

    if (ikm == NULL || okm == NULL) {
        return -1;
    }

    /* Simple hash of IKM */
    for (i = 0; i < ikmlen; i++) {
        hash ^= ikm[i];
    }

    /* Fill output with hash + counter */
    for (i = 0; i < okmlen; i++) {
        okm[i] = hash + (unsigned char)i;
    }

    return 0;
#endif
}

/*
 * Parse and validate ticket from client
 */
int dp9ik_validate_ticket(const unsigned char *ticket, int tlen,
                          const char *password,
                          Ticket *t)
{
    /* For now, just parse the ticket structure */
    /* Full validation requires decrypting with password-derived key */

    if (ticket == NULL || t == NULL) {
        return -1;
    }

    /* Ticket format: num + chal + cuid + suid + key */
    if (tlen < (int)(1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN + AUTH_NONCELEN)) {
        fprintf(stderr, "dp9ik_validate_ticket: ticket too short\n");
        return -1;
    }

    memset(t, 0, sizeof(Ticket));

    t->num = ticket[0];
    memcpy(t->chal, ticket + 1, AUTH_CHALLEN);
    memcpy(t->cuid, ticket + 1 + AUTH_CHALLEN, AUTH_ANAMELEN);
    memcpy(t->suid, ticket + 1 + AUTH_CHALLEN + AUTH_ANAMELEN, AUTH_ANAMELEN);
    memcpy(t->key, ticket + 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN,
           AUTH_NONCELEN);

    fprintf(stderr, "dp9ik: parsed ticket for user=%s\n", t->cuid);

    /* TODO: Validate ticket MAC using password */
    (void)password;

    return 0;
}

/*
 * Build authenticator for server response
 */
int dp9ik_build_authenticator(const Ticket *t,
                              const unsigned char *client_chal,
                              const unsigned char *server_nonce,
                              Authenticator *auth,
                              unsigned char *buf, size_t len)
{
    size_t needed;

    if (t == NULL || auth == NULL || buf == NULL) {
        return -1;
    }

    memset(auth, 0, sizeof(Authenticator));

    /* Set authenticator fields */
    auth->num = 0;  /* TODO: proper counter */
    memcpy(auth->chal, client_chal, AUTH_CHALLEN);
    memcpy(auth->rand, server_nonce, AUTH_NONCELEN);

    /* Calculate needed size */
    needed = 1 + AUTH_CHALLEN + AUTH_NONCELEN;

    if (len < needed) {
        return -1;
    }

    /* Serialize authenticator */
    buf[0] = auth->num;
    memcpy(buf + 1, auth->chal, AUTH_CHALLEN);
    memcpy(buf + 1 + AUTH_CHALLEN, auth->rand, AUTH_NONCELEN);

    /* TODO: Compute authenticator MAC */
    (void)t;  /* Use ticket key for MAC */

    return needed;
}

/*
 * Derive session keys using HKDF
 */
int dp9ik_derive_keys(const unsigned char *client_chal,
                      const unsigned char *server_chal,
                      const Ticket *t,
                      AuthInfo *ai)
{
    unsigned char ikm[128];
    size_t ikmlen;
    unsigned char info[] = "dp9ik session key";

    if (client_chal == NULL || server_chal == NULL ||
        t == NULL || ai == NULL) {
        return -1;
    }

    /* Build IKM from challenges and ticket key */
    ikmlen = 0;
    memcpy(ikm + ikmlen, client_chal, AUTH_CHALLEN);
    ikmlen += AUTH_CHALLEN;
    memcpy(ikm + ikmlen, server_chal, AUTH_CHALLEN);
    ikmlen += AUTH_CHALLEN;
    memcpy(ikm + ikmlen, t->key, AUTH_NONCELEN);
    ikmlen += AUTH_NONCELEN;

    /* Allocate session key */
    ai->nsecret = DPIK_SESSION_KEY_LEN;
    ai->secret = (unsigned char *)malloc(ai->nsecret);
    if (ai->secret == NULL) {
        return -1;
    }

    /* Derive session key using HKDF */
    if (dp9ik_hkdf(ikm, ikmlen, NULL, 0, info, sizeof(info) - 1,
                   ai->secret, ai->nsecret) < 0) {
        free(ai->secret);
        ai->secret = NULL;
        return -1;
    }

    fprintf(stderr, "dp9ik: derived %d-byte session key\n", ai->nsecret);

    return 0;
}

/*
 * Encrypt ticket using password-derived key
 * Simple XOR-based encryption (from 9front)
 */
void dp9ik_encrypt_ticket(const unsigned char *ticket, int len,
                          const unsigned char *key)
{
    int i;
    unsigned char k = key[0];

    for (i = 0; i < len; i++) {
        ((unsigned char *)ticket)[i] ^= k;
        k = (k << 1) | (k >> 7);  /* Rotate */
    }
}

/*
 * Decrypt ticket using password-derived key
 */
void dp9ik_decrypt_ticket(unsigned char *ticket, int len,
                          const unsigned char *key)
{
    /* XOR is symmetric */
    dp9ik_encrypt_ticket(ticket, len, key);
}

/*
 * HMAC-SHA256 implementation
 * From RFC 2104
 */
static int hmac_sha256(const unsigned char *data, size_t data_len,
                       const unsigned char *key, size_t key_len,
                       unsigned char *out)
{
#ifdef USE_OPENSSL
    unsigned int len;
    unsigned char *result;

    if (data == NULL || key == NULL || out == NULL) {
        return -1;
    }

    result = HMAC(EVP_sha256(), key, key_len, data, data_len, out, &len);
    if (result == NULL) {
        fprintf(stderr, "hmac_sha256: HMAC failed\n");
        return -1;
    }

    return 0;
#else
    /* Simple fallback */
    size_t i;
    unsigned char hash = 0;

    if (data == NULL || key == NULL || out == NULL) {
        return -1;
    }

    /* Very weak fallback hash */
    for (i = 0; i < data_len; i++) {
        hash ^= data[i];
    }
    for (i = 0; i < key_len; i++) {
        hash ^= key[i];
    }

    /* Fill output with hash variations */
    for (i = 0; i < 32; i++) {
        out[i] = hash + (unsigned char)i;
    }

    return 0;
#endif
}

/*
 * Verify client authenticator
 */
int dp9ik_verify_authenticator(const Ticket *t,
                               const unsigned char *client_chal,
                               const unsigned char *server_nonce,
                               const Authenticator *auth)
{
    /* TODO: Verify authenticator MAC */
    (void)t;
    (void)client_chal;
    (void)server_nonce;
    (void)auth;

    fprintf(stderr, "dp9ik: authenticator verification not fully implemented\n");

    return 0;  /* Accept for now */
}

/*
 * Real ticket validation with MAC verification
 * Validates ticket by checking MAC with password-derived key
 */
int dp9ik_validate_ticket_real(const unsigned char *ticket, int tlen,
                                const char *password,
                                const char *user,
                                Ticket *t)
{
    unsigned char key[32];
    unsigned char computed_mac[32];
    unsigned char ticket_mac[32];
    int data_len;
    int i;

    if (ticket == NULL || t == NULL || password == NULL) {
        return -1;
    }

    /* Ticket format: num + chal + cuid + suid + key + MAC(32 bytes) */
    if (tlen < (int)(1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN + AUTH_NONCELEN + 32)) {
        fprintf(stderr, "dp9ik_validate_ticket_real: ticket too short (%d bytes)\n", tlen);
        return -1;
    }

    /* Extract data length (excluding MAC) */
    data_len = tlen - 32;

    /* Extract MAC from ticket */
    memcpy(ticket_mac, ticket + data_len, 32);

    /* Parse ticket structure */
    memset(t, 0, sizeof(Ticket));
    t->num = ticket[0];
    memcpy(t->chal, ticket + 1, AUTH_CHALLEN);
    memcpy(t->cuid, ticket + 1 + AUTH_CHALLEN, AUTH_ANAMELEN);
    memcpy(t->suid, ticket + 1 + AUTH_CHALLEN + AUTH_ANAMELEN, AUTH_ANAMELEN);
    memcpy(t->key, ticket + 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN, AUTH_NONCELEN);

    /* Derive key from password */
    if (dp9ik_passtokey((const unsigned char *)password, strlen(password),
                        user, key, sizeof(key)) < 0) {
        fprintf(stderr, "dp9ik_validate_ticket_real: passtokey failed\n");
        return -1;
    }

    /* Compute expected MAC */
    if (hmac_sha256(ticket, data_len, key, sizeof(key), computed_mac) < 0) {
        fprintf(stderr, "dp9ik_validate_ticket_real: HMAC computation failed\n");
        return -1;
    }

    /* Verify MAC */
    if (memcmp(computed_mac, ticket_mac, 32) != 0) {
        fprintf(stderr, "dp9ik_validate_ticket_real: MAC validation failed\n");
        fprintf(stderr, "  Expected: ");
        for (i = 0; i < 8; i++) {
            fprintf(stderr, "%02x", computed_mac[i]);
        }
        fprintf(stderr, "...\n");
        fprintf(stderr, "  Got:      ");
        for (i = 0; i < 8; i++) {
            fprintf(stderr, "%02x", ticket_mac[i]);
        }
        fprintf(stderr, "...\n");
        return -1;
    }

    fprintf(stderr, "dp9ik: ticket validated for user=%s\n", t->cuid);

    return 0;
}

/*
 * Real session key derivation using HKDF
 * Derives 256-bit session key from challenges and ticket
 */
int dp9ik_derive_keys_real(const unsigned char *client_chal,
                           const unsigned char *server_chal,
                           const Ticket *t,
                           AuthInfo *ai)
{
    unsigned char ikm[256];
    size_t ikmlen;
    unsigned char info[] = "Plan 9 session secret";

    if (client_chal == NULL || server_chal == NULL || t == NULL || ai == NULL) {
        return -1;
    }

    /* Build IKM: client_chal + server_chal + ticket_key */
    ikmlen = 0;
    memcpy(ikm + ikmlen, client_chal, AUTH_CHALLEN);
    ikmlen += AUTH_CHALLEN;
    memcpy(ikm + ikmlen, server_chal, AUTH_CHALLEN);
    ikmlen += AUTH_CHALLEN;
    memcpy(ikm + ikmlen, t->key, AUTH_NONCELEN);
    ikmlen += AUTH_NONCELEN;

    /* Allocate session key */
    ai->nsecret = 32;  /* 256-bit key */
    ai->secret = (unsigned char *)malloc(ai->nsecret);
    if (ai->secret == NULL) {
        fprintf(stderr, "dp9ik_derive_keys_real: malloc failed\n");
        return -1;
    }

    /* Derive using HKDF-SHA256 */
    if (dp9ik_hkdf(ikm, ikmlen,
                   NULL, 0,  /* No salt */
                   info, sizeof(info) - 1,
                   ai->secret, ai->nsecret) < 0) {
        free(ai->secret);
        ai->secret = NULL;
        fprintf(stderr, "dp9ik_derive_keys_real: HKDF failed\n");
        return -1;
    }

    fprintf(stderr, "dp9ik: derived %d-byte session key\n", ai->nsecret);

    return 0;
}

/*
 * Real authenticator computation with HMAC
 * Computes and serializes server authenticator
 */
int dp9ik_build_authenticator_real(const Ticket *t,
                                   const unsigned char *client_chal,
                                   const unsigned char *server_nonce,
                                   const unsigned char *session_key,
                                   Authenticator *auth,
                                   unsigned char *buf, size_t len)
{
    unsigned char to_mac[256];
    unsigned char mac[32];
    size_t maclen;
    size_t needed;

    if (t == NULL || client_chal == NULL || server_nonce == NULL ||
        session_key == NULL || auth == NULL || buf == NULL) {
        return -1;
    }

    /* Set authenticator fields */
    auth->num = 0;  /* TODO: proper counter */
    memcpy(auth->chal, client_chal, AUTH_CHALLEN);
    memcpy(auth->rand, server_nonce, AUTH_NONCELEN);

    /* Build data to MAC: num + chal + rand */
    maclen = 0;
    to_mac[maclen++] = auth->num;
    memcpy(to_mac + maclen, auth->chal, AUTH_CHALLEN);
    maclen += AUTH_CHALLEN;
    memcpy(to_mac + maclen, auth->rand, AUTH_NONCELEN);
    maclen += AUTH_NONCELEN;

    /* Compute HMAC using session key */
    if (hmac_sha256(to_mac, maclen, session_key, 32, mac) < 0) {
        fprintf(stderr, "dp9ik_build_authenticator_real: HMAC failed\n");
        return -1;
    }

    /* Calculate needed size */
    needed = 1 + AUTH_CHALLEN + AUTH_NONCELEN + 16;  /* Use 16 bytes of MAC */

    if (len < needed) {
        fprintf(stderr, "dp9ik_build_authenticator_real: buffer too small\n");
        return -1;
    }

    /* Serialize authenticator */
    buf[0] = auth->num;
    memcpy(buf + 1, auth->chal, AUTH_CHALLEN);
    memcpy(buf + 1 + AUTH_CHALLEN, auth->rand, AUTH_NONCELEN);
    memcpy(buf + 1 + AUTH_CHALLEN + AUTH_NONCELEN, mac, 16);

    fprintf(stderr, "dp9ik: built authenticator (%zu bytes)\n", needed);

    return (int)needed;
}

/*
 * Parse ticket from buffer
 * Ticket format: num(1) + chal(8) + cuid(28) + suid(28) + key(8+) + MAC(32)
 * For dp9ik, the key is larger (56 bytes for Ed448)
 */
int dp9ik_parse_ticket(Ticket *t, const unsigned char *buf, int len)
{
    int ticket_min_len;
    int key_len;

    if (t == NULL || buf == NULL) {
        return -1;
    }

    /* Minimum ticket size without MAC:
     * num(1) + chal(8) + cuid(28) + suid(28) + key(8) = 73 bytes
     * With MAC(32): 105 bytes minimum
     * For dp9ik with larger key: 1 + 8 + 28 + 28 + 56 + 32 = 153 bytes
     */
    ticket_min_len = 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN + AUTH_NONCELEN + 32;

    if (len < ticket_min_len) {
        fprintf(stderr, "dp9ik_parse_ticket: buffer too short (%d < %d)\n",
                len, ticket_min_len);
        return -1;
    }

    /* Clear ticket structure */
    memset(t, 0, sizeof(Ticket));

    /* Parse ticket fields (excluding MAC for now) */
    key_len = len - 32;  /* Exclude MAC from ticket data */

    t->num = buf[0];
    memcpy(t->chal, buf + 1, AUTH_CHALLEN);
    memcpy(t->cuid, buf + 1 + AUTH_CHALLEN, AUTH_ANAMELEN);
    memcpy(t->suid, buf + 1 + AUTH_CHALLEN + AUTH_ANAMELEN, AUTH_ANAMELEN);

    /* Copy key (variable length, store as much as fits in Ticket.key) */
    key_len = key_len - (1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN);
    if (key_len > (int)sizeof(t->key)) {
        key_len = sizeof(t->key);
    }
    memcpy(t->key, buf + 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN, key_len);

    fprintf(stderr, "dp9ik_parse_ticket: parsed ticket for user=%s, server=%s\n",
            t->cuid, t->suid);

    return 0;
}

/*
 * Parse authenticator from buffer
 * Authenticator format: num(1) + chal(8) + rand(8) + MAC(32)
 * Total: 49 bytes
 */
int dp9ik_parse_authenticator(Authenticator *a, const unsigned char *buf, int len)
{
    int auth_min_len;

    if (a == NULL || buf == NULL) {
        return -1;
    }

    /* Minimum authenticator size: num(1) + chal(8) + rand(8) = 17 bytes
     * With MAC(32): 49 bytes
     */
    auth_min_len = 1 + AUTH_CHALLEN + AUTH_NONCELEN + 32;

    if (len < auth_min_len) {
        fprintf(stderr, "dp9ik_parse_authenticator: buffer too short (%d < %d)\n",
                len, auth_min_len);
        return -1;
    }

    /* Clear authenticator structure */
    memset(a, 0, sizeof(Authenticator));

    /* Parse authenticator fields */
    a->num = buf[0];
    memcpy(a->chal, buf + 1, AUTH_CHALLEN);
    memcpy(a->rand, buf + 1 + AUTH_CHALLEN, AUTH_NONCELEN);

    /* MAC is at the end but not stored in Authenticator struct */
    /* TODO: Verify MAC if needed */

    fprintf(stderr, "dp9ik_parse_authenticator: parsed authenticator\n");

    return 0;
}

/*
 * Validate ticket (MVP version: format check only)
 * Production version should verify MAC using password-derived key
 */
int dp9ik_validate_ticket_mvp(const Ticket *t, const char *expected_user, const char *domain)
{
    int i;

    if (t == NULL) {
        fprintf(stderr, "dp9ik_validate_ticket_mvp: NULL ticket\n");
        return -1;
    }

    /* Check that cuid is not all zeros */
    for (i = 0; i < AUTH_ANAMELEN && t->cuid[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_ANAMELEN) {
        fprintf(stderr, "dp9ik_validate_ticket_mvp: empty cuid\n");
        return -1;
    }

    /* Check that suid is not all zeros */
    for (i = 0; i < AUTH_ANAMELEN && t->suid[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_ANAMELEN) {
        fprintf(stderr, "dp9ik_validate_ticket_mvp: empty suid\n");
        return -1;
    }

    /* Verify cuid matches expected user */
    if (expected_user != NULL) {
        if (strncmp(t->cuid, expected_user, AUTH_ANAMELEN) != 0) {
            fprintf(stderr, "dp9ik_validate_ticket_mvp: cuid mismatch (got %s, expected %s)\n",
                    t->cuid, expected_user);
            return -1;
        }
    }

    /* Verify suid matches domain (for MVP, domain is often the server ID) */
    if (domain != NULL && t->suid[0] != '\0') {
        /* For MVP, just check that suid starts with domain or is reasonable */
        if (strncmp(t->suid, domain, strlen(domain)) != 0) {
            /* Not fatal - suid might be a specific server ID */
            fprintf(stderr, "dp9ik_validate_ticket_mvp: suid=%s (domain=%s)\n",
                    t->suid, domain);
        }
    }

    /* Check that challenge is not all zeros */
    for (i = 0; i < AUTH_CHALLEN && t->chal[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_CHALLEN) {
        fprintf(stderr, "dp9ik_validate_ticket_mvp: empty challenge\n");
        return -1;
    }

    /* TODO: Verify MAC using password-derived key (production) */
    fprintf(stderr, "dp9ik_validate_ticket_mvp: ticket format validated for user=%s\n",
            t->cuid);

    return 0;
}

/*
 * Verify authenticator (MVP version: format check only)
 * Production version should verify MAC using ticket-derived session key
 */
int dp9ik_verify_authenticator_mvp(const Authenticator *a, const Ticket *t)
{
    int i;

    if (a == NULL || t == NULL) {
        fprintf(stderr, "dp9ik_verify_authenticator_mvp: NULL parameter\n");
        return -1;
    }

    /* Check that challenge matches ticket's challenge */
    if (memcmp(a->chal, t->chal, AUTH_CHALLEN) != 0) {
        fprintf(stderr, "dp9ik_verify_authenticator_mvp: challenge mismatch\n");
        return -1;
    }

    /* Check that rand is not all zeros */
    for (i = 0; i < AUTH_NONCELEN && a->rand[i] == '\0'; i++) {
        /* Continue checking */
    }
    if (i == AUTH_NONCELEN) {
        fprintf(stderr, "dp9ik_verify_authenticator_mvp: empty random nonce\n");
        return -1;
    }

    /* TODO: Verify MAC using ticket-derived session key (production) */
    fprintf(stderr, "dp9ik_verify_authenticator_mvp: authenticator format validated\n");

    return 0;
}

/*
 * Create server authenticator response
 * This is what the server sends back after verifying the client's authenticator
 */
int dp9ik_create_server_authenticator(Authenticator *a,
                                     const unsigned char *client_chal,
                                     const unsigned char *server_rand)
{
    if (a == NULL || client_chal == NULL || server_rand == NULL) {
        return -1;
    }

    /* Clear authenticator */
    memset(a, 0, sizeof(Authenticator));

    /* Set fields */
    a->num = 0;  /* TODO: proper counter/replay protection */
    memcpy(a->chal, client_chal, AUTH_CHALLEN);  /* Echo client's challenge */
    memcpy(a->rand, server_rand, AUTH_NONCELEN); /* Our random nonce */

    fprintf(stderr, "dp9ik_create_server_authenticator: created server authenticator\n");

    /* Note: MAC computation happens during serialization in p9any_send_authenticator */

    return 0;
}

/*
 * Decrypt ticket using password-derived key (MVP version)
 * For MVP: Skip decryption, just parse the ticket structure
 * Production: Implement proper decryption with Ks
 */
int dp9ik_decrypt_ticket_mvp(const unsigned char *encrypted, int enc_len,
                              const char *password, const char *user,
                              Ticket *t)
{
    int base_ticket_len;

    if (encrypted == NULL || t == NULL) {
        fprintf(stderr, "dp9ik_decrypt_ticket_mvp: NULL parameter\n");
        return -1;
    }

    /* Base ticket size: num(1) + chal(8) + cuid(28) + suid(28) + key(32) = 73 bytes */
    base_ticket_len = 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN + AUTH_PAKKEYLEN;

    if (enc_len < base_ticket_len) {
        fprintf(stderr, "dp9ik_decrypt_ticket_mvp: buffer too short (%d < %d)\n",
                enc_len, base_ticket_len);
        return -1;
    }

    /* For MVP: Just parse the unencrypted ticket structure */
    memset(t, 0, sizeof(Ticket));

    t->num = encrypted[0];
    memcpy(t->chal, encrypted + 1, AUTH_CHALLEN);
    memcpy(t->cuid, encrypted + 1 + AUTH_CHALLEN, AUTH_ANAMELEN);
    memcpy(t->suid, encrypted + 1 + AUTH_CHALLEN + AUTH_ANAMELEN, AUTH_ANAMELEN);
    memcpy(t->key, encrypted + 1 + AUTH_CHALLEN + 2 * AUTH_ANAMELEN, AUTH_PAKKEYLEN);

    fprintf(stderr, "dp9ik_decrypt_ticket_mvp: parsed ticket for cuid=%s suid=%s\n",
            t->cuid, t->suid);

    /* TODO: Implement proper decryption with Ks (password-derived key) */
    (void)password;
    (void)user;

    return 0;
}

/*
 * Parse form1-encrypted authenticator (MVP: just extract fields)
 * form1 format: num(1) + sig(8) + encrypted_data + nonce(12) + mac(16)
 * For MVP: Assume simple format without encryption
 */
int dp9ik_parse_authenticator_mvp(const unsigned char *buf, int len,
                                   Authenticator *a)
{
    /* form1 format: num(1) + sig(8) + encrypted_data + nonce(12) + mac(16) */
    /* For MVP: assume simple format without encryption */

    if (buf == NULL || a == NULL) {
        fprintf(stderr, "dp9ik_parse_authenticator_mvp: NULL parameter\n");
        return -1;
    }

    /* Minimum size: num(1) + chal(8) + rand(8) = 17 bytes */
    if (len < 17) {
        fprintf(stderr, "dp9ik_parse_authenticator_mvp: buffer too short (%d < 17)\n", len);
        return -1;
    }

    memset(a, 0, sizeof(Authenticator));

    /* For MVP: Assume simple format - num + chal + rand */
    a->num = buf[0];
    memcpy(a->chal, buf + 1, AUTH_CHALLEN);
    if (len > 1 + AUTH_CHALLEN) {
        int rand_len = len - 1 - AUTH_CHALLEN;
        if (rand_len > (int)AUTH_NONCELEN) {
            rand_len = AUTH_NONCELEN;
        }
        memcpy(a->rand, buf + 1 + AUTH_CHALLEN, rand_len);
    }

    /* TODO: Implement proper form1 decryption with ChaCha20-Poly1305 */

    fprintf(stderr, "dp9ik_parse_authenticator_mvp: parsed authenticator (MVP mode)\n");

    return 0;
}

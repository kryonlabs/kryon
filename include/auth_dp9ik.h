/*
 * Kryon Authentication - dp9ik (Ed448) Authentication
 * C89/C90 compliant
 *
 * Based on 9front libauthsrv/authpak.c
 * Uses OpenSSL EVP_PKEY_ED448 for Ed448 operations
 */

#ifndef AUTH_DPIK_H
#define AUTH_DPIK_H

#include "devfactotum.h"
#include <stddef.h>
#include <stdint.h>

/*
 * dp9ik constants
 */
#define DPIK_KEYLEN      56      /* Ed448 key length (448 bits = 56 bytes) */
#define DPIK_NONCELEN    64      /* SPAKE2-EE nonce length */
#define DPIK_HASHLEN     64      /* SHA-512 hash length */
#define DPIK_SESSION_KEY_LEN 32  /* 256-bit session key */

/*
 * Build ticket request for dp9ik
 * Fills tr structure with ticket request data
 * Returns 0 on success, -1 on error
 */
int dp9ik_build_ticketreq(const char *authid, const char *authdom,
                          const char *hostid, const char *uid,
                          const unsigned char *chal,
                          Ticketreq *tr);

/*
 * Serialize ticket request to buffer
 * Returns bytes written or -1 on error
 */
int dp9ik_serialize_ticketreq(const Ticketreq *tr,
                               unsigned char *buf, size_t len);

/*
 * Parse and validate ticket from client
 * Verifies ticket using password
 * Returns 0 on success, -1 on error
 */
int dp9ik_validate_ticket(const unsigned char *ticket, int tlen,
                          const char *password,
                          Ticket *t);

/*
 * Build authenticator for server response
 * Returns bytes written or -1 on error
 */
int dp9ik_build_authenticator(const Ticket *t,
                              const unsigned char *client_chal,
                              const unsigned char *server_nonce,
                              Authenticator *auth,
                              unsigned char *buf, size_t len);

/*
 * Derive session keys using HKDF
 * This implements the key derivation from drawterm's dp9ik
 * Returns 0 on success, -1 on error
 */
int dp9ik_derive_keys(const unsigned char *client_chal,
                      const unsigned char *server_chal,
                      const Ticket *t,
                      AuthInfo *ai);

/*
 * Generate PAK key (Ed448 key pair)
 * Generates public/private key pair for server
 * Returns 0 on success, -1 on error
 */
int dp9ik_pak_key_generate(unsigned char *pubkey, size_t publen,
                           unsigned char *seckey, size_t seclen);

/*
 * Compute PAK hash point from password
 * This implements the password-to-key conversion from 9front authpak.c
 * Returns 0 on success, -1 on error
 */
int dp9ik_passtokey(const unsigned char *password, size_t pwlen,
                    const char *user,
                    unsigned char *key, size_t keylen);

/*
 * Compute PAK shared secret
 * Implements SPAKE2-EE key exchange using Ed448
 * Returns 0 on success, -1 on error
 */
int dp9ik_pak_server(const unsigned char *client_pubkey,
                     const unsigned char *server_seckey,
                     const unsigned char *password_hash,
                     unsigned char *shared_secret);

/*
 * HKDF key derivation (from drawterm/libauthsrv)
 * Implements HKDF-X with HMAC-SHA2-256
 * Returns 0 on success, -1 on error
 */
int dp9ik_hkdf(const unsigned char *ikm, size_t ikmlen,
               const unsigned char *salt, size_t saltlen,
               const unsigned char *info, size_t infolen,
               unsigned char *okm, size_t okmlen);

/*
 * Convert password to PAK key hash
 * From 9front authpak.c:195
 * Returns 0 on success, -1 on error
 */
int dp9ik_authpak_hash(unsigned char *key, const char *user);

/*
 * Encrypt ticket using password-derived key
 * Simple XOR-based encryption (from 9front)
 */
void dp9ik_encrypt_ticket(const unsigned char *ticket, int len,
                          const unsigned char *key);

/*
 * Decrypt ticket using password-derived key
 */
void dp9ik_decrypt_ticket(unsigned char *ticket, int len,
                          const unsigned char *key);

/*
 * Generate random bytes
 * Uses OpenSSL RAND_bytes
 * Returns 0 on success, -1 on error
 */
int dp9ik_random_bytes(unsigned char *buf, size_t len);

/*
 * Generate server challenge
 * Returns 0 on success, -1 on error
 */
int dp9ik_gen_challenge(unsigned char *chal);

/*
 * Generate server nonce
 * Returns 0 on success, -1 on error
 */
int dp9ik_gen_nonce(unsigned char *nonce);

/*
 * Verify client authenticator
 * Returns 0 on success, -1 on error
 */
int dp9ik_verify_authenticator(const Ticket *t,
                               const unsigned char *client_chal,
                               const unsigned char *server_nonce,
                               const Authenticator *auth);

/*
 * Real ticket validation with MAC verification
 * Returns 0 on success, -1 on error
 */
int dp9ik_validate_ticket_real(const unsigned char *ticket, int tlen,
                                const char *password,
                                const char *user,
                                Ticket *t);

/*
 * Real session key derivation using HKDF
 * Returns 0 on success, -1 on error
 */
int dp9ik_derive_keys_real(const unsigned char *client_chal,
                           const unsigned char *server_chal,
                           const Ticket *t,
                           AuthInfo *ai);

/*
 * Real authenticator computation with HMAC
 * Returns bytes written or -1 on error
 */
int dp9ik_build_authenticator_real(const Ticket *t,
                                   const unsigned char *client_chal,
                                   const unsigned char *server_nonce,
                                   const unsigned char *session_key,
                                   Authenticator *auth,
                                   unsigned char *buf, size_t len);

/*
 * Crypto initialization
 * Must be called before using dp9ik functions
 * Returns 0 on success, -1 on error
 */
int dp9ik_crypto_init(void);

/*
 * Crypto cleanup
 */
void dp9ik_crypto_cleanup(void);

/*
 * Parse ticket from buffer
 * Ticket format: num(1) + chal(8) + cuid(28) + suid(28) + key(64+) + MAC(32)
 * Returns 0 on success, -1 on error
 */
int dp9ik_parse_ticket(Ticket *t, const unsigned char *buf, int len);

/*
 * Parse authenticator from buffer
 * Authenticator format: num(1) + chal(8) + rand(8) + MAC(32)
 * Returns 0 on success, -1 on error
 */
int dp9ik_parse_authenticator(Authenticator *a, const unsigned char *buf, int len);

/*
 * Validate ticket (MVP: format check, production: MAC verification)
 * Returns 0 on success, -1 on error
 */
int dp9ik_validate_ticket_mvp(const Ticket *t, const char *expected_user, const char *domain);

/*
 * Verify authenticator (MVP: format check, production: MAC verification)
 * Returns 0 on success, -1 on error
 */
int dp9ik_verify_authenticator_mvp(const Authenticator *a, const Ticket *t);

/*
 * Create server authenticator response
 * Returns 0 on success, -1 on error
 */
int dp9ik_create_server_authenticator(Authenticator *a,
                                     const unsigned char *client_chal,
                                     const unsigned char *server_rand);

/*
 * Decrypt ticket using password-derived key (MVP version)
 * For MVP: Skip decryption, just parse the ticket structure
 * Returns 0 on success, -1 on error
 */
int dp9ik_decrypt_ticket_mvp(const unsigned char *encrypted, int enc_len,
                              const char *password, const char *user,
                              Ticket *t);

/*
 * Parse form1-encrypted authenticator (MVP version)
 * For MVP: Just extract fields without decryption
 * Returns 0 on success, -1 on error
 */
int dp9ik_parse_authenticator_mvp(const unsigned char *buf, int len,
                                   Authenticator *a);

#endif /* AUTH_DPIK_H */

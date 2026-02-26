/*
 * Kryon Authentication - p9sk1 (DES-based Password Authentication)
 * C89/C90 compliant
 *
 * Based on Plan 9 p9sk1 authentication protocol
 * Uses DES for ticket encryption/decryption
 */

#ifndef AUTH_P9SK1_H
#define AUTH_P9SK1_H

#include "devfactotum.h"
#include <stddef.h>
#include <stdint.h>

/*
 * p9sk1 constants
 */
#define P9SK1_KEYLEN      8       /* DES key length (64 bits with parity) */
#define P9SK1_TICKETLEN   72      /* p9sk1 ticket length */
#define P9SK1_AUTHLEN     17      /* p9sk1 authenticator length */
#define P9SK1_CHALLEN     8       /* Challenge length */

/*
 * Convert password to DES key
 * Algorithm: MD5(password + username) → extract 7 bytes → add parity → 8 byte DES key
 * Returns 0 on success, -1 on error
 */
int p9sk1_passtokey(const char *password, const char *username,
                    unsigned char *des_key, size_t keylen);

/*
 * Find password key from factotum
 * Looks up proto=p9sk1 key in /mnt/factotum/ctl
 * Returns password string (must be freed by caller) or NULL on error
 */
char *p9sk1_find_password(const char *user, const char *dom);

/*
 * Decrypt ticket using DES key
 * Ticket format: num(1) + chal(8) + cuid(28) + suid(28) + key(7)
 * Total: 72 bytes
 * Returns 0 on success, -1 on error
 */
int p9sk1_decrypt_ticket(const unsigned char *encrypted, int enc_len,
                         const unsigned char *des_key,
                         Ticket *t);

/*
 * Validate ticket structure
 * Returns 0 on success, -1 on error
 */
int p9sk1_validate_ticket(const Ticket *t, const char *expected_user,
                          const char *expected_dom);

/*
 * Parse authenticator from buffer
 * Authenticator format: num(1) + chal(8) + id(8)
 * Total: 17 bytes
 * Returns 0 on success, -1 on error
 */
int p9sk1_parse_authenticator(const unsigned char *buf, int len,
                              Authenticator *a);

/*
 * Verify authenticator
 * Checks that challenge matches ticket's challenge
 * Returns 0 on success, -1 on error
 */
int p9sk1_verify_authenticator(const Authenticator *a,
                               const Ticket *t);

/*
 * Generate server authenticator
 * Returns 0 on success, -1 on error
 */
int p9sk1_create_server_authenticator(Authenticator *a,
                                     const unsigned char *client_chal,
                                     const unsigned char *server_rand);

/*
 * Generate random nonce
 * Returns 0 on success, -1 on error
 */
int p9sk1_gen_nonce(unsigned char *nonce, size_t len);

/*
 * Encrypt buffer using DES ECB mode
 * Returns 0 on success, -1 on error
 */
int p9sk1_des_ecb_encrypt(const unsigned char *plaintext,
                          unsigned char *ciphertext,
                          int len, const unsigned char *key);

/*
 * Decrypt buffer using DES ECB mode
 * Returns 0 on success, -1 on error
 */
int p9sk1_des_ecb_decrypt(const unsigned char *ciphertext,
                          unsigned char *plaintext,
                          int len, const unsigned char *key);

/*
 * Set odd parity on DES key
 * DES requires odd parity on each byte (least significant bit is parity)
 */
void p9sk1_des_set_parity(unsigned char *key, size_t len);

/*
 * Grant ticket for p9sk1 authentication
 * Server generates ticket using password-derived key and sends to client
 * Returns 0 on success, -1 on error
 */
int p9sk1_grant_ticket(const char *user, const char *dom,
                       const unsigned char *client_chal,
                       unsigned char *ticket_out, int *ticket_len);

#endif /* AUTH_P9SK1_H */

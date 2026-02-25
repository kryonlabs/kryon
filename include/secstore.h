/*
 * Kryon Authentication - Secstore Protocol Support
 * C89/C90 compliant
 *
 * Based on 9front secstore
 */

#ifndef SECSTORE_H
#define SECSTORE_H

#include "devfactotum.h"
#include <stddef.h>
#include <stdint.h>

/*
 * Secstore test message prefix
 */
#define SECSTORE_TESTMSG "__secstore\tPAK\nC=%s\nm=0\n"

/*
 * Secstore message prefix: 0x80 0x17 + "secstore"
 */
#define SECSTORE_PREFIX_LEN 10
#define SECSTORE_PREFIX "\x80\x17secstore"

/*
 * Secstore connection detection
 * Returns 1 if secstore protocol detected, 0 otherwise
 */
int secstore_detect(int fd);

/*
 * Secstore protocol handler
 * Returns 0 on success, -1 on error
 */
int secstore_handler(int fd);

/*
 * Secstore data structures (from 9front secstore.h)
 */
typedef struct SecstorePW {
    char *id;           /* User ID */
    unsigned long expire;       /* Expiration time */
    unsigned short status;      /* Enabled, STA flags */
    unsigned short failed;      /* Failed login attempts */
    char *other;        /* Additional info */
    unsigned char *hi;  /* Hi field for PAK (large integer) */
    size_t hi_len;      /* Length of hi field */
} SecstorePW;

/*
 * Virtual secstore filesystem at /adm/secstore
 * Returns 0 on success, -1 on error
 */
int secstore_init(void *root_dir);

/*
 * Secstore operations
 */

/*
 * Fetch keys for user from secstore
 * Returns keys in factotum format (malloc'd, caller frees)
 * Returns 0 on success, -1 on error
 */
int secstore_fetch(const char *user, const char *password,
                   char **factotum_keys);

/*
 * Store data in secstore for user
 * Returns 0 on success, -1 on error
 */
int secstore_store(const char *user, const char *password,
                   const char *data);

/*
 * Validate secstore password
 * Returns 1 if valid, 0 if invalid
 */
int secstore_validate(const char *user, const char *password);

/*
 * Load secstore database from file
 * Default: /adm/secstore/store
 * Returns 0 on success, -1 on error
 */
int secstore_load_db(const char *path);

/*
 * Save secstore database to file
 * Returns 0 on success, -1 on error
 */
int secstore_save_db(const char *path);

/*
 * PAK protocol for secstore
 * Similar to dp9ik but with different parameters
 * Returns 0 on success, -1 on error
 */
int secstore_pak_server(int fd, const char *user, const char *password);

/*
 * Secstore file operations
 */

/*
 * /adm/secstore/store/<user> file read
 */
ssize_t secstore_user_read(const char *user, char *buf, size_t count,
                           uint64_t offset);

/*
 * /adm/secstore/store/<user> file write
 */
ssize_t secstore_user_write(const char *user, const char *buf, size_t count,
                            uint64_t offset);

/*
 * Secstore log file
 */
#define SECSTORE_LOG_FILE "/adm/secstore/log"

/*
 * Log secstore access
 */
void secstore_log(const char *user, const char *action, int success);

/*
 * Secstore directory QIDs
 */
enum {
    SECSTORE_QID_ROOT = 100,
    SECSTORE_QID_STORE_DIR,
    SECSTORE_QID_USER_BASE,  /* + user_id */
    SECSTORE_QID_LOG,
};

/*
 * SConn - Secure Connection for encrypted secstore communication
 * Simple XOR-based encryption for now (can upgrade to AES later)
 */
typedef struct SConn {
    int fd;                      /* Underlying file descriptor */
    unsigned char key[32];       /* Session key */
    unsigned char iv[16];        /* Initialization vector */
    int encrypt;                 /* Whether encryption is enabled */
} SConn;

/*
 * SConn functions
 */

/*
 * Create new SConn with session key
 * Returns SConn pointer or NULL on error
 */
SConn *sconn_new(int fd, const unsigned char *key, size_t keylen);

/*
 * Read from SConn with decryption
 * Returns bytes read or -1 on error
 */
ssize_t sconn_read(SConn *c, void *buf, size_t len);

/*
 * Write to SConn with encryption
 * Returns bytes written or -1 on error
 */
ssize_t sconn_write(SConn *c, const void *buf, size_t len);

/*
 * Read line from SConn (up to newline)
 * Returns bytes read or -1 on error
 */
ssize_t sconn_readline(SConn *c, char *buf, size_t len);

/*
 * Free SConn
 */
void sconn_free(SConn *c);

#endif /* SECSTORE_H */

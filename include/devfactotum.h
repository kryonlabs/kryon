/*
 * Kryon Authentication - 9front-Style Factotum Interface
 * C89/C90 compliant
 *
 * Based on 9front sources:
 * - sys/src/cmd/auth/factotum/dat.h
 * - sys/include/authsrv.h
 */

#ifndef DEVFACTOTUM_H
#define DEVFACTOTUM_H

#include <stddef.h>
#include <stdint.h>
#include <time.h>

/*
 * C89 compatibility: ssize_t is not defined in C89
 */
#ifdef _WIN32
typedef long ssize_t;
#else
#include <sys/types.h>
#endif

/*
 * Authentication constants from 9front authsrv.h
 */
#define AUTH_ANAMELEN    28      /* Authentication name length */
#define AUTH_DOMLEN      48      /* Domain name length */
#define AUTH_CHALLEN     8       /* Challenge length */
#define AUTH_NONCELEN    8       /* Nonce length for p9sk1 */
#define AUTH_PAKYLEN     57      /* dp9ik Ed448 key length (decaf encoded) */
#define AUTH_PAKKEYLEN   32      /* PAK shared secret length (Kn) */
#define AUTH_TICKETLEN   73      /* Base ticket size (num + chal + cuid + suid + key) */
#define AUTH_FORM1HDRLEN 12      /* form1 header length */
#define AUTH_FORM1MACLEN 16      /* ChaCha20-Poly1305 MAC length */
#define AUTH_MAXKEYLEN   256     /* Maximum key password length */

/*
 * Authentication message types (from 9front authsrv.h)
 */
enum {
    AUTH_TREQ = 1,     /* Ticket request */
    AUTH_CHAL = 2,     /* Challenge */
    AUTH_PASS = 3,     /* Change password */
    AUTH_OK = 4,       /* Fixed length reply */
    AUTH_ERR = 5,      /* Error */
    AUTH_PAK = 19,     /* Authenticated DH key exchange (dp9ik) */
    AUTH_TS = 64,      /* Ticket with server key */
    AUTH_TC = 65,      /* Ticket with client key */
    AUTH_AS = 66,      /* Server authenticator */
    AUTH_AC = 67,      /* Client authenticator */
};

/*
 * Virtual file QIDs for /mnt/factotum
 */
enum {
    FACTOTUM_QID_CONFIRM = 1,
    FACTOTUM_QID_NEEDKEY,
    FACTOTUM_QID_CTL,
    FACTOTUM_QID_RPC,
    FACTOTUM_QID_PROTO,
    FACTOTUM_QID_LOG,
};

/*
 * Key attribute structure (from 9front dat.h)
 * Attribute-value pairs for key lookup
 */
typedef struct FactotumAttr {
    char *name;
    char *value;
    struct FactotumAttr *next;
} FactotumAttr;

/*
 * Protocol types (from 9front fs.c prototab[])
 */
typedef enum {
    PROTO_NONE = 0,
    PROTO_P9SK1,       /* Original Plan 9 shared key (DES-based, legacy) */
    PROTO_DPIK,        /* Extended P9SK1 with forward secrecy (Ed448 ECC) */
    PROTO_PASS,        /* Clear text passwords (for testing) */
    PROTO_APOP,        /* APOP protocol */
    PROTO_CHAP,        /* CHAP protocol */
    PROTO_CRAM,        /* CRAM protocol */
    PROTO_HTTPDIGEST,  /* HTTP digest */
    PROTO_MSCHAP,      /* MS-CHAP */
    PROTO_NTLM,        /* NTLM */
    PROTO_RSA,         /* RSA */
    PROTO_ECDSA,       /* ECDSA */
    PROTO_TOTP,        /* TOTP */
    PROTO_WPAPSK,      /* WPA PSK */
} ProtoType;

/*
 * Key structure (from 9front dat.h)
 */
typedef struct FactotumKey {
    FactotumAttr *attr;          /* Public attributes (proto, dom, user, etc.) */
    FactotumAttr *privattr;      /* Private attributes (!password, !key, etc.) */
    ProtoType proto_type;        /* Protocol type */
    void *priv;                  /* Protocol-specific parsed data */
    unsigned long successes;     /* Usage counter */
    struct FactotumKey *next;    /* Next key in list */
} FactotumKey;

/*
 * Authentication info (from 9front)
 * Result of successful authentication
 */
typedef struct AuthInfo {
    char *suid;          /* Server user ID */
    char *cuid;          /* Client user ID */
    unsigned char *secret;   /* Session key */
    int nsecret;         /* Secret length */
} AuthInfo;

/*
 * Ticket request structure (from 9front authsrv.h)
 */
typedef struct Ticketreq {
    char type;
    char authid[AUTH_ANAMELEN];     /* Server's encryption id */
    char authdom[AUTH_DOMLEN];      /* Authentication domain */
    char chal[AUTH_CHALLEN];        /* Challenge from server */
    char hostid[AUTH_ANAMELEN];     /* Host's encryption id */
    char uid[AUTH_ANAMELEN];        /* UID of requesting user */
} Ticketreq;

/*
 * Ticket structure (from 9front authsrv.h)
 * For dp9ik: key stores PAK shared secret Kn (32 bytes)
 */
typedef struct Ticket {
    char num;                   /* Replay protection */
    char chal[AUTH_CHALLEN];    /* Server challenge */
    char cuid[AUTH_ANAMELEN];   /* Client uid */
    char suid[AUTH_ANAMELEN];   /* Server uid */
    unsigned char key[AUTH_PAKKEYLEN]; /* PAK shared secret Kn (32 bytes for dp9ik) */
} Ticket;

/*
 * Authenticator structure (from 9front authsrv.h)
 */
typedef struct Authenticator {
    char num;                       /* Replay protection */
    char chal[AUTH_CHALLEN];        /* Server/client challenge */
    unsigned char rand[AUTH_NONCELEN]; /* Server/client nonce */
} Authenticator;

/*
 * Form1 encrypted message format (for dp9ik)
 * Used for encrypted authenticators with ChaCha20-Poly1305
 */
typedef struct Form1Msg {
    char num;                       /* Message type (AuthAc, AuthAs, etc.) */
    char sig[8];                    /* Signature "form1 xxx" */
    unsigned char nonce[12];        /* ChaCha20 nonce */
    unsigned char mac[16];          /* Poly1305 MAC */
} Form1Msg;

/*
 * Maximum number of keys
 */
#define MAX_KEYS 64

/*
 * Maximum number of concurrent authentication sessions
 */
#define MAX_AUTH_SESSIONS 64

/*
 * Authentication session state
 */
typedef enum {
    AUTH_PROTO_NONE = 0,
    AUTH_PROTO_P9ANY,
    AUTH_PROTO_SECSTORE
} AuthProtoType;

/*
 * Forward declarations
 */
struct P9AnySession;

/*
 * Authentication session (per-connection)
 */
typedef struct AuthSession {
    int client_fd;
    AuthProtoType proto_type;
    union {
        struct P9AnySession *p9any;
        void *secstore;
    } proto_state;
    AuthInfo *ai;
    int state;
    time_t start_time;
} AuthSession;

/*
 * Initialize /mnt/factotum filesystem hierarchy
 * Returns 0 on success, -1 on error
 */
int factotum_init(void *root_dir);

/*
 * Key management functions
 */

/*
 * Add a key from a string format
 * Format: "key proto=dp9ik dom=localhost user=glenda !password=secret"
 * Returns 0 on success, -1 on error
 */
int factotum_add_key(const char *key_str);

/*
 * Delete a key matching the specification
 * Format: "delkey proto=dp9ik dom=kryon user=glenda"
 * Returns 0 on success, -1 on error
 */
int factotum_del_key(const char *key_spec);

/*
 * Find a key matching proto, domain, and user
 * Returns key pointer or NULL if not found
 */
FactotumKey *factotum_find_key(const char *proto, const char *dom,
                                const char *user);

/*
 * Free a key structure
 */
void factotum_free_key(FactotumKey *key);

/*
 * Get attribute value from key
 */
const char *factotum_get_attr(FactotumAttr *attr, const char *name);

/*
 * Key storage functions
 */

/*
 * Default key file location
 */
#define FACTOTUM_KEY_FILE "/etc/kryon/factotum.keys"

/*
 * Load keys from file
 * Returns 0 on success, -1 on error
 */
int factotum_load_keys(const char *path);

/*
 * Save keys to file
 * Returns 0 on success, -1 on error
 */
int factotum_save_keys(const char *path);

/*
 * Parse a single key line
 * Returns 0 on success, -1 on error
 */
int factotum_parse_key_line(const char *line);

/*
 * Virtual file operations for /mnt/factotum
 */

/*
 * ctl file - key management interface
 */
ssize_t factotum_ctl_read(char *buf, size_t count, uint64_t offset);
ssize_t factotum_ctl_write(const char *buf, size_t count, uint64_t offset);

/*
 * proto file - available protocols list (read-only)
 */
ssize_t factotum_proto_read(char *buf, size_t count, uint64_t offset);

/*
 * confirm file - key confirmation interface
 */
ssize_t factotum_confirm_read(char *buf, size_t count, uint64_t offset);
ssize_t factotum_confirm_write(const char *buf, size_t count, uint64_t offset);

/*
 * needkey file - key prompting interface
 */
ssize_t factotum_needkey_read(char *buf, size_t count, uint64_t offset);
ssize_t factotum_needkey_write(const char *buf, size_t count, uint64_t offset);

/*
 * rpc file - RPC interface for authentication operations
 */
ssize_t factotum_rpc_read(char *buf, size_t count, uint64_t offset);
ssize_t factotum_rpc_write(const char *buf, size_t count, uint64_t offset);

/*
 * log file - activity log
 */
ssize_t factotum_log_read(char *buf, size_t count, uint64_t offset);
ssize_t factotum_log_write(const char *buf, size_t count, uint64_t offset);

/*
 * Session management
 */

/*
 * Initialize session tracking
 * Returns 0 on success, -1 on error
 */
int auth_session_init(void);

/*
 * Create new session for client fd
 * Returns session pointer or NULL on error
 */
AuthSession *auth_session_new(int client_fd);

/*
 * Get session for client fd
 * Returns session pointer or NULL if not found
 */
AuthSession *auth_session_get(int client_fd);

/*
 * Delete session for client fd
 */
void auth_session_delete(int client_fd);

/*
 * Cleanup all sessions (on shutdown)
 */
void auth_session_cleanup(void);

/*
 * Check for session timeouts (call periodically)
 * Timeout: 30 seconds
 */
void auth_session_check_timeouts(void);

/*
 * Protocol detection from main.c
 */
#define PROTOCOL_9P       0
#define PROTOCOL_RCPU      1
#define PROTOCOL_AUTH_P9   2   /* p9 auth */
#define PROTOCOL_AUTH_SEC  3   /* secstore auth */

#endif /* DEVFACTOTUM_H */

/*
 * Kryon Authentication - p9any Protocol Handler
 * C89/C90 compliant
 *
 * Based on drawterm cpu.c and 9front factotum
 */

#ifndef AUTH_P9ANY_H
#define AUTH_P9ANY_H

#include "devfactotum.h"
#include <stddef.h>
#include <stdint.h>

/*
 * p9any protocol state machine
 */
typedef enum {
    P9ANY_STATE_INIT = 0,        /* Initial state */
    P9ANY_STATE_SENT_V2,         /* Sent v.2 protocol list */
    P9ANY_STATE_RCVD_CHOICE,     /* Received client choice */
    P9ANY_STATE_SENT_OK,         /* Sent OK confirmation */
    P9ANY_STATE_RCVD_CHAL,       /* Received client challenge */
    P9ANY_STATE_SENT_TICKREQ,    /* Sent ticket request */
    P9ANY_STATE_RCVD_TICKET,     /* Received ticket */
    P9ANY_STATE_COMPLETE,        /* Authentication complete */
    P9ANY_STATE_ERROR            /* Error state */
} P9AnyState;

/*
 * Per-connection p9any state
 */
typedef struct P9AnySession {
    int client_fd;
    P9AnyState state;
    char chosen_proto[AUTH_ANAMELEN];    /* p9sk1 or dp9ik */
    char domain[AUTH_DOMLEN];
    char user[AUTH_ANAMELEN];
    unsigned char client_chal[AUTH_CHALLEN];
    unsigned char server_nonce[AUTH_NONCELEN];
    unsigned char pak_y[AUTH_PAKYLEN];   /* PAK public key (dp9ik) */
    AuthInfo *ai;
} P9AnySession;

/*
 * Main p9any handler
 * Called from server main loop when p9 auth is detected
 * Returns 0 on success, -1 on error
 */
int p9any_handler(int client_fd, const char *domain);

/*
 * Step-by-step protocol functions
 */

/*
 * Send available protocols to client
 * Format: "p9sk1@domain dp9ik@domain"
 * Returns bytes sent or -1 on error
 */
int p9any_send_protocols(int fd, const char *domain);

/*
 * Receive client's protocol choice
 * Parses "dp9ik@kryon" into proto and domain
 * Returns 0 on success, -1 on error
 */
int p9any_recv_choice(int fd, char *proto, size_t proto_len,
                      char *dom, size_t dom_len);

/*
 * Send OK confirmation (v2 protocol only)
 * Returns bytes sent or -1 on error
 */
int p9any_send_ok(int fd);

/*
 * Receive client challenge (8 bytes)
 * Returns 0 on success, -1 on error
 */
int p9any_recv_challenge(int fd, unsigned char *chal);

/*
 * Send ticket request + PAK public key (for dp9ik)
 * Returns bytes sent or -1 on error
 */
int p9any_send_ticketreq(int fd, const char *proto, const char *dom,
                          const char *user, const unsigned char *chal,
                          const unsigned char *pak_y);

/*
 * Receive ticket + authenticator from client
 * Returns total bytes received or -1 on error
 */
int p9any_recv_ticket(int fd, unsigned char *ticket, int *ticket_len,
                      unsigned char *auth, int *auth_len);

/*
 * Send authenticator response to client
 * Returns bytes sent or -1 on error
 */
int p9any_send_authenticator(int fd, const unsigned char *auth, int len);

/*
 * String parsing functions
 */

/*
 * Parse client's initial "p9 rc4_256 sha1" or just "p9"
 * Extracts encryption algorithms if present
 * Returns 0 on success, -1 on error
 */
int p9any_parse_client_hello(const char *buf, char *ealgs, size_t ealgs_len);

/*
 * Build server's protocol list response
 * Format: "p9sk1@domain dp9ik@domain"
 * Returns string length or -1 on error
 */
int p9any_build_server_hello(char *buf, size_t len, const char *domain);

/*
 * Parse "dp9ik@kryon" into proto and domain
 * Returns 0 on success, -1 on error
 */
int p9any_parse_choice(const char *buf, char *proto, size_t proto_len,
                       char *dom, size_t dom_len);

/*
 * Get protocol type from string
 * Returns PROTO_DPIK, PROTO_P9SK1, or PROTO_NONE
 */
ProtoType p9any_proto_type(const char *proto_str);

/*
 * Create new p9any session
 * Returns session pointer or NULL on error
 */
P9AnySession *p9any_session_new(int client_fd);

/*
 * Free p9any session
 */
void p9any_session_free(P9AnySession *sess);

/*
 * Get default domain for authentication
 */
const char *p9any_default_domain(void);

/*
 * Protocol timeout (seconds)
 */
#define P9ANY_TIMEOUT 30

#endif /* AUTH_P9ANY_H */

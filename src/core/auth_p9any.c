/*
 * Kryon Authentication - p9any Protocol Handler
 * C89/C90 compliant
 *
 * Based on drawterm cpu.c and 9front factotum
 */

#include "auth_p9any.h"
#include "auth_dp9ik.h"
#include "devfactotum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <unistd.h>

static int send_line(int fd, const char *s) {
    size_t len = strlen(s);
    if (write(fd, s, len) != (ssize_t)len) return -1;
    return 0;
}

void dump_buf(const char *msg, char *buf, int n) {
    int i;
    fprintf(stderr, "DEBUG: %s (%d bytes): [", msg, n);
    for(i=0; i<n; i++) {
        unsigned char c = buf[i];
        if(c >= 32 && c <= 126) fprintf(stderr, "%c", c);
        else fprintf(stderr, "\\x%02x", c);
    }
    fprintf(stderr, "]\n");
}

/* Plan 9 style malloc that zeros memory */
static void* mallocz(size_t n, int clr)
{
    void *p = malloc(n);
    if(p != NULL && clr)
        memset(p, 0, n);
    return p;
}

/*
 * strdup is not in C89, provide a simple implementation
 */
static char *strdup_impl(const char *s)
{
    char *dup;
    size_t len;

    if (s == NULL) {
        return NULL;
    }

    len = strlen(s) + 1;
    dup = (char *)malloc(len);
    if (dup == NULL) {
        return NULL;
    }

    memcpy(dup, s, len);
    return dup;
}

#define strdup(s) strdup_impl(s)

/*
 * Default domain for authentication
 */
static const char default_domain[] = "kryon";

/*
 * Get default domain
 */
const char *p9any_default_domain(void)
{
    return default_domain;
}

/*
 * Build server's protocol list response
 * Format: "p9sk1@domain dp9ik@domain"
 */
int p9any_build_server_hello(char *buf, size_t len, const char *domain)
{
    int written;

    if (buf == NULL || len == 0) {
        return -1;
    }

    /* Use provided domain, or default if NULL */
    if (domain == NULL) {
        domain = default_domain;
    }

    written = snprintf(buf, len, "p9sk1@%s dp9ik@%s",
                       domain, domain);

    if (written < 0 || (size_t)written >= len) {
        return -1;
    }

    return written;
}

/*
 * Send available protocols to client
 * Sends newline-terminated string (Plan 9 auth protocol uses \n for line-based protocols)
 */
int p9any_send_protocols(int fd, const char *domain)
{
    char buf[256];
    int len;

    
    len = snprintf(buf, sizeof(buf), "dp9ik@localhost p9sk1@localhost\n");

    fprintf(stderr, "p9any: sending protocol list (%d bytes): %s", len, buf);
    
    if (write(fd, buf, (size_t)len) != (ssize_t)len) {
        return -1;
    }

    fsync(fd);

    return 0;
}
/* * Parsing logic that handles both 'proto@dom' and 'proto dom'
 */
int p9any_parse_choice(const char *buf, char *proto, size_t plen, char *dom, size_t dlen) {
    char *sep = strchr(buf, '@');
    if (!sep) sep = strchr(buf, ' ');
    
    if (sep) {
        size_t proto_part = sep - buf;
        if (proto_part >= plen) return -1;
        memcpy(proto, buf, proto_part);
        proto[proto_part] = '\0';
        strncpy(dom, sep + 1, dlen - 1);
        return 0;
    }
    return -1;
}

/*
 * Receive client's protocol choice
 * Returns 0 on success, -1 on failure
 */
 
 int p9any_recv_choice(int fd, char *proto, size_t proto_len, char *dom, size_t dom_len)
{
    char buf[256];
    char *p, *first_nl, *choice; /* FIX: Declare these at the TOP */
    ssize_t n;
    struct timeval tv;
    fd_set fds;

    choice = NULL; /* Initialize */

    fprintf(stderr, "p9any: awaiting client choice...\n");
    
    FD_ZERO(&fds);
    FD_SET(fd, &fds);
    tv.tv_sec = 60;
    tv.tv_usec = 0;

    if (select(fd + 1, &fds, NULL, NULL, &tv) <= 0) return -1;

    n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';

    /* Handle potential \x00 or \n split from drawterm */
    first_nl = strchr(buf, '\n');
    if (!first_nl) first_nl = strchr(buf, '\0');

    if (first_nl && (size_t)(first_nl - buf) < (size_t)n - 1) {
        choice = first_nl + 1;
    } else {
        choice = buf;
    }

    /* Standard Plan 9 whitespace cleaning */
    for (p = choice + strlen(choice) - 1; p >= choice; p--) {
        if (*p == '\n' || *p == '\r' || *p == ' ' || *p == '\t' || *p == '\0') *p = '\0';
        else break;
    }

    if (p9any_parse_choice(choice, proto, proto_len, dom, dom_len) < 0) {
        strncpy(proto, choice, proto_len - 1);
        proto[proto_len - 1] = '\0';
    }

    return 0;
}

int p9any_recv_challenge(int fd, unsigned char *chal)
{
    ssize_t n;
    n = recv(fd, (char *)chal, AUTH_CHALLEN, 0);
    if (n != AUTH_CHALLEN) {
        /* FIX: Change %zd to %ld and cast n to (long) for C90 */
        fprintf(stderr, "p9any_recv_challenge: failed (got %ld bytes, errno=%d)\n", 
                (long)n, errno);
        return -1;
    }
    return 0;
}

/*
 * Send OK confirmation (v2 protocol only)
 */
int p9any_send_ok(int fd)
{
    const char ok_msg[] = "OK\n";
    ssize_t sent;

    sent = send(fd, ok_msg, strlen(ok_msg), 0);
    if (sent < 0) {
        fprintf(stderr, "p9any_send_ok: send failed\n");
        return -1;
    }

    fprintf(stderr, "p9any: sent OK\n");

    return sent;
}


/*
 * Send ticket request + PAK public key (for dp9ik)
 */
int p9any_send_ticketreq(int fd, const char *proto, const char *dom,
                          const char *user, const unsigned char *chal,
                          const unsigned char *pak_y)
{
    unsigned char buf[512];
    int len;
    ssize_t sent;
    Ticketreq tr;

    /* Build ticket request structure */
    memset(&tr, 0, sizeof(tr));
    tr.type = AUTH_PAK;

    strncpy(tr.authid, proto, AUTH_ANAMELEN - 1);
    strncpy(tr.authdom, dom, AUTH_DOMLEN - 1);
    strncpy(tr.hostid, dom, AUTH_ANAMELEN - 1);  /* Use domain as hostid for now */
    strncpy(tr.uid, user, AUTH_ANAMELEN - 1);
    memcpy(tr.chal, chal, AUTH_CHALLEN);

    /* Serialize ticket request */
    len = dp9ik_serialize_ticketreq(&tr, buf, sizeof(buf));
    if (len < 0) {
        fprintf(stderr, "p9any_send_ticketreq: serialize failed\n");
        return -1;
    }

    /* Append PAK public key for dp9ik */
    if (strcmp(proto, "dp9ik") == 0 && pak_y != NULL) {
        memcpy(buf + len, pak_y, AUTH_PAKYLEN);
        len += AUTH_PAKYLEN;
    }

    /* Send */
    sent = send(fd, (char *)buf, len, 0);
    if (sent < 0) {
        fprintf(stderr, "p9any_send_ticketreq: send failed\n");
        return -1;
    }

    fprintf(stderr, "p9any: sent ticket request (%d bytes)\n", len);

    return sent;
}

/*
 * Receive ticket + authenticator from client
 */
int p9any_recv_ticket(int fd, unsigned char *ticket, int *ticket_len,
                      unsigned char *auth, int *auth_len)
{
    unsigned char buf[1024];
    ssize_t n;

    /* Receive ticket + authenticator */
    /* Ticket format: ticket structure + authenticator */
    n = recv(fd, (char *)buf, sizeof(buf), 0);
    if (n <= 0) {
        fprintf(stderr, "p9any_recv_ticket: recv failed: %s\n", strerror(errno));
        return -1;
    }

    /* For dp9ik, ticket is larger */
    /* Format: [ticket_len][ticket_data][auth_data] */
    /* For now, assume fixed format */

    /* Copy ticket */
    memcpy(ticket, buf, n);
    *ticket_len = n;

    /* TODO: Parse ticket and authenticator separately */
    *auth_len = 0;

    fprintf(stderr, "p9any: received ticket (%d bytes)\n", *ticket_len);

    return n;
}

/*
 * Send authenticator response to client
 */
int p9any_send_authenticator(int fd, const unsigned char *auth, int len)
{
    ssize_t sent;

    sent = send(fd, (char *)auth, len, 0);
    if (sent < 0) {
        fprintf(stderr, "p9any_send_authenticator: send failed\n");
        return -1;
    }

    fprintf(stderr, "p9any: sent authenticator (%d bytes)\n", len);

    return sent;
}

/*
 * Get protocol type from string
 */
ProtoType p9any_proto_type(const char *proto_str)
{
    if (proto_str == NULL) {
        return PROTO_NONE;
    }

    if (strcmp(proto_str, "dp9ik") == 0) {
        return PROTO_DPIK;
    } else if (strcmp(proto_str, "p9sk1") == 0) {
        return PROTO_P9SK1;
    } else if (strcmp(proto_str, "pass") == 0) {
        return PROTO_PASS;
    }

    return PROTO_NONE;
}

/*
 * Create new p9any session
 */
P9AnySession *p9any_session_new(int client_fd)
{
    P9AnySession *sess;

    sess = (P9AnySession *)malloc(sizeof(P9AnySession));
    if (sess == NULL) {
        fprintf(stderr, "p9any_session_new: malloc failed\n");
        return NULL;
    }

    memset(sess, 0, sizeof(P9AnySession));

    sess->client_fd = client_fd;
    sess->state = P9ANY_STATE_INIT;
    sess->ai = NULL;

    return sess;
}

/*
 * Free p9any session
 */
void p9any_session_free(P9AnySession *sess)
{
    if (sess == NULL) {
        return;
    }

    if (sess->ai != NULL) {
        if (sess->ai->suid != NULL) {
            free(sess->ai->suid);
        }
        if (sess->ai->cuid != NULL) {
            free(sess->ai->cuid);
        }
        if (sess->ai->secret != NULL) {
            memset(sess->ai->secret, 0, sess->ai->nsecret);
            free(sess->ai->secret);
        }
        free(sess->ai);
    }

    free(sess);
}

/*
 * Parse client's initial "p9 rc4_256 sha1" or just "p9"
 */
int p9any_parse_client_hello(const char *buf, char *ealgs, size_t ealgs_len)
{
    const char *space;

    if (buf == NULL) {
        return -1;
    }

    /* Check if it starts with "p9" */
    if (memcmp(buf, "p9", 2) != 0) {
        return -1;
    }

    /* Check if there are encryption algorithms specified */
    if (buf[2] == ' ' || buf[2] == '\t') {
        /* "p9 rc4_256 sha1" - extract algorithms */
        space = buf + 3;

        if (ealgs != NULL && ealgs_len > 0) {
            strncpy(ealgs, space, ealgs_len - 1);
            ealgs[ealgs_len - 1] = '\0';
        }

        fprintf(stderr, "p9any: client hello with ealgs: %s\n", space);
    } else {
        /* Just "p9" - no algorithms specified */
        if (ealgs != NULL && ealgs_len > 0) {
            ealgs[0] = '\0';
        }

        fprintf(stderr, "p9any: client hello (no ealgs)\n");
    }

    return 0;
}

static int p9any_proceed_with_auth(int fd, char *proto, char *dom) {
    unsigned char client_chal[AUTH_CHALLEN];
    unsigned char server_nonce[AUTH_NONCELEN];
    unsigned char pak_y[AUTH_PAKYLEN];
    unsigned char ticket_buf[1024];
    int ticket_len, auth_len;
    unsigned char auth_buf[1024];
    const char *user = "glenda";

    /* Receive 8-byte challenge */
    if (p9any_recv_challenge(fd, client_chal) < 0) return -1;

    /* Generate security parameters */
    if (dp9ik_gen_nonce(server_nonce) < 0) return -1;
    if (dp9ik_pak_key_generate(pak_y, sizeof(pak_y), NULL, 0) < 0) return -1;

    /* Send ticket request */
    if (p9any_send_ticketreq(fd, proto, dom, user, client_chal, pak_y) < 0) return -1;

    /* Receive ticket/authenticator */
    if (p9any_recv_ticket(fd, ticket_buf, &ticket_len, auth_buf, &auth_len) < 0) return -1;

    /* Here you would normally finish with your key verification logic */
    fprintf(stderr, "p9any: Proceeding with binary auth for user %s\n", user);
    return 0; 
}


/* * p9any_handler: The main state machine
 */
 /* * p9any_handler: The main state machine
 */
int p9any_handler(int fd, const char *domain) {
    char buf[512], proto[32], dom[64], offer[128];
    char *choice, *p;
    ssize_t n;
    int len;
    const char ok_msg[] = "OK"; /* For OK response below */
    const char empty_ack[] = ""; /* For empty acknowledgment */

    choice = NULL;
    if (domain == NULL) domain = "localhost";

    /* 1. Initial Read */
    n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = '\0';
    
    dump_buf("Initial Read", buf, (int)n);

    /* * Logic: Iterate through the buffer to find the first \n or \0.
     * If there is more data after that first terminator, that's our 'choice'.
     */
    for (p = buf; p < buf + n; p++) {
        if (*p == '\n' || *p == '\r' || *p == '\0') {
            /* Found the end of the 'p9 ...' hello */
            *p = '\0'; 
            /* Peek ahead: is there a choice immediately following? */
            if (p + 1 < buf + n && *(p + 1) != '\0') {
                choice = p + 1;
                /* Skip any leading whitespace/newlines in the choice part */
                while(choice < buf + n && (*choice == '\n' || *choice == '\r' || *choice == ' ')) 
                    choice++;
            }
            break;
        }
    }

    /* 2. Send empty acknowledgment first (drawterm expects this after "p9 ...") */
    if (write(fd, empty_ack, sizeof(empty_ack)) != (ssize_t)sizeof(empty_ack)) return -1;
    fprintf(stderr, "p9any: sent empty acknowledgment\n");

    /* 3. Offer Protocols if client hasn't chosen yet */
    if (!choice || *choice == '\0') {
        /* Drawterm expects v.2 prefix and null-terminated strings */
        len = sprintf(offer, "v.2 dp9ik@%s p9sk1@%s", domain, domain);
        offer[len++] = '\0'; /* Add null terminator explicitly */
        fprintf(stderr, "p9any: sending offer (%d bytes)\n", len);

        if (write(fd, offer, len) != len) return -1;

        /* Wait for the client to reply with a choice */
        n = read(fd, buf, sizeof(buf) - 1);
        if (n <= 0) return -1;
        buf[n] = '\0';
        choice = buf;
        dump_buf("Second Read (Choice)", choice, (int)n);
    }

    /* 3. Clean and Parse Choice */
    for (p = choice; *p; p++) {
        if (*p == '\r' || *p == '\n' || *p == ' ' || *p == '\t' || *p == '\0') {
            *p = '\0';
            break;
        }
    }
    
    if (p9any_parse_choice(choice, proto, sizeof(proto), dom, sizeof(dom)) < 0) {
        strncpy(proto, choice, sizeof(proto) - 1);
        proto[sizeof(proto)-1] = '\0';
        strncpy(dom, domain, sizeof(dom)-1);
        dom[sizeof(dom)-1] = '\0';
    }
    fprintf(stderr, "p9any: finalized choice [%s] on domain [%s]\n", proto, dom);

    /* 5. THE CRITICAL 'OK' - send null-terminated like drawterm expects */
    if (write(fd, ok_msg, sizeof(ok_msg)) != (ssize_t)sizeof(ok_msg)) return -1;

    /* 6. Enter binary phase */
    return p9any_proceed_with_auth(fd, proto, dom);
}
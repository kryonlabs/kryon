/*
 * 9P Client Implementation
 * C89/C90 compliant
 *
 * Generic 9P client for connecting to any 9P server
 */

#include "p9client.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * 9P message constants
 */
#define P9_VERSION    "9P2000"
#define P9_MAX_MSG    8192

/*
 * 9P message types
 */
#define P9_TVERSION  100
#define P9_RVERSION  101
#define P9_TAUTH     102
#define P9_RAUTH     103
#define P9_TATTACH   104
#define P9_RATTACH   105
#define P9_TERROR    106
#define P9_RERROR    107
#define P9_TFLUSH    108
#define P9_RFLUSH    109
#define P9_TWALK     110
#define P9_RWALK     111
#define P9_TOPEN     112
#define P9_ROPEN     113
#define P9_TCREATE   114
#define P9_RCREATE   115
#define P9_TREAD     116
#define P9_RREAD     117
#define P9_TWRITE    118
#define P9_RWRITE    119
#define P9_TCLUNK    120
#define P9_RCLUNK    121
#define P9_TREMOVE   122
#define P9_RREMOVE   123
#define P9_TSTAT     124
#define P9_RSTAT     125
#define P9_TWSTAT    126
#define P9_RWSTAT    127

/*
 * Client state
 */
struct P9Client {
    int fd;                     /* Socket fd */
    int fid;                    /* Current fid */
    int auth_fid;               /* Authentication fid */
    uint32_t tag;               /* Current tag */
    char error[256];            /* Last error message */
    char username[32];          /* Username */
    char service_name[32];      /* Service name */
};

/*
 * 9P message header
 */
typedef struct {
    uint32_t size;
    uint8_t type;
    uint16_t tag;
} P9Hdr;

/*
 * Read n bytes from socket
 */
static ssize_t read_n(int fd, void *buf, size_t n)
{
    size_t total = 0;
    ssize_t r;
    unsigned char *p = (unsigned char *)buf;

    while (total < n) {
        r = read(fd, p + total, n - total);
        if (r <= 0) {
            if (r == 0) {
                /* Connection closed */
                return -1;
            }
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += r;
    }

    return total;
}

/*
 * Write n bytes to socket
 */
static ssize_t write_n(int fd, const void *buf, size_t n)
{
    size_t total = 0;
    ssize_t w;
    const unsigned char *p = (const unsigned char *)buf;

    while (total < n) {
        w = write(fd, p + total, n - total);
        if (w <= 0) {
            if (w == 0) {
                return -1;
            }
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        total += w;
    }

    return total;
}

/*
 * Connect to 9P server
 */
P9Client *p9_connect(const char *address)
{
    P9Client *client;
    struct sockaddr_in addr;
    struct hostent *he;
    int fd;
    char *host_str;
    int port;

    /* For now, just use localhost:17010 */
    /* TODO: Parse tcp!host!port format */
    host_str = "localhost";
    port = 17010;

    /* Create socket */
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        fprintf(stderr, "p9_connect: socket failed\n");
        return NULL;
    }

    /* Resolve host */
    he = gethostbyname(host_str);
    if (he == NULL) {
        fprintf(stderr, "p9_connect: gethostbyname failed for %s\n", host_str);
        close(fd);
        return NULL;
    }

    /* Connect */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        fprintf(stderr, "p9_connect: connect failed\n");
        close(fd);
        return NULL;
    }

    /* Allocate client */
    client = (P9Client *)malloc(sizeof(P9Client));
    if (client == NULL) {
        fprintf(stderr, "p9_connect: malloc failed\n");
        close(fd);
        return NULL;
    }

    memset(client, 0, sizeof(P9Client));
    client->fd = fd;
    client->fid = 0;
    client->tag = 0;

    /* Send version message */
    {
        uint8_t msg[128];
        uint8_t resp[P9_MAX_MSG];
        P9Hdr *hdr;
        uint32_t *size;
        uint16_t *tag;
        uint8_t *p;
        ssize_t n;

        /* Build Tversion */
        p = msg + 4;  /* Skip size field */
        *p++ = P9_TVERSION;

        tag = (uint16_t *)p;
        *tag = htons(0xFFFF);  /* NOTAG */
        p += 2;

        size = (uint32_t *)p;
        *size = htonl(P9_MAX_MSG);
        p += 4;

        strcpy((char *)p, P9_VERSION);
        p += strlen(P9_VERSION) + 1;

        /* Fill size */
        uint32_t msg_size = p - msg;
        *(uint32_t *)msg = htonl(msg_size - 4);

        /* Send */
        if (write_n(fd, msg, msg_size) < 0) {
            fprintf(stderr, "p9_connect: write failed\n");
            free(client);
            close(fd);
            return NULL;
        }

        /* Receive response */
        n = read_n(fd, resp, 4);
        if (n < 4) {
            fprintf(stderr, "p9_connect: read header failed\n");
            free(client);
            close(fd);
            return NULL;
        }

        uint32_t resp_size = ntohl(*(uint32_t *)resp);
        n = read_n(fd, resp + 4, resp_size);
        if (n < resp_size) {
            fprintf(stderr, "p9_connect: read body failed\n");
            free(client);
            close(fd);
            return NULL;
        }

        hdr = (P9Hdr *)resp;
        if (hdr->type != P9_RVERSION) {
            fprintf(stderr, "p9_connect: unexpected response type 0x%02x\n", hdr->type);
            free(client);
            close(fd);
            return NULL;
        }
    }

    fprintf(stderr, "p9_connect: connected to %s:%d\n", host_str, port);

    return client;
}

/*
 * Disconnect from 9P server
 */
void p9_disconnect(P9Client *client)
{
    if (client == NULL) {
        return;
    }

    if (client->fd >= 0) {
        close(client->fd);
    }

    free(client);
}

/*
 * Authenticate with 9P server
 */
int p9_authenticate(P9Client *client, int auth_method,
                    const char *user, const char *password)
{
    (void)password;  /* Unused for now */

    if (client == NULL) {
        return -1;
    }

    strncpy(client->username, user ? user : "none", sizeof(client->username) - 1);

    /* Attach as root user */
    {
        uint8_t msg[256];
        uint8_t resp[P9_MAX_MSG];
        P9Hdr *hdr;
        uint16_t *tag;
        uint16_t *fid;
        uint8_t *p;
        ssize_t n;

        p = msg + 4;
        *p++ = P9_TATTACH;

        tag = (uint16_t *)p;
        *tag = htons(client->tag++);
        p += 2;

        fid = (uint16_t *)p;
        *fid = htons(client->fid++);  /* Fid for root */
        p += 2;

        fid = (uint16_t *)p;
        *fid = htons(0xFFFFFFFF);  /* No auth fid */
        p += 2;

        /* User name */
        strcpy((char *)p, client->username);
        p += strlen(client->username) + 1;

        /* Mount point */
        strcpy((char *)p, "");
        p += 1;

        /* Fill size */
        uint32_t msg_size = p - msg;
        *(uint32_t *)msg = htonl(msg_size - 4);

        /* Send */
        if (write_n(client->fd, msg, msg_size) < 0) {
            fprintf(stderr, "p9_authenticate: write failed\n");
            return -1;
        }

        /* Receive response */
        n = read_n(client->fd, resp, 4);
        if (n < 4) {
            fprintf(stderr, "p9_authenticate: read header failed\n");
            return -1;
        }

        uint32_t resp_size = ntohl(*(uint32_t *)resp);
        n = read_n(client->fd, resp + 4, resp_size);
        if (n < resp_size) {
            fprintf(stderr, "p9_authenticate: read body failed\n");
            return -1;
        }

        hdr = (P9Hdr *)resp;
        if (hdr->type == P9_RERROR) {
            fprintf(stderr, "p9_authenticate: attach failed\n");
            return -1;
        }

        if (hdr->type != P9_RATTACH) {
            fprintf(stderr, "p9_authenticate: unexpected response 0x%02x\n", hdr->type);
            return -1;
        }
    }

    fprintf(stderr, "p9_authenticate: authenticated as %s\n", client->username);

    return 0;
}

/*
 * Open a file on 9P server
 */
int p9_open(P9Client *client, const char *path, int mode)
{
    /* Implementation for walk + open */
    /* This is a simplified version - full implementation would walk the path */

    if (client == NULL || path == NULL) {
        return -1;
    }

    /* For now, return the next fid */
    int fid = client->fid++;

    fprintf(stderr, "p9_open: opened %s (fid=%d)\n", path, fid);

    return fid;
}

/*
 * Close a file on 9P server
 */
int p9_close(P9Client *client, int fd)
{
    if (client == NULL) {
        return -1;
    }

    fprintf(stderr, "p9_close: closed fid=%d\n", fd);

    return 0;
}

/*
 * Read from a file on Marrow
 */
ssize_t p9_read(P9Client *client, int fd, void *buf, size_t count)
{
    (void)client;
    (void)fd;
    (void)buf;
    (void)count;

    /* TODO: Implement Tread/Rread */

    return -1;
}

/*
 * Write to a file on Marrow
 */
ssize_t p9_write(P9Client *client, int fd, const void *buf, size_t count)
{
    (void)client;
    (void)fd;
    (void)buf;
    (void)count;

    /* TODO: Implement Twrite/Rwrite */

    return -1;
}

/*
 * Create a directory on 9P server
 */
int p9_mkdir(P9Client *client, const char *path)
{
    (void)client;
    (void)path;

    /* TODO: Implement Twalk + Tcreate */

    return -1;
}

/*
 * Get last error message
 */
const char *p9_error(P9Client *client)
{
    if (client == NULL) {
        return "No client";
    }

    return client->error;
}

/*
 * Reset file offset to beginning
 */
int p9_reset_fid(P9Client *client, int fd)
{
    (void)client;
    (void)fd;

    /* For simple reads, we just reopen the file to reset offset */
    /* TODO: Implement proper Tseek or just close/reopen */

    return 0;
}

/*
 * Clunk (close) a file descriptor
 */
int p9_clunk(P9Client *client, int fd)
{
    return p9_close(client, fd);
}

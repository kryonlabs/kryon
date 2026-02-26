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
 * 64-bit network byte order conversion
 * Writes 64-bit value in network byte order (big-endian) directly to buffer
 * This avoids the endianness bug where returning a uint64_t and memcpy'ing it
 * causes the CPU to reinterpret the bytes in native byte order.
 */
static void htonll_bytes(uint8_t *buf, uint64_t value)
{
    /* Manually place bytes in big-endian order directly into output buffer */
    buf[0] = (value >> 56) & 0xFF;
    buf[1] = (value >> 48) & 0xFF;
    buf[2] = (value >> 40) & 0xFF;
    buf[3] = (value >> 32) & 0xFF;
    buf[4] = (value >> 24) & 0xFF;
    buf[5] = (value >> 16) & 0xFF;
    buf[6] = (value >> 8) & 0xFF;
    buf[7] = value & 0xFF;
}

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
    uint64_t fid_offsets[256];  /* Track offset for each FID */
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
    char host_buf[256];
    char *host_str;
    int port;
    char *port_str;
    char *p;

    /* Parse address formats:
     * - tcp!host!port (Plan 9 style)
     * - host:port (common format)
     * - host (default port)
     */
    if (address == NULL) {
        fprintf(stderr, "p9_connect: NULL address\n");
        return NULL;
    }

    /* Copy address to buffer for parsing */
    if (strlen(address) >= sizeof(host_buf) - 1) {
        fprintf(stderr, "p9_connect: address too long\n");
        return NULL;
    }
    strcpy(host_buf, address);

    /* Parse tcp!host!port format */
    if (strncmp(host_buf, "tcp!", 4) == 0) {
        p = host_buf + 4;
        host_str = p;

        /* Find second ! */
        p = strchr(p, '!');
        if (p != NULL) {
            *p = '\0';
            port_str = p + 1;
            port = atoi(port_str);
            if (port <= 0 || port > 65535) {
                fprintf(stderr, "p9_connect: invalid port in %s\n", address);
                return NULL;
            }
        } else {
            port = 17010;  /* Default Marrow port */
        }
    }
    /* Parse host:port format */
    else if (strchr(host_buf, ':') != NULL) {
        p = strchr(host_buf, ':');
        *p = '\0';
        host_str = host_buf;
        port_str = p + 1;
        port = atoi(port_str);
        if (port <= 0 || port > 65535) {
            fprintf(stderr, "p9_connect: invalid port in %s\n", address);
            return NULL;
        }
    }
    /* Just hostname, use default port */
    else {
        host_str = host_buf;
        port = 17010;  /* Default Marrow port */
    }

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
        fprintf(stderr, "p9_connect: connect to %s:%d failed\n", host_str, port);
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

    /* Initialize offset tracking for all FIDs */
    {
        int i;
        for (i = 0; i < 256; i++) {
            client->fid_offsets[i] = 0;
        }
    }

    /* Send version message */
    {
        uint8_t msg[128];
        uint8_t resp[P9_MAX_MSG];
        P9Hdr *hdr;
        uint16_t *tag;
        uint32_t *size;
        uint8_t *msg_ptr;
        uint32_t resp_size;
        uint32_t msg_size;
        ssize_t n;

        /* Build Tversion */
        msg_ptr = msg + 4;  /* Skip size field */
        *msg_ptr++ = P9_TVERSION;

        tag = (uint16_t *)msg_ptr;
        *tag = htons(0xFFFF);  /* NOTAG */
        msg_ptr += 2;

        size = (uint32_t *)msg_ptr;
        *size = htonl(P9_MAX_MSG);
        msg_ptr += 4;

        /* Version string with 2-byte length prefix (9P string format) */
        {
            uint16_t version_len = strlen(P9_VERSION);
            *(uint16_t *)msg_ptr = htons(version_len);
            msg_ptr += 2;
            memcpy(msg_ptr, P9_VERSION, version_len);
            msg_ptr += version_len;
        }

        /* Fill size */
        msg_size = msg_ptr - msg;
        *(uint32_t *)msg = htonl(msg_size);

        /* Send */
        if (write_n(fd, msg, msg_ptr - msg) < 0) {
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

        /* Debug: print raw bytes */
        fprintf(stderr, "p9_connect: received size bytes: %02x %02x %02x %02x\n",
                resp[0], resp[1], resp[2], resp[3]);

        resp_size = ntohl(*(uint32_t *)resp);
        fprintf(stderr, "p9_connect: resp_size (after ntohl): %u\n", resp_size);

        /* resp_size includes the 4-byte size field, so read only the body */
        n = read_n(fd, resp + 4, resp_size - 4);
        if (n < resp_size - 4) {
            fprintf(stderr, "p9_connect: read body failed (got %d, expected %u)\n",
                    (int)n, resp_size - 4);
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
        uint32_t *fid;
        uint8_t *p;
        uint32_t msg_size;
        uint32_t resp_size;
        ssize_t n;

        p = msg + 4;
        *p++ = P9_TATTACH;

        tag = (uint16_t *)p;
        *tag = htons(client->tag++);
        p += 2;

        fid = (uint32_t *)p;
        *fid = htonl(client->fid++);  /* Fid for root */
        p += 4;

        fid = (uint32_t *)p;
        *fid = htonl(0xFFFFFFFF);  /* No auth fid */
        p += 4;

        /* User name with 2-byte length prefix */
        {
            uint16_t uname_len = strlen(client->username);
            *(uint16_t *)p = htons(uname_len);
            p += 2;
            memcpy(p, client->username, uname_len);
            p += uname_len;
        }

        /* Mount point with 2-byte length prefix */
        {
            *(uint16_t *)p = htons(0);
            p += 2;
        }

        /* Fill size */
        msg_size = p - msg;
        *(uint32_t *)msg = htonl(msg_size);

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

        resp_size = ntohl(*(uint32_t *)resp);
        /* resp_size includes the 4-byte size field, so read only the body */
        n = read_n(client->fd, resp + 4, resp_size - 4);
        if (n < resp_size - 4) {
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
 * Uses Twalk to walk the path, then Topen to open the file
 */
int p9_open(P9Client *client, const char *path, int mode)
{
    uint8_t msg[512];
    uint8_t resp[P9_MAX_MSG];
    P9Hdr *hdr;
    uint16_t *tag_ptr;
    uint32_t *fid_ptr;
    uint8_t *p;
    uint32_t msg_size;
    uint32_t resp_size;
    ssize_t n;
    char path_copy[256];
    char *token;
    int fid;
    int root_fid;
    char *wnames[16];
    int nwname = 0;

    if (client == NULL || path == NULL) {
        return -1;
    }

    /* Skip leading / */
    if (path[0] == '/') {
        path++;
    }

    /* Copy path and tokenize */
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    token = strtok(path_copy, "/");
    while (token != NULL && nwname < 16) {
        wnames[nwname++] = token;
        token = strtok(NULL, "/");
    }

    if (nwname == 0) {
        fprintf(stderr, "p9_open: invalid path '%s'\n", path);
        return -1;
    }

    fid = client->fid++;
    root_fid = 0;  /* Root fid from Tattach */

    /* Build Twalk message: size[4] Twalk tag[2] fid[4] newfid[4] nwname[2] wname[nwname][s] */
    p = msg + 4;  /* Skip size field */
    *p++ = P9_TWALK;

    tag_ptr = (uint16_t *)p;
    *tag_ptr = htons(client->tag++);
    p += 2;

    fid_ptr = (uint32_t *)p;
    *fid_ptr = htonl(root_fid);
    p += 4;

    fid_ptr = (uint32_t *)p;
    *fid_ptr = htonl(fid);
    p += 4;

    *(uint16_t *)p = htons(nwname);
    p += 2;

    /* Add path components */
    {
        int i;
        for (i = 0; i < nwname; i++) {
            uint16_t len = strlen(wnames[i]);
            *(uint16_t *)p = htons(len);
            p += 2;
            memcpy(p, wnames[i], len);
            p += len;
        }
    }

    /* Fill size */
    msg_size = p - msg;
    *(uint32_t *)msg = htonl(msg_size);

    /* Send Twalk */
    if (write_n(client->fd, msg, msg_size) < 0) {
        fprintf(stderr, "p9_open: Twalk write failed\n");
        return -1;
    }

    /* Receive response */
    n = read_n(client->fd, resp, 4);
    if (n < 4) {
        fprintf(stderr, "p9_open: Twalk read header failed\n");
        return -1;
    }

    resp_size = ntohl(*(uint32_t *)resp);
    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_open: Twalk read body failed\n");
        return -1;
    }

    hdr = (P9Hdr *)resp;
    if (hdr->type == P9_RERROR) {
        fprintf(stderr, "p9_open: Twalk failed for '%s'\n", path);
        return -1;
    }

    if (hdr->type != P9_RWALK) {
        fprintf(stderr, "p9_open: unexpected Twalk response type 0x%02x\n", hdr->type);
        return -1;
    }

    /* Now Topen the fid */
    p = msg + 4;
    *p++ = P9_TOPEN;

    tag_ptr = (uint16_t *)p;
    *tag_ptr = htons(client->tag++);
    p += 2;

    fid_ptr = (uint32_t *)p;
    *fid_ptr = htonl(fid);
    p += 4;

    *p++ = mode;

    msg_size = p - msg;
    *(uint32_t *)msg = htonl(msg_size);

    /* Send Topen */
    if (write_n(client->fd, msg, msg_size) < 0) {
        fprintf(stderr, "p9_open: Topen write failed\n");
        return -1;
    }

    /* Receive response */
    n = read_n(client->fd, resp, 4);
    if (n < 4) {
        fprintf(stderr, "p9_open: Topen read header failed\n");
        return -1;
    }

    resp_size = ntohl(*(uint32_t *)resp);
    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_open: Topen read body failed\n");
        return -1;
    }

    hdr = (P9Hdr *)resp;
    if (hdr->type == P9_RERROR) {
        fprintf(stderr, "p9_open: Topen failed for '%s'\n", path);
        return -1;
    }

    if (hdr->type != P9_ROPEN) {
        fprintf(stderr, "p9_open: unexpected Topen response type 0x%02x\n", hdr->type);
        return -1;
    }

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
 * Read from a file on 9P server
 */
ssize_t p9_read(P9Client *client, int fd, void *buf, size_t count)
{
    uint8_t msg[256];
    uint8_t resp[P9_MAX_MSG];
    P9Hdr *hdr;
    uint16_t *tag_ptr;
    uint32_t *fid_ptr;
    uint32_t *count_ptr;
    uint64_t offset;
    uint8_t *p;
    uint32_t msg_size;
    uint32_t resp_size;
    ssize_t n;
    uint32_t data_count;

    if (client == NULL || buf == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= 256) {
        fprintf(stderr, "p9_read: invalid fd %d\n", fd);
        return -1;
    }

    /* Get current offset for this FID */
    offset = client->fid_offsets[fd];

    /* Build Tread message: size[4] Tread tag[2] fid[4] offset[8] count[4] */
    p = msg + 4;  /* Skip size field */
    *p++ = P9_TREAD;

    tag_ptr = (uint16_t *)p;
    *tag_ptr = htons(client->tag++);
    p += 2;

    fid_ptr = (uint32_t *)p;
    *fid_ptr = htonl((uint32_t)fd);
    p += 4;

    /* Encode 64-bit offset directly to buffer in network byte order */
    htonll_bytes(p, offset);

    /* Debug: print raw bytes of offset */
    fprintf(stderr, "p9_read: offset=%llu raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            (unsigned long long)offset,
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

    p += 8;

    count_ptr = (uint32_t *)p;
    *count_ptr = htonl((uint32_t)count);
    p += 4;

    /* Fill size */
    msg_size = p - msg;
    *(uint32_t *)msg = htonl(msg_size);

    /* Debug: log offset being sent */
    fprintf(stderr, "p9_read: fd=%d offset=%llu count=%zu\n",
            (unsigned long long)fd, (unsigned long long)offset, count);

    /* Send Tread */
    if (write_n(client->fd, msg, msg_size) < 0) {
        fprintf(stderr, "p9_read: write failed\n");
        return -1;
    }

    /* Receive response */
    n = read_n(client->fd, resp, 4);
    if (n < 4) {
        fprintf(stderr, "p9_read: read header failed\n");
        return -1;
    }

    resp_size = ntohl(*(uint32_t *)resp);
    /* resp_size includes the 4-byte size field, so read only the body */
    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_read: read body failed (got %d, expected %u)\n",
                (int)n, resp_size - 4);
        return -1;
    }

    hdr = (P9Hdr *)resp;
    if (hdr->type == P9_RERROR) {
        fprintf(stderr, "p9_read: server returned error\n");
        return -1;
    }

    if (hdr->type != P9_RREAD) {
        fprintf(stderr, "p9_read: unexpected response type 0x%02x\n", hdr->type);
        return -1;
    }

    /* Parse Rread: size[4] Rread tag[2] count[4] data[count] */
    p = resp + 7;  /* Skip header */
    data_count = ntohl(*(uint32_t *)p);
    p += 4;

    /* Copy data to buffer */
    if (data_count > count) {
        data_count = count;
    }
    memcpy(buf, p, data_count);

    /* Update offset for next read */
    client->fid_offsets[fd] += data_count;

    return (ssize_t)data_count;
}

/*
 * Write to a file on Marrow
 */
ssize_t p9_write(P9Client *client, int fd, const void *buf, size_t count)
{
    uint8_t msg[P9_MAX_MSG];
    uint8_t resp[P9_MAX_MSG];
    uint8_t *p;
    uint32_t *fid_ptr;
    uint64_t *offset_ptr;
    uint32_t *count_ptr;
    uint16_t *tag_ptr;
    uint32_t msg_size;
    uint32_t resp_size;
    ssize_t n;
    P9Hdr *hdr;
    uint64_t offset;

    if (client == NULL || buf == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= 256) {
        fprintf(stderr, "p9_write: invalid fd %d\n", fd);
        return -1;
    }

    /* Get current offset for this FID */
    offset = client->fid_offsets[fd];

    /* Build Twrite message */
    p = msg + 4;  /* Skip size field */
    *p++ = P9_TWRITE;

    /* Tag */
    tag_ptr = (uint16_t *)p;
    *tag_ptr = htons(client->tag++);
    p += 2;

    /* FID */
    fid_ptr = (uint32_t *)p;
    *fid_ptr = htonl(fd);
    p += 4;

    /* Offset - write directly to buffer in network byte order */
    htonll_bytes(p, offset);

    /* Debug: print raw bytes of offset */
    fprintf(stderr, "p9_write: fd=%d offset=%llu raw bytes: %02X %02X %02X %02X %02X %02X %02X %02X\n",
            (unsigned long long)fd, (unsigned long long)offset,
            p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);

    p += 8;

    /* Count */
    count_ptr = (uint32_t *)p;
    *count_ptr = htonl(count);
    p += 4;

    /* Data */
    if (p + count > msg + sizeof(msg)) {
        fprintf(stderr, "p9_write: data too large (%d bytes)\n", (int)count);
        return -1;
    }
    memcpy(p, buf, count);
    p += count;

    /* Fill size */
    msg_size = p - msg;
    *(uint32_t *)msg = htonl(msg_size);

    /* Debug: log offset being sent */
    fprintf(stderr, "p9_write: fd=%d offset=%llu count=%zu\n",
            (unsigned long long)fd, (unsigned long long)offset, count);

    /* Send Twrite */
    if (write_n(client->fd, msg, msg_size) < 0) {
        fprintf(stderr, "p9_write: Twrite send failed\n");
        return -1;
    }

    /* Receive response */
    n = read_n(client->fd, resp, 4);
    if (n < 4) {
        fprintf(stderr, "p9_write: Twrite read header failed\n");
        return -1;
    }

    resp_size = ntohl(*(uint32_t *)resp);
    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_write: Twrite read body failed\n");
        return -1;
    }

    hdr = (P9Hdr *)resp;
    if (hdr->type == P9_RERROR) {
        fprintf(stderr, "p9_write: server returned error\n");
        return -1;
    }

    if (hdr->type != P9_RWRITE) {
        fprintf(stderr, "p9_write: unexpected response type 0x%02x\n", hdr->type);
        return -1;
    }

    /* Update offset for next write */
    client->fid_offsets[fd] += count;

    return (ssize_t)count;
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
    if (client == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= 256) {
        fprintf(stderr, "p9_reset_fid: invalid fd %d\n", fd);
        return -1;
    }

    /* Reset offset to 0 for this FID */
    client->fid_offsets[fd] = 0;

    fprintf(stderr, "p9_reset_fid: reset fd=%d offset to 0\n", fd);

    return 0;
}

/*
 * Clunk (close) a file descriptor
 */
int p9_clunk(P9Client *client, int fd)
{
    return p9_close(client, fd);
}

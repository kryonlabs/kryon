/*
 * Kryon 9P Client Implementation
 * C89/C90 compliant
 */

#include "p9client.h"
#include "kryon.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

/* Network includes */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close
#endif

/*
 * String parsing helper (from 9p.c)
 */
static size_t p9_get_string(const uint8_t *buf, char *str, size_t max_len)
{
    uint16_t len = le_get16(buf);
    if (len >= max_len) {
        len = max_len - 1;
    }
    memcpy(str, buf + 2, len);
    str[len] = '\0';
    return 2 + len;
}

/*
 * Parse Rversion message
 */
static int p9_parse_rversion(const uint8_t *buf, size_t len, uint32_t *msize, char *version)
{
    const uint8_t *p = buf + 7;  /* Skip header */

    if (len < 7 + 4 + 2) {
        return -1;
    }

    *msize = le_get32(p);
    p += 4;

    p += p9_get_string(p, version, P9_MAX_VERSION);

    return 0;
}

/*
 * String encoding helper
 */
static size_t p9_put_string(uint8_t *buf, const char *str)
{
    size_t len = strlen(str);
    if (len > P9_MAX_STR) {
        len = P9_MAX_STR;
    }
    le_put16(buf, (uint16_t)len);
    memcpy(buf + 2, str, len);
    return 2 + len;
}

/* Network includes */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
typedef int socklen_t;
#else
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#define closesocket close
#endif

/*
 * FID state tracking for offset management
 */
#define MAX_FIDS 256

typedef struct {
    int fid;
    uint64_t offset;
    int in_use;
} FidState;

static FidState fid_states[MAX_FIDS];
static int fid_states_initialized = 0;

/*
 * Initialize FID state tracking
 */
static void fid_states_init(void)
{
    int i;
    if (fid_states_initialized) {
        return;
    }
    for (i = 0; i < MAX_FIDS; i++) {
        fid_states[i].fid = 0;
        fid_states[i].offset = 0;
        fid_states[i].in_use = 0;
    }
    fid_states_initialized = 1;
}

/*
 * Get FID state
 */
static FidState *get_fid_state(int fid)
{
    if (!fid_states_initialized || fid < 0 || fid >= MAX_FIDS) {
        return NULL;
    }
    if (!fid_states[fid].in_use) {
        return NULL;
    }
    return &fid_states[fid];
}

/*
 * Allocate FID state
 */
static FidState *alloc_fid_state(int fid)
{
    if (!fid_states_initialized) {
        fid_states_init();
    }
    if (fid < 0 || fid >= MAX_FIDS) {
        return NULL;
    }
    if (fid_states[fid].in_use) {
        /* Already in use */
        return NULL;
    }
    fid_states[fid].fid = fid;
    fid_states[fid].offset = 0;
    fid_states[fid].in_use = 1;
    return &fid_states[fid];
}

/*
 * Free FID state
 */
static void free_fid_state(int fid)
{
    if (!fid_states_initialized || fid < 0 || fid >= MAX_FIDS) {
        return;
    }
    fid_states[fid].in_use = 0;
    fid_states[fid].offset = 0;
}

/*
 * Receive message (first 4 bytes contain the length)
 */
static int recv_msg(int fd, uint8_t *buf, size_t bufsize)
{
    uint32_t msg_size;
    ssize_t n;
    size_t total;

    /* Read first 4 bytes to get message length */
    total = 0;
    while (total < 4) {
        n = recv(fd, (char *)(buf + total), 4 - total, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        total += n;
    }

    /* Convert from little-endian */
    msg_size = le_get32(buf);

    if (msg_size < 7 || msg_size > bufsize) {
        fprintf(stderr, "recv_msg: invalid message size: %u\n", msg_size);
        return -1;
    }

    /* Read rest of message (we already have the first 4 bytes) */
    while (total < msg_size) {
        n = recv(fd, (char *)(buf + total), msg_size - total, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        total += n;
    }

    return (int)msg_size;
}

/*
 * Send message (buffer already includes 4-byte length prefix)
 */
static int send_msg(int fd, const uint8_t *buf, size_t len)
{
    ssize_t n;
    size_t total;

    /* Send entire message (buffer has length prefix at offset 0) */
    total = 0;
    while (total < len) {
        n = send(fd, (char *)(buf + total), len - total, 0);
        if (n < 0) {
            return -1;
        }
        if (n == 0) {
            return -1;
        }
        total += n;
    }

    return 0;
}

/*
 * Allocate new FID
 */
static uint32_t alloc_fid(P9Client *client)
{
    return client->next_fid++;
}

/*
 * Allocate new tag
 */
static uint16_t alloc_tag(P9Client *client)
{
    uint16_t tag = client->next_tag++;
    if (tag == 0 || tag >= P9_MAX_TAG) {
        client->next_tag = 1;
        tag = 1;
    }
    return tag;
}

/*
 * Forward declaration of p9_parse_rerror
 */
static int p9_parse_rerror(const uint8_t *buf, size_t len, char *ename, size_t ename_size);

/*
 * Connect to Kryon server
 */
P9Client *p9_client_connect(const char *host, int port)
{
    P9Client *client;
    struct sockaddr_in addr;
    struct hostent *he;
    int flag;
#ifdef _WIN32
    WSADATA wsa_data;
#endif

    /* Initialize Winsock on Windows */
#ifdef _WIN32
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0) {
        return NULL;
    }
#endif

    client = (P9Client *)calloc(1, sizeof(P9Client));
    if (client == NULL) {
        return NULL;
    }

    /* Create socket */
    client->fd = socket(AF_INET, SOCK_STREAM, 0);
    if (client->fd < 0) {
        sprintf(client->error_msg, "socket: %s", strerror(errno));
        free(client);
        return NULL;
    }

    /* Resolve host */
    he = gethostbyname(host);
    if (he == NULL) {
        sprintf(client->error_msg, "gethostbyname: %s", host);
        closesocket(client->fd);
        free(client);
        return NULL;
    }

    /* Connect */
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    memcpy(&addr.sin_addr, he->h_addr_list[0], he->h_length);

    if (connect(client->fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        sprintf(client->error_msg, "connect: %s", strerror(errno));
        closesocket(client->fd);
        free(client);
        return NULL;
    }

    /* Disable Nagle's algorithm for lower latency */
    flag = 1;
    setsockopt(client->fd, IPPROTO_TCP, TCP_NODELAY,
               (char *)&flag, sizeof(flag));

    /* Initialize client state */
    client->next_tag = 1;
    client->next_fid = 1;  /* FID 0 is reserved for root */
    client->root_fid = 0;
    client->connected = 1;
    client->error = 0;

    return client;
}

/*
 * Disconnect and cleanup
 */
void p9_client_disconnect(P9Client *client)
{
    if (client == NULL) {
        return;
    }

    if (client->fd >= 0) {
        closesocket(client->fd);
    }

    if (client->aname != NULL) {
        free(client->aname);
    }

    free(client);

#ifdef _WIN32
    WSACleanup();
#endif
}

/*
 * Send version message and get response
 */
static int do_version(P9Client *client)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    uint8_t *p;
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    char version[P9_MAX_VERSION];
    uint32_t msize;

    /* Build Tversion */
    p = out_buf + 7;  /* Skip header */
    le_put32(p, P9_MAX_MSG);  /* msize */
    p += 4;
    p += p9_put_string(p, "9P2000");  /* version */
    out_len = p - out_buf;

    /* Write header */
    le_put32(out_buf, out_len);
    out_buf[4] = Tversion;
    le_put16(out_buf + 5, 0);  /* tag */

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Tversion failed");
        return -1;
    }

    /* Receive Rversion */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rversion failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rversion header failed");
        return -1;
    }

    if (hdr.type != Rversion) {
        if (hdr.type == Rerror) {
            p9_parse_rerror(in_buf, in_len, client->error_msg, sizeof(client->error_msg));
            fprintf(stderr, "Debug: Tversion got Rerror: '%s'\n", client->error_msg);
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
            fprintf(stderr, "Debug: Tversion expected Rversion(%d), got type=%d\n", Rversion, hdr.type);
        }
        return -1;
    }

    if (p9_parse_rversion(in_buf, in_len, &msize, version) < 0) {
        sprintf(client->error_msg, "parse Rversion failed");
        return -1;
    }

    if (strcmp(version, "9P2000") != 0) {
        sprintf(client->error_msg, "unsupported version: %s", version);
        return -1;
    }

    return 0;
}

/*
 * Attach to server root
 */
int p9_client_attach(P9Client *client, const char *path)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;
    const char *aname;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    /* Do version exchange first */
    if (do_version(client) < 0) {
        return -1;
    }

    /* Build Tattach */
    aname = (path != NULL) ? path : "";
    tag = alloc_tag(client);
    client->root_fid = alloc_fid(client);

    out_len = 7;  /* header */
    le_put32(out_buf + out_len, client->root_fid);  /* fid */
    out_len += 4;
    le_put32(out_buf + out_len, 0xFFFFFFFF);  /* afid (no auth) */
    out_len += 4;
    out_len += p9_put_string(out_buf + out_len, "none");  /* uname */
    out_len += p9_put_string(out_buf + out_len, aname);  /* aname */

    p9_build_header(out_buf, Tattach, tag, out_len - 7);

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Tattach failed");
        return -1;
    }

    /* Receive Rattach */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rattach failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rattach header failed");
        return -1;
    }

    if (hdr.type != Rattach) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            if (ename_len >= sizeof(client->error_msg)) {
                ename_len = sizeof(client->error_msg) - 1;
            }
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
            fprintf(stderr, "Debug: Received Rerror: '%s' (len=%d)\n", client->error_msg, ename_len);
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
            fprintf(stderr, "Debug: Expected Rattach(%d), got type=%d tag=%d\n", Rattach, hdr.type, hdr.tag);
        }
        return -1;
    }

    /* Allocate FID state for root fid */
    alloc_fid_state(client->root_fid);

    /* Save attach name */
    client->aname = (char *)malloc(strlen(aname) + 1);
    if (client->aname != NULL) {
        strcpy(client->aname, aname);
    }

    return 0;
}

/*
 * Walk to a path
 * Returns new fid on success, -1 on failure
 * Supports multi-component walks (e.g., "/dev/screen")
 */
int p9_client_walk(P9Client *client, const char *path)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;
    uint32_t newfid;
    char path_copy[P9_MAX_STR];
    char *component;
    int nwname;
    uint8_t *wname_ptr;
    int i;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    if (path == NULL || path[0] == '\0') {
        return client->root_fid;
    }

    newfid = alloc_fid(client);
    tag = alloc_tag(client);

    /* Copy path for parsing */
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    /* Skip leading slash */
    if (path_copy[0] == '/') {
        char *p = path_copy;
        while (*p == '/') p++;
        memmove(path_copy, p, strlen(p) + 1);
    }

    /* Empty path after removing slashes */
    if (path_copy[0] == '\0') {
        return client->root_fid;
    }

    /* Count path components */
    nwname = 0;
    for (i = 0; path_copy[i] != '\0'; i++) {
        if (path_copy[i] == '/') {
            nwname++;
        }
    }
    nwname++;  /* Last component */

    /* 9P limits the number of walk components */
    if (nwname > P9_MAX_WELEM) {
        sprintf(client->error_msg, "path too long");
        return -1;
    }

    /* Build Twalk message */
    out_len = 7;  /* header */
    le_put32(out_buf + out_len, client->root_fid);  /* fid */
    out_len += 4;
    le_put32(out_buf + out_len, newfid);  /* newfid */
    out_len += 4;
    le_put16(out_buf + out_len, (uint16_t)nwname);  /* nwname */
    out_len += 2;

    /* Add path components */
    wname_ptr = out_buf + out_len;
    component = strtok(path_copy, "/");
    i = 0;
    while (component != NULL && i < nwname) {
        size_t len = strlen(component);
        le_put16(out_buf + out_len, (uint16_t)len);
        out_len += 2;
        strcpy((char *)(out_buf + out_len), component);
        out_len += len;
        component = strtok(NULL, "/");
        i++;
    }

    p9_build_header(out_buf, Twalk, tag, out_len - 7);

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Twalk failed");
        return -1;
    }

    /* Receive Rwalk */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rwalk failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rwalk header failed");
        return -1;
    }

    if (hdr.type != Rwalk) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
        }
        return -1;
    }

    /* Allocate FID state to track offset */
    if (alloc_fid_state(newfid) == NULL) {
        sprintf(client->error_msg, "failed to track fid state");
        return -1;
    }

    return newfid;
}

/*
 * Open a file
 */
int p9_client_open(P9Client *client, int fid, int mode)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    tag = alloc_tag(client);

    /* Build Topen */
    out_len = 7;  /* header */
    le_put32(out_buf + out_len, (uint32_t)fid);  /* fid */
    out_len += 4;
    out_buf[out_len++] = (uint8_t)mode;  /* mode */

    p9_build_header(out_buf, Topen, tag, out_len - 7);

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Topen failed");
        return -1;
    }

    /* Receive Ropen */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Ropen failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Ropen header failed");
        return -1;
    }

    if (hdr.type != Ropen) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
        }
        return -1;
    }

    return 0;
}

/*
 * Read from an open file
 */
ssize_t p9_client_read(P9Client *client, int fid, char *buf, size_t count)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;
    uint32_t data_count;
    const uint8_t *p;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    tag = alloc_tag(client);

    /* Get current offset for this FID */
    {
        FidState *fid_state = get_fid_state(fid);
        uint64_t current_offset = 0;
        if (fid_state != NULL) {
            current_offset = fid_state->offset;
        }

        /* Build Tread */
        out_len = 7;  /* header */
        le_put32(out_buf + out_len, (uint32_t)fid);  /* fid */
        out_len += 4;
        le_put64(out_buf + out_len, current_offset);  /* offset */
        out_len += 8;
        le_put32(out_buf + out_len, (uint32_t)count);  /* count */
        out_len += 4;

        p9_build_header(out_buf, Tread, tag, 16);

        if (send_msg(client->fd, out_buf, out_len) < 0) {
            sprintf(client->error_msg, "send Tread failed");
            return -1;
        }

        }

    /* Receive Rread */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rread failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rread header failed");
        return -1;
    }

    if (hdr.type != Rread) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
        }
        return -1;
    }

    /* Parse data */
    p = in_buf + 7;
    data_count = le_get32(p);
    p += 4;

    if (data_count > count) {
        data_count = (uint32_t)count;
    }

    memcpy(buf, p, data_count);

    /* Update offset for this FID */
    {
        FidState *fid_state = get_fid_state(fid);
        if (fid_state != NULL) {
            fid_state->offset += data_count;
        }
    }

    return data_count;
}

/*
 * Write to an open file
 */
ssize_t p9_client_write(P9Client *client, int fid, const char *buf, size_t count)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    tag = alloc_tag(client);

    /* Build Twrite */
    out_len = 7;  /* header */
    le_put32(out_buf + out_len, (uint32_t)fid);  /* fid */
    out_len += 4;
    le_put64(out_buf + out_len, 0);  /* offset (TODO: support offset) */
    out_len += 8;
    le_put32(out_buf + out_len, (uint32_t)count);  /* count */
    out_len += 4;
    memcpy(out_buf + out_len, buf, count);
    out_len += count;

    p9_build_header(out_buf, Twrite, tag, out_len - 7);

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Twrite failed");
        return -1;
    }

    /* Receive Rwrite */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rwrite failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rwrite header failed");
        return -1;
    }

    if (hdr.type != Rwrite) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
        }
        return -1;
    }

    return count;
}

/*
 * Close/clunk a FID
 */
int p9_client_clunk(P9Client *client, int fid)
{
    uint8_t out_buf[P9_MAX_MSG];
    uint8_t in_buf[P9_MAX_MSG];
    size_t out_len;
    int in_len;
    P9Hdr hdr;
    uint16_t tag;

    if (client == NULL || !client->connected) {
        sprintf(client->error_msg, "not connected");
        return -1;
    }

    tag = alloc_tag(client);

    /* Build Tclunk */
    out_len = 7;  /* header */
    le_put32(out_buf + out_len, (uint32_t)fid);  /* fid */
    out_len += 4;

    p9_build_header(out_buf, Tclunk, tag, 4);

    if (send_msg(client->fd, out_buf, out_len) < 0) {
        sprintf(client->error_msg, "send Tclunk failed");
        return -1;
    }

    /* Receive Rclunk */
    in_len = recv_msg(client->fd, in_buf, sizeof(in_buf));
    if (in_len < 7) {
        sprintf(client->error_msg, "recv Rclunk failed");
        return -1;
    }

    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        sprintf(client->error_msg, "parse Rclunk header failed");
        return -1;
    }

    if (hdr.type != Rclunk) {
        if (hdr.type == Rerror) {
            uint16_t ename_len;
            const uint8_t *p = in_buf + 7;
            ename_len = le_get16(p);
            p += 2;
            memcpy(client->error_msg, p, ename_len);
            client->error_msg[ename_len] = '\0';
        } else {
            sprintf(client->error_msg, "unexpected message type: %d", hdr.type);
        }
        return -1;
    }

    /* Free FID state */
    free_fid_state(fid);

    return 0;
}

/*
 * Reset FID offset to 0 (for re-reading files from beginning)
 */
void p9_client_reset_fid(P9Client *client, int fid)
{
    FidState *fid_state;

    if (client == NULL) {
        return;
    }

    fid_state = get_fid_state(fid);
    if (fid_state != NULL) {
        fid_state->offset = 0;
    }
}

/*
 * Utility: walk and open combined
 */
int p9_client_open_path(P9Client *client, const char *path, int mode)
{
    int fid;

    fid = p9_client_walk(client, path);
    if (fid < 0) {
        return -1;
    }

    if (p9_client_open(client, fid, mode) < 0) {
        return -1;
    }

    return fid;
}

/*
 * Get last error message
 */
const char *p9_client_error(P9Client *client)
{
    if (client == NULL) {
        return "null client";
    }
    return client->error_msg;
}

/*
 * Utility: read entire file
 */
char *p9_client_read_file(P9Client *client, const char *path, size_t *size)
{
    int fid;
    char *buf;
    char *new_buf;
    size_t capacity = 4096;
    size_t len = 0;
    ssize_t n;

    if (size != NULL) {
        *size = 0;
    }

    fid = p9_client_open_path(client, path, P9_OREAD);
    if (fid < 0) {
        return NULL;
    }

    buf = (char *)malloc(capacity);
    if (buf == NULL) {
        p9_client_clunk(client, fid);
        return NULL;
    }

    while (1) {
        n = p9_client_read(client, fid, buf + len, capacity - len);
        if (n < 0) {
            free(buf);
            p9_client_clunk(client, fid);
            return NULL;
        }
        if (n == 0) {
            break;
        }

        len += n;

        if (len >= capacity) {
            capacity *= 2;
            new_buf = (char *)realloc(buf, capacity);
            if (new_buf == NULL) {
                free(buf);
                p9_client_clunk(client, fid);
                return NULL;
            }
            buf = new_buf;
        }
    }

    p9_client_clunk(client, fid);

    if (size != NULL) {
        *size = len;
    }

    return buf;
}

/*
 * Parse Rerror message
 */
static int p9_parse_rerror(const uint8_t *buf, size_t len, char *ename, size_t ename_size)
{
    const uint8_t *p = buf + 7;
    uint16_t ename_len;

    if (len < 7 + 2) {
        return -1;
    }

    ename_len = le_get16(p);
    p += 2;

    if (ename_len >= ename_size) {
        ename_len = ename_size - 1;
    }

    memcpy(ename, p, ename_len);
    ename[ename_len] = '\0';

    return 0;
}

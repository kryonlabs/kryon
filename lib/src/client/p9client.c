/*
 * 9P Client Implementation
 * C89/C90 compliant
 *
 * Generic 9P client for connecting to any 9P server
 */

#include "p9client.h"
#include "kryon.h"  /* For P9Node type and lib9's Fcall */
#include <lib9.h>
#include <fcall.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>

/* Undefine plan9port macros that conflict with POSIX socket functions */
#undef accept
#undef bind
#undef listen

/* Undefine plan9port macros to use standard C library */
#undef malloc
#undef free
#undef realloc
#undef atoi
#undef time
#undef read
#undef write
#undef close

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

/*
 * 9P message constants
 */
#define P9_MAX_MSG    8192
#define P9_VERSION    "9P2000"

/*
 * FID state structure
 */
typedef struct {
    uint32_t fid;
    int is_stream;
    uint64_t offset;
    int in_use;
} P9FID;

#define MAX_FIDS 256

/*
 * Client state
 */
struct P9Client {
    int fd;                     /* Socket fd */
    int fid;                    /* Current fid */
    int auth_fid;               /* Authentication fid */
    int root_fid;               /* Root fid from Tattach */
    uint32_t tag;               /* Current tag */
    char error[256];            /* Last error message */
    char username[32];          /* Username */
    char service_name[32];      /* Service name */
    P9FID fids[MAX_FIDS];       /* Track FID state including streaming */
};

/*
 * Namespace context
 * Current namespace root for bind mount resolution
 * This is process-global (not per-connection) for simplicity
 * In a multi-threaded environment, this would be thread-local
 */
static P9Node *current_namespace_root = NULL;

/*
 * Path cache for namespace resolution
 * Reduces O(n) tree traversal to O(1) cache lookup for repeated paths
 */
#define PATH_CACHE_SIZE 16
static const char *path_cache_original[PATH_CACHE_SIZE];
static char path_cache_resolved[PATH_CACHE_SIZE][512];
static int path_cache_next = 0;

/*
 * Add entry to path cache
 */
static void path_cache_add(const char *original, const char *resolved)
{
    int idx = path_cache_next;
    path_cache_next = (path_cache_next + 1) % PATH_CACHE_SIZE;

    path_cache_original[idx] = original;
    strncpy(path_cache_resolved[idx], resolved, sizeof(path_cache_resolved[idx]) - 1);
    path_cache_resolved[idx][sizeof(path_cache_resolved[idx]) - 1] = '\0';
}

/*
 * Lookup path in cache
 * Returns resolved path if found, NULL otherwise
 */
static const char *path_cache_lookup(const char *original)
{
    int i;
    for (i = 0; i < PATH_CACHE_SIZE; i++) {
        if (path_cache_original[i] != NULL &&
            strcmp(path_cache_original[i], original) == 0) {
            return path_cache_resolved[i];
        }
    }
    return NULL;
}

/*
 * Invalidate path cache (call when namespace changes)
 */
static void path_cache_invalidate(void)
{
    int i;
    for (i = 0; i < PATH_CACHE_SIZE; i++) {
        path_cache_original[i] = NULL;
        path_cache_resolved[i][0] = '\0';
    }
    path_cache_next = 0;
}

/*
 * Resolve a path through the current namespace's bind mounts
 * Returns the actual Marrow path to open
 *
 * Example:
 *   Input:  "/dev/screen"
 *   Output: "/dev/win1/screen" (after following bind mount)
 */
static const char *namespace_resolve_path(const char *path)
{
    P9Node *resolved;
    static char resolved_path[512];  /* Note: not thread-safe, use TLS in prod */
    const char *cached;

    if (current_namespace_root == NULL) {
        return path;  /* No namespace, use path as-is */
    }

    /* Check cache first */
    cached = path_cache_lookup(path);
    if (cached != NULL) {
        return cached;
    }

    /* Not in cache, resolve the path */
    resolved = tree_resolve_path(current_namespace_root, path);
    if (resolved != NULL && resolved->is_bind && resolved->bind_target != NULL) {
        /* Return bind target (Marrow path) */
        strncpy(resolved_path, resolved->bind_target, sizeof(resolved_path) - 1);
        resolved_path[sizeof(resolved_path) - 1] = '\0';

        /* Add to cache for future lookups */
        path_cache_add(path, resolved_path);

        return resolved_path;
    }

    /* Path not found in namespace or not a bind, use as-is */
    return path;
}

/*
 * Set current namespace for this thread/connection
 * This affects all subsequent 9P operations
 */
void p9_set_namespace(P9Node *ns_root)
{
    current_namespace_root = ns_root;
    path_cache_invalidate();  /* Clear cache when namespace changes */
}

/*
 * Get current namespace
 */
P9Node *p9_get_namespace(void)
{
    return current_namespace_root;
}

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
    strecpy(host_buf, host_buf + sizeof(host_buf), address);

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

    /* Set 5-second socket timeout to prevent infinite hangs */
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) < 0) {
        fprintf(stderr, "p9_connect: warning - failed to set receive timeout\n");
    }
    if (setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv)) < 0) {
        fprintf(stderr, "p9_connect: warning - failed to set send timeout\n");
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
    client->auth_fid = -1;
    client->root_fid = -1;
    client->tag = 0;

    /* Initialize FID tracking for all FIDs */
    {
        int i;
        for (i = 0; i < MAX_FIDS; i++) {
            client->fids[i].fid = 0;
            client->fids[i].is_stream = 0;
            client->fids[i].offset = 0;
            client->fids[i].in_use = 0;
        }
    }

    /* Send version message */
    {
        uint8_t msg[128];
        uint8_t resp[P9_MAX_MSG];
        Fcall f;
        uint msg_size;
        ssize_t n;

        /* Build Tversion using lib9 */
        memset(&f, 0, sizeof(f));
        f.type = Tversion;
        f.tag = NOTAG;
        f.msize = P9_MAX_MSG;
        f.version = P9_VERSION;

        msg_size = convS2M(&f, msg, sizeof(msg));
        if (msg_size == 0) {
            fprintf(stderr, "p9_connect: convS2M failed\n");
            free(client);
            close(fd);
            return NULL;
        }

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

        /* Parse response size */
        {
            uint32_t resp_size = GBIT32(resp);
            fprintf(stderr, "p9_connect: resp_size: %u\n", resp_size);

            /* Read response body */
            n = read_n(fd, resp + 4, resp_size - 4);
            if (n < resp_size - 4) {
                fprintf(stderr, "p9_connect: read body failed (got %d, expected %u)\n",
                        (int)n, resp_size - 4);
                free(client);
                close(fd);
                return NULL;
            }

            /* Parse response using lib9 */
            memset(&f, 0, sizeof(f));
            if (convM2S(resp, resp_size, &f) == 0) {
                fprintf(stderr, "p9_connect: convM2S failed\n");
                free(client);
                close(fd);
                return NULL;
            }
        }

        if (f.type != Rversion) {
            fprintf(stderr, "p9_connect: unexpected response type 0x%02x\n", f.type);
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
    (void)auth_method;  /* Unused for now */

    if (client == NULL) {
        return -1;
    }

    strncpy(client->username, user ? user : "none", sizeof(client->username) - 1);
    client->username[sizeof(client->username) - 1] = '\0';

    /* Attach as root user */
    {
        uint8_t msg[256];
        uint8_t resp[P9_MAX_MSG];
        Fcall f;
        uint msg_size;
        uint32_t resp_size;
        ssize_t n;

        /* Build Tattach using lib9 */
        memset(&f, 0, sizeof(f));
        f.type = Tattach;
        f.tag = client->tag++;
        f.fid = client->fid;
        f.afid = NOFID;
        f.uname = client->username;
        f.aname = "";

        client->root_fid = client->fid;  /* Save root fid */
        client->fid = (client->fid + 1) % MAX_FIDS;  /* Wrap around */

        msg_size = convS2M(&f, msg, sizeof(msg));
        if (msg_size == 0) {
            fprintf(stderr, "p9_authenticate: convS2M failed\n");
            return -1;
        }

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

        /* Parse response size */
        resp_size = GBIT32(resp);

        /* Read response body */
        n = read_n(client->fd, resp + 4, resp_size - 4);
        if (n < resp_size - 4) {
            fprintf(stderr, "p9_authenticate: read body failed\n");
            return -1;
        }

        /* Parse response using lib9 */
        memset(&f, 0, sizeof(f));
        if (convM2S(resp, resp_size, &f) == 0) {
            fprintf(stderr, "p9_authenticate: convM2S failed\n");
            return -1;
        }

        if (f.type == Rerror) {
            fprintf(stderr, "p9_authenticate: attach failed: %s\n", f.ename ? f.ename : "unknown");
            return -1;
        }

        if (f.type != Rattach) {
            fprintf(stderr, "p9_authenticate: unexpected response 0x%02x\n", f.type);
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
    Fcall f;
    uint msg_size;
    uint32_t resp_size;
    ssize_t n;
    char path_copy[256];
    char *token;
    int fid;
    int root_fid;
    char *wnames[16];
    int nwname = 0;
    const char *path_for_log;  /* Original path for error messages */

    if (client == NULL || path == NULL) {
        return -1;
    }

    /* Store original path for logging before any modifications */
    path_for_log = path;

    /* Resolve path through namespace bind mounts */
    path = namespace_resolve_path(path);

    /* Copy path and tokenize - work with path_copy, not path pointer */
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    if (path_copy[0] == '/') {
        token = strtok(path_copy + 1, "/");
    } else {
        token = strtok(path_copy, "/");
    }

    while (token != NULL && nwname < 16) {
        wnames[nwname++] = token;
        token = strtok(NULL, "/");
    }

    if (nwname == 0) {
        fprintf(stderr, "p9_open: invalid path '%s'\n", path_for_log);
        return -1;
    }

    /* Find next available FID slot with wraparound */
    {
        int attempts;
        for (attempts = 0; attempts < MAX_FIDS; attempts++) {
            fid = client->fid;
            client->fid = (client->fid + 1) % MAX_FIDS;

            if (!client->fids[fid].in_use) {
                break;  /* Found free slot */
            }
        }

        if (attempts >= MAX_FIDS) {
            fprintf(stderr, "p9_open: no free FID slots\n");
            return -1;
        }
    }

    root_fid = client->root_fid;  /* Root fid from Tattach */

    if (root_fid < 0) {
        fprintf(stderr, "p9_open: ERROR - root_fid not set! Did authentication fail?\n");
        return -1;
    }

    /* Build Twalk using lib9 */
    memset(&f, 0, sizeof(f));
    f.type = Twalk;
    f.tag = client->tag++;
    f.fid = root_fid;
    f.newfid = fid;
    f.nwname = nwname;
    /* Copy wname pointers */
    {
        int i;
        for (i = 0; i < nwname; i++) {
            f.wname[i] = wnames[i];
        }
    }

    msg_size = convS2M(&f, msg, sizeof(msg));
    if (msg_size == 0) {
        fprintf(stderr, "p9_open: convS2M failed for Twalk\n");
        return -1;
    }

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

    /* Parse response size */
    resp_size = GBIT32(resp);

    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_open: Twalk read body failed\n");
        return -1;
    }

    /* Parse response using lib9 */
    memset(&f, 0, sizeof(f));
    if (convM2S(resp, resp_size, &f) == 0) {
        fprintf(stderr, "p9_open: convM2S failed for Twalk\n");
        return -1;
    }

    if (f.type == Rerror) {
        fprintf(stderr, "p9_open: Twalk failed for '%s': %s\n", path_for_log, f.ename ? f.ename : "unknown");
        return -1;
    }

    if (f.type != Rwalk) {
        fprintf(stderr, "p9_open: unexpected Twalk response type 0x%02x\n", f.type);
        return -1;
    }

    /* Now Topen the fid */
    memset(&f, 0, sizeof(f));
    f.type = Topen;
    f.tag = client->tag++;
    f.fid = fid;
    f.mode = mode;

    msg_size = convS2M(&f, msg, sizeof(msg));
    if (msg_size == 0) {
        fprintf(stderr, "p9_open: convS2M failed for Topen\n");
        return -1;
    }

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

    /* Parse response size */
    resp_size = GBIT32(resp);

    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_open: Topen read body failed\n");
        return -1;
    }

    /* Parse response using lib9 */
    memset(&f, 0, sizeof(f));
    if (convM2S(resp, resp_size, &f) == 0) {
        fprintf(stderr, "p9_open: convM2S failed for Topen\n");
        return -1;
    }

    if (f.type == Rerror) {
        fprintf(stderr, "p9_open: Topen failed for '%s': %s\n", path_for_log, f.ename ? f.ename : "unknown");
        return -1;
    }

    if (f.type != Ropen) {
        fprintf(stderr, "p9_open: unexpected Topen response type 0x%02x\n", f.type);
        return -1;
    }

    /* Mark FID as in use and initialize state */
    if (fid >= 0 && fid < MAX_FIDS) {
        client->fids[fid].fid = fid;
        client->fids[fid].is_stream = 0;  /* Default to non-streaming */
        client->fids[fid].offset = 0;
        client->fids[fid].in_use = 1;
    }

    return fid;
}

/*
 * Close a file on 9P server
 * Sends Tclunk message to properly release FID on server
 */
int p9_close(P9Client *client, int fd)
{
    uint8_t msg[256];
    uint8_t resp[P9_MAX_MSG];
    Fcall f;
    uint msg_size;
    uint32_t resp_size;
    ssize_t n;

    if (client == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= MAX_FIDS) {
        fprintf(stderr, "p9_close: invalid fd %d\n", fd);
        return -1;
    }

    /* Check if FID is in use */
    if (!client->fids[fd].in_use) {
        fprintf(stderr, "p9_close: fd %d not in use\n", fd);
        return -1;
    }

    /* Build Tclunk using lib9 */
    memset(&f, 0, sizeof(f));
    f.type = Tclunk;
    f.tag = client->tag++;
    f.fid = fd;

    msg_size = convS2M(&f, msg, sizeof(msg));
    if (msg_size == 0) {
        fprintf(stderr, "p9_close: convS2M failed\n");
        return -1;
    }

    /* Send Tclunk */
    if (write_n(client->fd, msg, msg_size) < 0) {
        fprintf(stderr, "p9_close: Tclunk write failed\n");
        return -1;
    }

    /* Receive response */
    n = read_n(client->fd, resp, 4);
    if (n < 4) {
        fprintf(stderr, "p9_close: Tclunk read header failed\n");
        return -1;
    }

    /* Parse response size */
    resp_size = GBIT32(resp);

    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_close: Tclunk read body failed (got %d, expected %u)\n",
                (int)n, resp_size - 4);
        return -1;
    }

    /* Parse response using lib9 */
    memset(&f, 0, sizeof(f));
    if (convM2S(resp, resp_size, &f) == 0) {
        fprintf(stderr, "p9_close: convM2S failed\n");
        return -1;
    }

    if (f.type == Rerror) {
        fprintf(stderr, "p9_close: server returned error for fd %d: %s\n",
                fd, f.ename ? f.ename : "unknown");
        return -1;
    }

    if (f.type != Rclunk) {
        fprintf(stderr, "p9_close: unexpected response type 0x%02x for fd %d\n",
                f.type, fd);
        return -1;
    }

    /* Clear local FID state */
    client->fids[fd].in_use = 0;
    client->fids[fd].is_stream = 0;
    client->fids[fd].offset = 0;
    client->fids[fd].fid = 0;

    return 0;
}

/*
 * Read from a file on 9P server
 */
ssize_t p9_read(P9Client *client, int fd, void *buf, size_t count)
{
    uint8_t msg[256];
    uint8_t resp[P9_MAX_MSG];
    Fcall f;
    uint msg_size;
    uint32_t resp_size;
    ssize_t n;
    uint64_t offset;
    P9FID *fid;

    if (client == NULL || buf == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= MAX_FIDS) {
        fprintf(stderr, "p9_read: invalid fd %d\n", fd);
        return -1;
    }

    fid = &client->fids[fd];
    if (!fid->in_use) {
        fprintf(stderr, "p9_read: fd %d not in use\n", fd);
        return -1;
    }

    /* For streaming FIDs, reset offset before read */
    if (fid->is_stream) {
        /* fprintf(stderr, "[P9CLIENT] Resetting offset for streaming FD %d\n", fd); */
        fid->offset = 0;
    }

    /* Get current offset for this FID */
    offset = fid->offset;

    /* Build Tread using lib9 */
    memset(&f, 0, sizeof(f));
    f.type = Tread;
    f.tag = client->tag++;
    f.fid = fd;
    f.offset = offset;
    f.count = count;

    msg_size = convS2M(&f, msg, sizeof(msg));
    if (msg_size == 0) {
        fprintf(stderr, "p9_read: convS2M failed\n");
        return -1;
    }

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

    /* Parse response size */
    resp_size = GBIT32(resp);

    /* Read response body */
    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_read: read body failed (got %d, expected %u)\n",
                (int)n, resp_size - 4);
        return -1;
    }

    /* Parse response using lib9 */
    memset(&f, 0, sizeof(f));
    if (convM2S(resp, resp_size, &f) == 0) {
        fprintf(stderr, "p9_read: convM2S failed\n");
        return -1;
    }

    if (f.type == Rerror) {
        fprintf(stderr, "p9_read: server returned error: %s\n", f.ename ? f.ename : "unknown");
        return -1;
    }

    if (f.type != Rread) {
        fprintf(stderr, "p9_read: unexpected response type 0x%02x\n", f.type);
        return -1;
    }

    /* Copy data to buffer */
    if (f.count > count) {
        f.count = count;
    }
    memcpy(buf, f.data, f.count);

    /* Update offset for next read (will be 0 for streaming) */
    fid->offset += f.count;

    return (ssize_t)f.count;
}

/*
 * Write to a file on Marrow
 */
ssize_t p9_write(P9Client *client, int fd, const void *buf, size_t count)
{
    uint8_t msg[P9_MAX_MSG];
    uint8_t resp[P9_MAX_MSG];
    Fcall f;
    uint msg_size;
    uint32_t resp_size;
    ssize_t n;
    uint64_t offset;
    P9FID *fid;

    if (client == NULL || buf == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= MAX_FIDS) {
        fprintf(stderr, "p9_write: invalid fd %d\n", fd);
        return -1;
    }

    fid = &client->fids[fd];
    if (!fid->in_use) {
        fprintf(stderr, "p9_write: fd %d not in use\n", fd);
        return -1;
    }

    /* Get current offset for this FID */
    offset = fid->offset;

    /* Build Twrite using lib9 */
    memset(&f, 0, sizeof(f));
    f.type = Twrite;
    f.tag = client->tag++;
    f.fid = fd;
    f.offset = offset;
    f.count = count;
    f.data = (char *)buf;  /* Cast away const for lib9 */

    msg_size = convS2M(&f, msg, sizeof(msg));
    if (msg_size == 0) {
        fprintf(stderr, "p9_write: convS2M failed\n");
        return -1;
    }

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

    /* Parse response size */
    resp_size = GBIT32(resp);

    n = read_n(client->fd, resp + 4, resp_size - 4);
    if (n < resp_size - 4) {
        fprintf(stderr, "p9_write: Twrite read body failed\n");
        return -1;
    }

    /* Parse response using lib9 */
    memset(&f, 0, sizeof(f));
    if (convM2S(resp, resp_size, &f) == 0) {
        fprintf(stderr, "p9_write: convM2S failed\n");
        return -1;
    }

    if (f.type == Rerror) {
        fprintf(stderr, "p9_write: server returned error: %s\n", f.ename ? f.ename : "unknown");
        return -1;
    }

    if (f.type != Rwrite) {
        fprintf(stderr, "p9_write: unexpected response type 0x%02x\n", f.type);
        return -1;
    }

    /* Update offset for next write */
    fid->offset += count;

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
    P9FID *fid;

    if (client == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= MAX_FIDS) {
        fprintf(stderr, "p9_reset_fid: invalid fd %d\n", fd);
        return -1;
    }

    fid = &client->fids[fd];
    if (!fid->in_use) {
        fprintf(stderr, "p9_reset_fid: fd %d not in use\n", fd);
        return -1;
    }

    /* Reset offset to 0 for this FID */
    fid->offset = 0;

    return 0;
}

/*
 * Clunk (close) a file descriptor
 */
int p9_clunk(P9Client *client, int fd)
{
    return p9_close(client, fd);
}

/*
 * Reconnect to 9P server
 * Closes existing connection, reconnects, and re-authenticates
 * Returns 0 on success, -1 on failure
 */
int p9_reconnect(P9Client **client_ptr, const char *address)
{
    P9Client *old_client;
    P9Client *new_client;
    int old_fd;

    if (client_ptr == NULL || *client_ptr == NULL || address == NULL) {
        return -1;
    }

    old_client = *client_ptr;

    /* Create new connection */
    new_client = p9_connect(address);
    if (new_client == NULL) {
        return -1;
    }

    /* Authenticate with new connection */
    if (p9_authenticate(new_client, P9_AUTH_NONE, "none", "") < 0) {
        p9_disconnect(new_client);
        return -1;
    }

    /* Save old fd to close later */
    old_fd = old_client->fd;

    /* Replace old client with new one */
    *client_ptr = new_client;

    /* Clean up old client (don't use p9_disconnect as it closes socket we want to keep) */
    free(old_client);

    /* Close old socket after new connection is established */
    close(old_fd);

    fprintf(stderr, "p9_reconnect: successfully reconnected to %s\n", address);

    return 0;
}

/*
 * Open a streaming device (automatically marks as stream)
 * Streaming devices always read from offset 0
 */
int p9_open_stream(P9Client *client, const char *path)
{
    int fd;

    if (client == NULL || path == NULL) {
        return -1;
    }

    fd = p9_open(client, path, 0);
    if (fd < 0) {
        return fd;
    }

    /* Mark this FID as a streaming device */
    if (fd >= 0 && fd < MAX_FIDS) {
        client->fids[fd].is_stream = 1;
        client->fids[fd].offset = 0;
        /* fprintf(stderr, "[P9CLIENT] Opened %s as streaming device (fd=%d)\n", path, fd); */
    }

    return fd;
}

/*
 * Mark an existing FD as a streaming device
 */
void p9_mark_stream(P9Client *client, int fd)
{
    if (client == NULL || fd < 0 || fd >= MAX_FIDS) {
        return;
    }

    client->fids[fd].is_stream = 1;
    client->fids[fd].offset = 0;
    /* fprintf(stderr, "[P9CLIENT] Marked fd=%d as streaming device\n", fd); */
}

/*
 * Read from a streaming device (offset always 0)
 * This is an explicit streaming read function for clarity
 */
ssize_t p9_read_stream(P9Client *client, int fd, void *buf, size_t count)
{
    P9FID *fid;

    if (client == NULL || buf == NULL) {
        return -1;
    }

    if (fd < 0 || fd >= MAX_FIDS) {
        fprintf(stderr, "p9_read_stream: invalid fd %d\n", fd);
        return -1;
    }

    fid = &client->fids[fd];
    if (!fid->in_use) {
        fprintf(stderr, "p9_read_stream: fd %d not in use\n", fd);
        return -1;
    }

    /* Force offset to 0 for streaming read */
    fid->offset = 0;
    fid->is_stream = 1;  /* Ensure it's marked as streaming */

    /* Use regular read - it will now use offset 0 */
    return p9_read(client, fd, buf, count);
}

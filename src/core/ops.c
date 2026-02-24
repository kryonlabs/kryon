/*
 * 9P Operation Handlers
 */

#include "kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * CPU server integration
 */
#ifdef INCLUDE_CPU_SERVER
extern int cpu_server_init(P9Node *root);
extern int cpu_handle_new_client(int client_fd, const char *user, const char *aname);
extern const char *cpu_find_plan9_path(void);
#endif

/*
 * Current client fd (for CPU server tracking)
 * This is set before handling each message
 */
static int current_client_fd = -1;

/*
 * Set current client fd (called from server loop)
 */
void p9_set_client_fd(int fd)
{
    current_client_fd = fd;
}

/*
 * Get current client fd
 */
int p9_get_client_fd(void)
{
    return current_client_fd;
}

/*
 * Convert 9P message type to string for logging
 */
const char *p9_msg_type_to_str(uint8_t type)
{
    switch(type) {
        case Tversion: return "Tversion";
        case Rversion: return "Rversion";
        case Tauth: return "Tauth";
        case Rauth: return "Rauth";
        case Tattach: return "Tattach";
        case Rattach: return "Rattach";  /* Note: Rerror has same value (105) */
        case Tflush: return "Tflush";
        case Rflush: return "Rflush";
        case Twalk: return "Twalk";
        case Rwalk: return "Rwalk";
        case Topen: return "Topen";
        case Ropen: return "Ropen";
        case Tcreate: return "Tcreate";
        case Rcreate: return "Rcreate";
        case Tread: return "Tread";
        case Rread: return "Rread";
        case Twrite: return "Twrite";
        case Rwrite: return "Rwrite";
        case Tclunk: return "Tclunk";
        case Rclunk: return "Rclunk";
        case Tremove: return "Tremove";
        case Rremove: return "Rremove";
        case Tstat: return "Tstat";
        case Rstat: return "Rstat";
        case Twstat: return "Twstat";
        case Rwstat: return "Rwstat";
        default: return "Unknown";
    }
}

/*
 * Build full path for a node (for logging)
 */
static void build_node_path(P9Node *node, char *buf, size_t bufsize)
{
    char path_stack[256][32];
    int depth = 0;
    P9Node *current;

    if (node == NULL || buf == NULL || bufsize == 0) {
        return;
    }

    /* Build path from root to node */
    current = node;
    while (current != NULL && depth < 256) {
        if (current->name != NULL) {
            strncpy(path_stack[depth], current->name, 31);
            path_stack[depth][31] = '\0';
            depth++;
        }
        current = current->parent;
    }

    /* Build path string */
    buf[0] = '\0';
    while (depth > 0) {
        depth--;
        if (buf[0] != '\0') {
            strncat(buf, "/", bufsize - strlen(buf) - 1);
        }
        strncat(buf, path_stack[depth], bufsize - strlen(buf) - 1);
    }
}

/*
 * FID table
 */
static P9Fid fid_table[P9_MAX_FID];
static int fid_table_initialized = 0;

/*
 * Negotiated message size
 */
static uint32_t negotiated_msize = P9_MAX_MSG;

/*
 * Initialize FID table
 */
int fid_init(void)
{
    int i;

    if (fid_table_initialized) {
        return 0;
    }

    for (i = 0; i < P9_MAX_FID; i++) {
        fid_table[i].fid = 0;
        fid_table[i].node = NULL;
        fid_table[i].is_open = 0;
        fid_table[i].mode = 0;
    }

    fid_table_initialized = 1;
    return 0;
}

/*
 * Allocate a new FID
 */
P9Fid *fid_new(uint32_t fid_num, P9Node *node)
{
    if (fid_num >= P9_MAX_FID) {
        return NULL;
    }

    if (fid_table[fid_num].node != NULL) {
        /* FID already in use */
        return NULL;
    }

    fid_table[fid_num].fid = fid_num;
    fid_table[fid_num].node = node;
    fid_table[fid_num].is_open = 0;
    fid_table[fid_num].mode = 0;

    return &fid_table[fid_num];
}

/*
 * Get an existing FID
 */
P9Fid *fid_get(uint32_t fid_num)
{
    if (fid_num >= P9_MAX_FID) {
        return NULL;
    }

    if (fid_table[fid_num].node == NULL) {
        return NULL;
    }

    return &fid_table[fid_num];
}

/*
 * Release a FID
 */
int fid_put(uint32_t fid_num)
{
    P9Fid *fid;

    fid = fid_get(fid_num);
    if (fid == NULL) {
        return -1;
    }

    fid->node = NULL;
    fid->is_open = 0;
    fid->mode = 0;

    return 0;
}

/*
 * Clunk a FID (close if open, then release)
 */
int fid_clunk(uint32_t fid_num)
{
    P9Fid *fid;

    fid = fid_get(fid_num);
    if (fid == NULL) {
        return -1;
    }

    fid->is_open = 0;
    fid->mode = 0;
    fid->node = NULL;

    return 0;
}

/*
 * Cleanup FID table
 */
void fid_cleanup(void)
{
    fid_table_initialized = 0;
}

/*
 * Handle Tversion
 */
size_t handle_tversion(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t msize;
    char version[P9_MAX_VERSION];
    P9Hdr hdr;
    uint32_t final_msize;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tversion(in_buf, in_len, &msize, version) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tversion");
    }

    /* We only support 9P2000 */
    if (strcmp(version, "9P2000") != 0) {
        return p9_build_rerror(out_buf, hdr.tag, "unsupported version");
    }

    /* Negotiate message size */
    final_msize = msize;
    if (final_msize > P9_MAX_MSG) {
        final_msize = P9_MAX_MSG;
    }
    if (final_msize < 256) {
        final_msize = 256;
    }

    negotiated_msize = final_msize;

    return p9_build_rversion(out_buf, hdr.tag, final_msize, "9P2000");
}

/*
 * Handle Tattach
 */
size_t handle_tattach(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid, afid;
    char uname[P9_MAX_STR];
    char aname[P9_MAX_STR];
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *root;
    int is_cpu_attach;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tattach(in_buf, in_len, &fid, &afid, uname, aname) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tattach");
    }

    fprintf(stderr, "handle_tattach: user='%s' aname='%s' client_fd=%d\n",
            uname, aname, current_client_fd);

    /* Check if this is a CPU server attach */
    is_cpu_attach = (strcmp(aname, "cpu") == 0);

#ifdef INCLUDE_CPU_SERVER
    if (is_cpu_attach && current_client_fd >= 0) {
        /* Initialize CPU server session */
        int session_id = cpu_handle_new_client(current_client_fd, uname, aname);
        if (session_id < 0) {
            fprintf(stderr, "handle_tattach: failed to create CPU session\n");
            return p9_build_rerror(out_buf, hdr.tag, "CPU session failed");
        }
        fprintf(stderr, "handle_tattach: created CPU session %d\n", session_id);
    }
#endif

    /* Get root node */
    root = tree_root();
    if (root == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "no root");
    }

    /* Create FID */
    fid_obj = fid_new(fid, root);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid in use");
    }

    return p9_build_rattach(out_buf, hdr.tag, &root->qid);
}

/*
 * Handle Twalk
 */
size_t handle_twalk(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid, newfid;
    char *wnames[P9_MAX_WELEM];
    int nwname;
    P9Hdr hdr;
    P9Fid *fid_obj, *newfid_obj;
    P9Node *node, *newnode;
    P9Qid wqid[P9_MAX_WELEM];
    int i;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_twalk(in_buf, in_len, &fid, &newfid, wnames, &nwname) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Twalk");
    }

    /* Get source FID */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        /* Free allocated wnames */
        for (i = 0; i < nwname; i++) {
            free(wnames[i]);
        }
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Walk through all path components */
    newnode = node;
    fprintf(stderr, "handle_twalk: walking from '%s' through %d components\n",
            node->name ? node->name : "(null)", nwname);
    for (i = 0; i < nwname; i++) {
        fprintf(stderr, "  component[%d] = '%s'\n", i, wnames[i]);
        newnode = tree_walk(newnode, wnames[i]);
        if (newnode == NULL) {
            /* Free allocated wnames */
            int j;
            fprintf(stderr, "  -> not found!\n");
            for (j = i; j < nwname; j++) {
                free(wnames[j]);
            }
            return p9_build_rerror(out_buf, hdr.tag, "file not found");
        }
        fprintf(stderr, "  -> '%s' (qid.type=%02X)\n", newnode->name, newnode->qid.type);
        /* Store QID for this component */
        wqid[i] = newnode->qid;
    }

    /* Create new FID pointing to final node */
    newfid_obj = fid_new(newfid, newnode);
    if (newfid_obj == NULL) {
        /* Free allocated wnames */
        for (i = 0; i < nwname; i++) {
            free(wnames[i]);
        }
        return p9_build_rerror(out_buf, hdr.tag, "newfid in use");
    }

    /* Free allocated wnames */
    for (i = 0; i < nwname; i++) {
        free(wnames[i]);
    }

    /* Return all QIDs from the walk */
    return p9_build_rwalk(out_buf, hdr.tag, wqid, nwname);
}

/*
 * Handle Topen
 */
size_t handle_topen(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid;
    uint8_t mode;
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *node;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_topen(in_buf, in_len, &fid, &mode) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Topen");
    }

    /* Get FID */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Log full path */
    {
        char path_buf[256];
        build_node_path(node, path_buf, sizeof(path_buf));
        fprintf(stderr, "handle_topen: opening '%s' (fid=%u mode=%d)\n",
                path_buf, fid, mode);
    }

    /* Check permissions */
    if (mode == P9_OREAD || mode == P9_ORDWR) {
        /* Read OK */
    }
    if (mode == P9_OWRITE || mode == P9_ORDWR) {
        /* Write OK */
    }

    /* Mark as open */
    fid_obj->is_open = 1;
    fid_obj->mode = mode;

    fprintf(stderr, "handle_topen: opened '%s' successfully\n", node->name);

    return p9_build_ropen(out_buf, hdr.tag, &node->qid, 0);
}

/*
 * Forward declarations for tree functions
 */
extern ssize_t node_read(P9Node *node, char *buf, size_t count, uint64_t offset);
extern ssize_t node_write(P9Node *node, const char *buf, size_t count, uint64_t offset);

/*
 * Handle Tread for directories
 * Returns packed stat entries for directory contents
 */
static size_t handle_directory_read(P9Fid *fid_obj, uint64_t offset, uint32_t count,
                                     uint8_t *out_buf, uint16_t tag)
{
    P9Node *dir_node = fid_obj->node;
    uint8_t *data_start = out_buf + 4;  /* Skip count field */
    uint8_t *p = data_start;
    int i;
    P9Stat stat;
    size_t stat_size;

    /* Check if node has children */
    if (dir_node->children == NULL || dir_node->nchildren == 0) {
        /* Empty directory */
        le_put32(out_buf, 0);
        return p9_build_rread(out_buf, tag, (char *)data_start, 0);
    }

    /* Build stat for each child */
    for (i = 0; i < dir_node->nchildren; i++) {
        P9Node *child;

        /* Safety check */
        if (dir_node->children == NULL || i >= dir_node->nchildren) {
            break;
        }

        child = dir_node->children[i];
        if (child == NULL) {
            continue;
        }

        /* Build stat structure */
        memset(&stat, 0, sizeof(stat));
        stat.type = 0;
        stat.dev = 0;
        stat.qid = child->qid;
        stat.mode = child->mode;
        stat.atime = child->atime;
        stat.mtime = child->mtime;
        stat.length = child->length;

        /* Copy name safely */
        if (child->name != NULL) {
            strncpy(stat.name, child->name, P9_MAX_STR - 1);
            stat.name[P9_MAX_STR - 1] = '\0';
        }

        strcpy(stat.uid, "none");
        strcpy(stat.gid, "none");
        strcpy(stat.muid, "none");

        /* Pack stat and check if we have space */
        stat_size = p9_pack_stat(p, &stat);

        /* Check if this entry would exceed count */
        if ((p - data_start) + stat_size > count) {
            break;  /* Don't add partial entries */
        }

        p += stat_size;
    }

    /* Write total count */
    le_put32(out_buf, p - data_start);

    /* Build message */
    return p9_build_rread(out_buf, tag, (char *)data_start, p - data_start);
}

/*
 * Handle Tread
 */
size_t handle_tread(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid, count;
    uint64_t offset;
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *node;
    ssize_t nread;
    char data[P9_MAX_MSG];

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tread(in_buf, in_len, &fid, &offset, &count) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tread");
    }

    /* Get FID */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    fprintf(stderr, "handle_tread: fid=%u node='%s' offset=%lu count=%u\n",
            fid, node->name, (unsigned long)offset, count);

    /* Check if directory */
    if (node->qid.type & QTDIR) {
        return handle_directory_read(fid_obj, offset, count, out_buf, hdr.tag);
    }

    /* Limit count */
    if (count > negotiated_msize - 24) {
        count = negotiated_msize - 24;
    }

    /* Read from file */
    nread = node_read(node, data, count, offset);
    if (nread < 0) {
        fprintf(stderr, "handle_tread: read error for node '%s'\n", node->name);
        return p9_build_rerror(out_buf, hdr.tag, "read error");
    }

    fprintf(stderr, "handle_tread: returning %ld bytes for node '%s'\n", (long)nread, node->name);
    
    return p9_build_rread(out_buf, hdr.tag, data, (uint32_t)nread);
}

/*
 * Handle Twrite
 */
size_t handle_twrite(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid, count;
    uint64_t offset;
    const char *data;
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *node;
    ssize_t nwritten;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_twrite(in_buf, in_len, &fid, &offset, &data, &count) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Twrite");
    }

    /* Get FID */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Check if directory */
    if (node->qid.type & QTDIR) {
        return p9_build_rerror(out_buf, hdr.tag, "is directory");
    }

    /* Write to file */
    nwritten = node_write(node, data, count, offset);
    if (nwritten < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "write error");
    }

    return p9_build_rwrite(out_buf, hdr.tag, (uint32_t)nwritten);
}

/*
 * Handle Tclunk
 */
size_t handle_tclunk(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid;
    P9Hdr hdr;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tclunk(in_buf, in_len, &fid) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tclunk");
    }

    /* Clunk FID */
    if (fid_clunk(fid) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    return p9_build_rclunk(out_buf, hdr.tag);
}

/*
 * Handle Tstat
 */
size_t handle_tstat(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid;
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *node;
    P9Stat stat;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tstat(in_buf, in_len, &fid) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tstat");
    }

    /* Get FID */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Build stat structure */
    memset(&stat, 0, sizeof(stat));
    stat.type = 0;
    stat.dev = 0;
    stat.qid = node->qid;
    stat.mode = node->mode;
    stat.atime = node->atime;
    stat.mtime = node->mtime;
    stat.length = node->length;
    strncpy(stat.name, node->name, P9_MAX_STR - 1);
    strcpy(stat.uid, "none");
    strcpy(stat.gid, "none");
    strcpy(stat.muid, "none");

    return p9_build_rstat(out_buf, hdr.tag, &stat);
}

/*
 * Main 9P message dispatcher
 */
size_t dispatch_9p(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    P9Hdr hdr;
    size_t result;

    /* Parse header */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        fprintf(stderr, "dispatch_9p: failed to parse header\n");
        return 0;
    }

    fprintf(stderr, "dispatch_9p: received message type=%d (%s)\n",
            hdr.type, p9_msg_type_to_str(hdr.type));

    /* Dispatch based on message type */
    switch (hdr.type) {
        case Tversion:
            result = handle_tversion(in_buf, in_len, out_buf);
            break;

        case Tattach:
            result = handle_tattach(in_buf, in_len, out_buf);
            break;

        case Twalk:
            result = handle_twalk(in_buf, in_len, out_buf);
            break;

        case Topen:
            result = handle_topen(in_buf, in_len, out_buf);
            break;

        case Tread:
            result = handle_tread(in_buf, in_len, out_buf);
            break;

        case Twrite:
            result = handle_twrite(in_buf, in_len, out_buf);
            break;

        case Tclunk:
            result = handle_tclunk(in_buf, in_len, out_buf);
            break;

        case Tstat:
            result = handle_tstat(in_buf, in_len, out_buf);
            break;

        default:
            /* Unknown message type */
            result = p9_build_rerror(out_buf, hdr.tag, "not supported");
            break;
    }

    return result;
}

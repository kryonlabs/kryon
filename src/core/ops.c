/*
 * 9P Operation Handlers
 */

#include "kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

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
 * FID table
 */
static P9Fid fid_table[P9_MAX_FID];
static int fid_table_initialized = 0;
static uint32_t negotiated_msize = P9_MAX_MSG;

/*
 * Initialize FID table
 */
int fid_init(void)
{
    int i;
    if (fid_table_initialized) return 0;

    for (i = 0; i < P9_MAX_FID; i++) {
        fid_table[i].fid = 0;
        fid_table[i].node = NULL;
        fid_table[i].client_fd = -1; /* -1 indicates slot is empty */
        fid_table[i].is_open = 0;
        fid_table[i].mode = 0;
    }

    fid_table_initialized = 1;
    return 0;
}

/**
 * Cleanup FIDs for a specific client
 */
void fid_cleanup_conn(int client_fd)
{
    int i;
    int cleared = 0;
    for (i = 0; i < P9_MAX_FID; i++) {
        if (fid_table[i].node != NULL && fid_table[i].client_fd == client_fd) {
            fid_table[i].node = NULL;
            fid_table[i].client_fd = -1;
            fid_table[i].is_open = 0;
            cleared++;
        }
    }
    if (cleared > 0) {
        fprintf(stderr, "fid_cleanup: released %d FIDs for fd %d\n", cleared, client_fd);
    }
}

/*
 * Allocate a new FID
 */
P9Fid *fid_new(uint32_t fid_num, P9Node *node)
{
    if (fid_num >= P9_MAX_FID) return NULL;

    if (fid_table[fid_num].node != NULL && fid_table[fid_num].client_fd == current_client_fd) {
        return NULL;
    }

    fid_table[fid_num].fid = fid_num;
    fid_table[fid_num].node = node;
    fid_table[fid_num].client_fd = current_client_fd;
    fid_table[fid_num].is_open = 0;
    fid_table[fid_num].mode = 0;

    return &fid_table[fid_num];
}

/*
 * Get an existing FID
 */
P9Fid *fid_get(uint32_t fid_num)
{
    if (fid_num >= P9_MAX_FID) return NULL;

    if (fid_table[fid_num].node == NULL || fid_table[fid_num].client_fd != current_client_fd) {
        return NULL;
    }

    return &fid_table[fid_num];
}

/*
 * Release a FID
 */
int fid_put(uint32_t fid_num)
{
    return fid_clunk(fid_num);
}

/*
 * Clunk a FID (close if open, then release)
 */

int fid_clunk(uint32_t fid_num)
{
    P9Fid *fid = fid_get(fid_num);
    if (fid == NULL) return -1;

    fid->node = NULL;
    fid->client_fd = -1;
    fid->is_open = 0;
    fid->mode = 0;

    return 0;
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

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tattach(in_buf, in_len, &fid, &afid, uname, aname) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tattach");
    }

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
 * Handle Tauth
 * Return Rauth with QID to indicate no authentication required
 * This is what many 9P servers do
 */
size_t handle_tauth(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    P9Hdr hdr;
    char uname[P9_MAX_STR];
    char aname[P9_MAX_STR];
    P9Qid qid;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tauth(in_buf, in_len, uname, aname) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tauth");
    }

    /* Return Rauth with QID indicating authentication file */
    /* Use QID path 1 for the auth fid */
    qid.type = QTAUTH;
    qid.version = 0;
    qid.path = 1;

    return p9_build_rauth(out_buf, hdr.tag, &qid);
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
        /* No need to free wnames - they point into input buffer */
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Walk through all path components */
    newnode = node;
    for (i = 0; i < nwname; i++) {
        /* Get string length from buffer (stored 2 bytes before string) */
        uint16_t name_len = le_get16((uint8_t*)(wnames[i] - 2));
        int found = 0;

        /* "." means current directory */
        if (name_len == 1 && wnames[i][0] == '.') {
            /* newnode stays the same */
            found = 1;
        }
        /* ".." means parent directory */
        else if (name_len == 2 && wnames[i][0] == '.' && wnames[i][1] == '.') {
            if (newnode->parent != NULL) {
                newnode = newnode->parent;
                found = 1;
            }
        }
        /* Check for dynamic /dev/draw/[n] directories */
        else if (newnode->name != NULL && strcmp(newnode->name, "draw") == 0) {
            /* Check if name is all digits */
            int is_numeric = 1;
            int j;
            for (j = 0; j < name_len && j < 16; j++) {
                if (wnames[i][j] < '0' || wnames[i][j] > '9') {
                    is_numeric = 0;
                    break;
                }
            }
            if (is_numeric && name_len > 0 && name_len < 16) {
                char temp_name[16];
                memcpy(temp_name, wnames[i], name_len);
                temp_name[name_len] = '\0';
                int conn_id = atoi(temp_name);
                P9Node *dynamic_node = drawconn_create_dir(conn_id);
                if (dynamic_node != NULL) {
                    /* Add to children if not already present */
                    int already_exists = 0;
                    int k;
                    for (k = 0; k < newnode->nchildren; k++) {
                        if (newnode->children[k] != NULL &&
                            strlen(newnode->children[k]->name) == name_len &&
                            memcmp(newnode->children[k]->name, wnames[i], name_len) == 0) {
                            already_exists = 1;
                            newnode = newnode->children[k];
                            break;
                        }
                    }
                    if (!already_exists) {
                        tree_add_child(newnode, dynamic_node);
                        newnode = dynamic_node;
                    }
                    found = 1;
                }
            }
        }

        /* Search children using length-based comparison */
        if (!found && newnode->children != NULL) {
            int j;
            for (j = 0; j < newnode->nchildren; j++) {
                if (newnode->children[j] != NULL) {
                    size_t child_len = strlen(newnode->children[j]->name);
                    if (child_len == name_len &&
                        memcmp(newnode->children[j]->name, wnames[i], name_len) == 0) {
                        newnode = newnode->children[j];
                        found = 1;
                        break;
                    }
                }
            }
        }

        if (!found) {
            /* No need to free wnames - they point into input buffer */
            return p9_build_rerror(out_buf, hdr.tag, "file not found");
        }
        /* Store QID for this component */
        wqid[i] = newnode->qid;
    }

    /* Create new FID pointing to final node */
    newfid_obj = fid_new(newfid, newnode);
    if (newfid_obj == NULL) {
        /* No need to free wnames - they point into input buffer */
        return p9_build_rerror(out_buf, hdr.tag, "newfid in use");
    }

    /* No need to free wnames - they point into input buffer */

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

    /* Use iounit=0 to indicate no preferred I/O size */
    /* Let the client decide the optimal chunk size */
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
    uint8_t stat_buf[P9_MAX_MSG];  /* Temporary buffer for stat data */
    uint8_t *p = stat_buf;
    int i;
    P9Stat stat;
    size_t stat_size;

    /* Check if node has children */
    if (dir_node->children == NULL || dir_node->nchildren == 0) {
        /* Empty directory */
        return p9_build_rread(out_buf, tag, NULL, 0);
    }

    /* Build stat for each child, tracking byte offset for pagination */
    uint64_t bytes_serialized = 0;

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

        /* Pack stat to get its size */
        stat_size = p9_pack_stat(p, &stat);

        /* Skip entries until we reach the client's offset */
        if (bytes_serialized < offset) {
            bytes_serialized += stat_size;
            continue;
        }

        /* Check if adding this entry would overflow client's buffer */
        if ((p - stat_buf) + stat_size > count) {
            break;  /* Don't add partial entries */
        }

        p += stat_size;
        bytes_serialized += stat_size;
    }

    /* Build Rread message - p9_build_rread handles header, count, and data */
    return p9_build_rread(out_buf, tag, (char *)stat_buf, p - stat_buf);
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
 * Handle Tremove
 * Removes a node from the tree and clunks the FID
 */
size_t handle_tremove(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    uint32_t fid;
    P9Hdr hdr;
    P9Fid *fid_obj;
    P9Node *node;
    int remove_result;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    /* Parse fid (Tremove: size[4] Tremove tag[2] fid[4]) */
    if (in_len < 7 + 4) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tremove");
    }
    fid = le_get32(in_buf + 7);

    /* Get FID - return error if fid doesn't exist */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;

    /* Attempt removal from tree */
    remove_result = tree_remove_node(node);

    /* ALWAYS clunk the fid per 9P spec, even if removal fails */
    fid_clunk(fid);

    if (remove_result < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "permission denied");
    }

    return p9_build_rremove(out_buf, hdr.tag);
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

    /* Dispatch based on message type */
    switch (hdr.type) {
        case Tversion:
            result = handle_tversion(in_buf, in_len, out_buf);
            break;

        case Tauth:
            result = handle_tauth(in_buf, in_len, out_buf);
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

        case Tremove:
            result = handle_tremove(in_buf, in_len, out_buf);
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

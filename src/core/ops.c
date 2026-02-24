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
 * Hexdump helper for debugging
 */
static void hexdump(const uint8_t *data, size_t len)
{
    size_t i;
    for (i = 0; i < len; i++) {
        fprintf(stderr, "%02X ", data[i]);
        if ((i + 1) % 16 == 0) {
            fprintf(stderr, "\n");
        }
    }
    if (len % 16 != 0) {
        fprintf(stderr, "\n");
    }
}

/*
 * Build full path for a node (for logging)
 * Fixed to avoid buffer overflow by collecting component pointers first
 * Also handles the root node being its own parent
 */
static void build_node_path(P9Node *node, char *buf, size_t bufsize)
{
    const char *components[256]; /* Pointers to node names */
    int depth = 0;
    P9Node *current;
    size_t total_len = 0;
    int i;

    if (node == NULL || buf == NULL || bufsize == 0) {
        return;
    }

    /* Collect path components by walking from node to root */
    /* This collects them in reverse order (deepest first) */
    current = node;
    while (current != NULL && depth < 256) {
        if (current->name != NULL) {
            size_t len = strlen(current->name);
            /* Check if adding this component would overflow */
            if (total_len + len + 1 < bufsize) {
                components[depth] = current->name;
                total_len += len + 1; /* +1 for '/' separator */
                depth++;
            } else {
                break; /* Would overflow */
            }
        }
        /* Check if we've reached the root (root is its own parent) */
        if (current->parent == current) {
            break;
        }
        current = current->parent;
    }

    /* Build path from root to node by iterating backwards */
    /* components[depth-1] is the node itself, components[0] is the root */
    buf[0] = '\0';
    for (i = depth - 1; i >= 0; i--) {
        size_t buf_len = strlen(buf);
        size_t space_left = bufsize - buf_len - 1;

        if (buf[0] != '\0') {
            /* Add separator */
            if (space_left > 0) {
                strncat(buf, "/", space_left);
                buf_len = strlen(buf);
                space_left = bufsize - buf_len - 1;
            }
        }

        /* Add component name */
        if (space_left > 0) {
            strncat(buf, components[i], space_left);
        }
    }
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
    int is_cpu_attach;

    /* Parse request */
    if (p9_parse_header(in_buf, in_len, &hdr) < 0) {
        return 0;
    }

    if (p9_parse_tattach(in_buf, in_len, &fid, &afid, uname, aname) < 0) {
        return p9_build_rerror(out_buf, hdr.tag, "invalid Tattach");
    }

    fprintf(stderr, "handle_tattach: fid=%u afid=%u user='%s' aname='%s' client_fd=%d\n",
            fid, afid, uname, aname, current_client_fd);

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

    fprintf(stderr, "handle_tauth: user='%s' aname='%s' tag=%u\n", uname, aname, hdr.tag);

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
    fprintf(stderr, "handle_twalk: walking from '%s' through %d components\n",
            node->name ? node->name : "(null)", nwname);
    for (i = 0; i < nwname; i++) {
        /* Get string length from buffer (stored 2 bytes before string) */
        uint16_t name_len = le_get16((uint8_t*)(wnames[i] - 2));
        int found = 0;

        fprintf(stderr, "  component[%d] = '%.*s' (len=%u)\n", i, name_len, wnames[i], name_len);

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
            fprintf(stderr, "  -> not found!\n");
            /* No need to free wnames - they point into input buffer */
            return p9_build_rerror(out_buf, hdr.tag, "file not found");
        }
        fprintf(stderr, "  -> '%s' (qid.type=%02X)\n", newnode->name, newnode->qid.type);
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

    fprintf(stderr, "handle_tremove: fid=%u tag=%u\n", fid, hdr.tag);

    /* Get FID - return error if fid doesn't exist */
    fid_obj = fid_get(fid);
    if (fid_obj == NULL) {
        fprintf(stderr, "handle_tremove: fid %u not found\n", fid);
        return p9_build_rerror(out_buf, hdr.tag, "fid not found");
    }

    node = fid_obj->node;
    fprintf(stderr, "handle_tremove: removing '%s'\n", node->name ? node->name : "(null)");

    /* Attempt removal from tree */
    remove_result = tree_remove_node(node);

    /* ALWAYS clunk the fid per 9P spec, even if removal fails */
    fid_clunk(fid);

    if (remove_result < 0) {
        fprintf(stderr, "handle_tremove: removal failed (e.g., root node)\n");
        return p9_build_rerror(out_buf, hdr.tag, "permission denied");
    }

    fprintf(stderr, "handle_tremove: removal succeeded\n");
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

    fprintf(stderr, "dispatch_9p: received message type=%d (%s) tag=%u len=%zu\n",
            hdr.type, p9_msg_type_to_str(hdr.type), hdr.tag, in_len);

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
            fprintf(stderr, "dispatch_9p: unknown message type %d\n", hdr.type);
            result = p9_build_rerror(out_buf, hdr.tag, "not supported");
            break;
    }

    /* Log response */
    if (result > 0) {
        uint32_t resp_size = le_get32(out_buf);
        uint8_t resp_type = out_buf[4];

        fprintf(stderr, "dispatch_9p: sending response type=%d (%s) size=%u len=%zu\n",
                resp_type, p9_msg_type_to_str(resp_type), resp_size, result);
        fprintf(stderr, "Response hexdump:\n");
        hexdump(out_buf, result);
    } else {
        fprintf(stderr, "dispatch_9p: ERROR - failed to build response (result=%zu)\n", result);
    }

    return result;
}

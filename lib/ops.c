/*
 * 9P Operation Handlers
 */

#include "kryon.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/*
 * External function from Marrow library
 * Creates /dev/draw/[n] directory nodes
 */
extern P9Node *drawconn_create_dir(int conn_id);

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
int handle_tversion(int client_fd, const Fcall *f)
{
    uint32_t final_msize;
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;

    /* Validate message type */
    if (f->type != Tversion) {
        return -1;
    }

    /* We only support 9P2000 */
    if (f->version == NULL || strcmp(f->version, "9P2000") != 0) {
        return -1;
    }

    /* Negotiate message size */
    final_msize = f->msize;
    if (final_msize > P9_MAX_MSG) {
        final_msize = P9_MAX_MSG;
    }
    if (final_msize < 256) {
        final_msize = 256;
    }

    negotiated_msize = final_msize;

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rversion;
    r.tag = f->tag;
    r.msize = final_msize;
    r.version = "9P2000";

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tattach
 */
int handle_tattach(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *root;

    /* Validate message type */
    if (f->type != Tattach) {
        return -1;
    }

    /* Get root node */
    root = tree_root();
    if (root == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "no root";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Create FID */
    fid_obj = fid_new(f->fid, root);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid in use";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rattach;
    r.tag = f->tag;
    r.qid = root->qid;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tauth
 * Return Rauth with QID to indicate no authentication required
 * This is what many 9P servers do
 */
int handle_tauth(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    Qid qid;

    /* Validate message type */
    if (f->type != Tauth) {
        return -1;
    }

    /* Return Rauth with QID indicating authentication file */
    /* Use QID path 1 for the auth fid */
    qid.type = QTAUTH;
    qid.vers = 0;
    qid.path = 1;

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rauth;
    r.tag = f->tag;
    r.qid = qid;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Twalk
 */
int handle_twalk(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj, *newfid_obj;
    P9Node *node, *newnode;
    Qid wqid[P9_MAX_WELEM];
    int i;
    uint16_t name_len;
    int found;
    int conn_id;
    P9Node *dynamic_node;

    /* Validate message type */
    if (f->type != Twalk) {
        return -1;
    }

    /* Get source FID */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Walk through all path components */
    newnode = node;
    for (i = 0; i < f->nwname; i++) {
        char *wname = f->wname[i];
        name_len = strlen(wname);
        found = 0;

        /* "." means current directory */
        if (name_len == 1 && wname[0] == '.') {
            /* newnode stays the same */
            found = 1;
        }
        /* ".." means parent directory */
        else if (name_len == 2 && wname[0] == '.' && wname[1] == '.') {
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
                if (wname[j] < '0' || wname[j] > '9') {
                    is_numeric = 0;
                    break;
                }
            }
            if (is_numeric && name_len > 0 && name_len < 16) {
                char temp_name[16];
                memcpy(temp_name, wname, name_len);
                temp_name[name_len] = '\0';
                conn_id = atoi(temp_name);
                dynamic_node = drawconn_create_dir(conn_id);
                if (dynamic_node != NULL) {
                    /* Add to children if not already present */
                    int already_exists = 0;
                    int k;
                    for (k = 0; k < newnode->nchildren; k++) {
                        if (newnode->children[k] != NULL &&
                            strlen(newnode->children[k]->name) == name_len &&
                            memcmp(newnode->children[k]->name, wname, name_len) == 0) {
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
                        memcmp(newnode->children[j]->name, wname, name_len) == 0) {
                        newnode = newnode->children[j];
                        found = 1;
                        break;
                    }
                }
            }
        }

        if (!found) {
            /* Send Rerror */
            memset(&r, 0, sizeof(r));
            r.type = Rerror;
            r.tag = f->tag;
            r.ename = "file not found";
            outlen = convS2M(&r, outbuf, sizeof(outbuf));
            if (outlen == 0) return -1;
            return write(client_fd, outbuf, outlen);
        }
        /* Store QID for this component */
        wqid[i] = newnode->qid;
    }

    /* Create new FID pointing to final node */
    newfid_obj = fid_new(f->newfid, newnode);
    if (newfid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "newfid in use";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rwalk;
    r.tag = f->tag;
    r.nwqid = f->nwname;
    memcpy(r.wqid, wqid, sizeof(Qid) * r.nwqid);

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Topen
 */
int handle_topen(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *node;

    /* Validate message type */
    if (f->type != Topen) {
        return -1;
    }

    /* Get FID */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Check permissions */
    if (f->mode == OREAD || f->mode == ORDWR) {
        /* Read OK */
    }
    if (f->mode == OWRITE || f->mode == ORDWR) {
        /* Write OK */
    }

    /* Mark as open */
    fid_obj->is_open = 1;
    fid_obj->mode = f->mode;

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Ropen;
    r.tag = f->tag;
    r.qid = node->qid;
    r.iounit = 0; /* No preferred I/O size */

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Forward declarations for tree functions
 */
extern ssize_t node_read(P9Node *node, char *buf, size_t count, uint64_t offset);
extern ssize_t node_write(P9Node *node, const char *buf, size_t count, uint64_t offset);

/*
 * Handle Tread for directories
 * Returns packed stat entries for directory contents
 * NOTE: This function is currently disabled and needs to be reimplemented
 * to use lib9's Dir type and convD2M function.
 */
static int handle_directory_read(int client_fd, P9Fid *fid_obj, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;

    /* Directory reading needs to be reimplemented using lib9's Dir type
     * and convD2M function. For now, return an error. */

    memset(&r, 0, sizeof(r));
    r.type = Rerror;
    r.tag = f->tag;
    r.ename = "directory read not yet implemented";
    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) return -1;
    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tread
 */
int handle_tread(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *node;
    ssize_t nread;
    char data[P9_MAX_MSG];
    uint32_t count;

    /* Validate message type */
    if (f->type != Tread) {
        return -1;
    }

    /* Get FID */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Check if directory */
    if (node->qid.type & QTDIR) {
        return handle_directory_read(client_fd, fid_obj, f);
    }

    /* Limit count */
    count = f->count;
    if (count > negotiated_msize - 24) {
        count = negotiated_msize - 24;
    }

    /* Read from file */
    nread = node_read(node, data, count, f->offset);
    if (nread < 0) {
        fprintf(stderr, "handle_tread: read error for node '%s'\n", node->name);
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "read error";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rread;
    r.tag = f->tag;
    r.count = nread;
    r.data = data;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Twrite
 */
int handle_twrite(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *node;
    ssize_t nwritten;

    /* Validate message type */
    if (f->type != Twrite) {
        return -1;
    }

    /* Get FID */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Check if directory */
    if (node->qid.type & QTDIR) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "is directory";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Write to file */
    nwritten = node_write(node, (const char*)f->data, f->count, f->offset);
    if (nwritten < 0) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "write error";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rwrite;
    r.tag = f->tag;
    r.count = nwritten;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tclunk
 */
int handle_tclunk(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;

    /* Validate message type */
    if (f->type != Tclunk) {
        return -1;
    }

    /* Clunk FID */
    if (fid_clunk(f->fid) < 0) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rclunk;
    r.tag = f->tag;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tstat
 */
int handle_tstat(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *node;
    Dir stat;

    /* Validate message type */
    if (f->type != Tstat) {
        return -1;
    }

    /* Get FID */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Build stat structure using lib9's Dir */
    memset(&stat, 0, sizeof(stat));
    stat.type = 0;
    stat.dev = 0;
    stat.qid = node->qid;
    stat.mode = node->mode;
    stat.atime = node->atime;
    stat.mtime = node->mtime;
    stat.length = node->length;
    stat.name = node->name;
    stat.uid = "none";
    stat.gid = "none";
    stat.muid = "none";

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rstat;
    r.tag = f->tag;

    /* Convert Dir struct to wire format */
    {
        static uchar statbuf[P9_MAX_MSG];
        r.nstat = convD2M(&stat, statbuf, sizeof(statbuf));
        r.stat = statbuf;
    }

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Handle Tremove
 * Removes a node from the tree and clunks the FID
 */
int handle_tremove(int client_fd, const Fcall *f)
{
    Fcall r;
    uint8_t outbuf[P9_MAX_MSG];
    uint outlen;
    P9Fid *fid_obj;
    P9Node *node;
    int remove_result;

    /* Validate message type */
    if (f->type != Tremove) {
        return -1;
    }

    /* Get FID - return error if fid doesn't exist */
    fid_obj = fid_get(f->fid);
    if (fid_obj == NULL) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "fid not found";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    node = fid_obj->node;

    /* Attempt removal from tree */
    remove_result = tree_remove_node(node);

    /* ALWAYS clunk the fid per 9P spec, even if removal fails */
    fid_clunk(f->fid);

    if (remove_result < 0) {
        /* Send Rerror */
        memset(&r, 0, sizeof(r));
        r.type = Rerror;
        r.tag = f->tag;
        r.ename = "permission denied";
        outlen = convS2M(&r, outbuf, sizeof(outbuf));
        if (outlen == 0) return -1;
        return write(client_fd, outbuf, outlen);
    }

    /* Build response using lib9 */
    memset(&r, 0, sizeof(r));
    r.type = Rremove;
    r.tag = f->tag;

    outlen = convS2M(&r, outbuf, sizeof(outbuf));
    if (outlen == 0) {
        return -1;
    }

    return write(client_fd, outbuf, outlen);
}

/*
 * Main 9P message dispatcher
 */
size_t dispatch_9p(const uint8_t *in_buf, size_t in_len, uint8_t *out_buf)
{
    Fcall f;
    uint parsed;
    int result;

    /* Parse message using lib9 */
    parsed = convM2S((uchar*)in_buf, in_len, &f);
    if (parsed == 0) {
        fprintf(stderr, "dispatch_9p: failed to parse message\n");
        return 0;
    }

    /* Dispatch based on message type */
    switch (f.type) {
        case Tversion:
            result = handle_tversion(current_client_fd, &f);
            break;

        case Tauth:
            result = handle_tauth(current_client_fd, &f);
            break;

        case Tattach:
            result = handle_tattach(current_client_fd, &f);
            break;

        case Twalk:
            result = handle_twalk(current_client_fd, &f);
            break;

        case Topen:
            result = handle_topen(current_client_fd, &f);
            break;

        case Tread:
            result = handle_tread(current_client_fd, &f);
            break;

        case Twrite:
            result = handle_twrite(current_client_fd, &f);
            break;

        case Tclunk:
            result = handle_tclunk(current_client_fd, &f);
            break;

        case Tremove:
            result = handle_tremove(current_client_fd, &f);
            break;

        case Tstat:
            result = handle_tstat(current_client_fd, &f);
            break;

        default:
            /* Unknown message type - send Rerror */
            {
                Fcall r;
                uint outlen;
                memset(&r, 0, sizeof(r));
                r.type = Rerror;
                r.tag = f.tag;
                r.ename = "not supported";
                outlen = convS2M(&r, out_buf, P9_MAX_MSG);
                if (outlen == 0) return 0;
                result = write(current_client_fd, out_buf, outlen);
            }
            break;
    }

    return (result > 0) ? result : 0;
}

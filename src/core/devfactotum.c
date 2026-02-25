/*
 * Kryon Authentication - /mnt/factotum Virtual Filesystem
 * C89/C90 compliant
 *
 * Based on 9front factotum filesystem
 */

#include "devfactotum.h"
#include "kryon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

/*
 * External reference to global key list
 */
extern FactotumKey *g_keys;

/*
 * Global factotum directory node (for external access)
 */
static P9Node *g_factotum_dir = NULL;

/*
 * Log buffer (circular buffer)
 */
#define LOG_SIZE 4096
static char g_log_buf[LOG_SIZE];
static int g_log_pos = 0;
static int g_log_len = 0;

/*
 * Add message to log
 */
static void log_add(const char *msg)
{
    int len;
    int i;

    len = strlen(msg);

    for (i = 0; i < len; i++) {
        g_log_buf[g_log_pos] = msg[i];
        g_log_pos = (g_log_pos + 1) % LOG_SIZE;
        if (g_log_len < LOG_SIZE) {
            g_log_len++;
        }
    }
}

/*
 * Read from log buffer
 */
ssize_t factotum_log_read(char *buf, size_t count, uint64_t offset)
{
    int start;
    int to_read;
    int i;

    if (offset >= (uint64_t)g_log_len) {
        return 0;
    }

    start = (g_log_pos - g_log_len + offset) % LOG_SIZE;
    to_read = g_log_len - offset;

    if (to_read > (int)count) {
        to_read = count;
    }

    for (i = 0; i < to_read; i++) {
        buf[i] = g_log_buf[(start + i) % LOG_SIZE];
    }

    return to_read;
}

/*
 * Write to log (typically not used directly)
 */
ssize_t factotum_log_write(const char *buf, size_t count, uint64_t offset)
{
    (void)offset;

    /* Log messages are added internally via log_add() */
    /* External writes are ignored */

    return count;
}

/*
 * ctl file - key management interface
 * Read returns list of keys (with passwords hidden)
 */
ssize_t factotum_ctl_read(char *buf, size_t count, uint64_t offset)
{
    static char key_buf[4096];
    static int key_buf_len = 0;
    static int key_buf_initialized = 0;

    /* Build key list on first call */
    if (!key_buf_initialized) {
        FactotumKey *key;
        FactotumAttr *attr;
        char tmp[256];
        int proto;

        key_buf[0] = '\0';
        key_buf_len = 0;

        /* Get global key list from factotum_keys.c */
        extern FactotumKey *g_keys;

        key = g_keys;
        while (key != NULL && key_buf_len < (int)sizeof(key_buf) - 1) {
            int len;

            len = snprintf(tmp, sizeof(tmp), "key");

            /* Write public attributes */
            attr = key->attr;
            while (attr != NULL && key_buf_len < (int)sizeof(key_buf) - 1) {
                len = snprintf(tmp, sizeof(tmp), " %s=%s",
                               attr->name, attr->value);
                if (key_buf_len + len < (int)sizeof(key_buf)) {
                    strcat(key_buf, tmp);
                    key_buf_len += len;
                }
                attr = attr->next;
            }

            /* Write private attributes (hide passwords) */
            attr = key->privattr;
            while (attr != NULL && key_buf_len < (int)sizeof(key_buf) - 1) {
                if (strcmp(attr->name, "password") == 0) {
                    len = snprintf(tmp, sizeof(tmp), " !%s=?", attr->name);
                } else {
                    len = snprintf(tmp, sizeof(tmp), " !%s=%s",
                                   attr->name, attr->value);
                }
                if (key_buf_len + len < (int)sizeof(key_buf)) {
                    strcat(key_buf, tmp);
                    key_buf_len += len;
                }
                attr = attr->next;
            }

            if (key_buf_len + 2 < (int)sizeof(key_buf)) {
                strcat(key_buf, "\n");
                key_buf_len += 1;
            }

            key = key->next;
        }

        key_buf_initialized = 1;
    }

    /* Read from buffer */
    if (offset >= (uint64_t)key_buf_len) {
        return 0;
    }

    if (offset + count > (uint64_t)key_buf_len) {
        count = key_buf_len - offset;
    }

    memcpy(buf, key_buf + offset, count);

    return count;
}

/*
 * ctl file - write accepts key commands
 * "key proto=dp9ik dom=localhost user=glenda !password=secret"
 * "delkey proto=dp9ik dom=localhost user=glenda"
 */
ssize_t factotum_ctl_write(const char *buf, size_t count, uint64_t offset)
{
    char *cmd_copy;
    char log_msg[512];

    (void)offset;

    /* Make a copy of the command */
    cmd_copy = (char *)malloc(count + 1);
    if (cmd_copy == NULL) {
        return -1;
    }

    memcpy(cmd_copy, buf, count);
    cmd_copy[count] = '\0';

    /* Remove trailing newline */
    if (count > 0 && cmd_copy[count - 1] == '\n') {
        cmd_copy[count - 1] = '\0';
    }

    /* Log the command */
    snprintf(log_msg, sizeof(log_msg), "[%ld] ctl: %s\n",
             (long)time(NULL), cmd_copy);
    log_add(log_msg);

    /* Execute command */
    if (factotum_parse_key_line(cmd_copy) < 0) {
        free(cmd_copy);
        return -1;
    }

    free(cmd_copy);

    return count;
}

/*
 * proto file - available protocols list (read-only)
 */
ssize_t factotum_proto_read(char *buf, size_t count, uint64_t offset)
{
    static const char proto_list[] =
        "p9sk1\n"
        "dp9ik\n"
        "pass\n"
        "apop\n"
        "chap\n"
        "cram\n"
        "httpdigest\n"
        "mschap\n"
        "ntlm\n"
        "rsa\n"
        "ecdsa\n"
        "totp\n"
        "wpapsk\n";

    size_t len;

    len = strlen(proto_list);

    if (offset >= len) {
        return 0;
    }

    if (offset + count > len) {
        count = len - offset;
    }

    memcpy(buf, proto_list + offset, count);

    return count;
}

/*
 * confirm file - key confirmation interface
 * (not fully implemented yet)
 */
ssize_t factotum_confirm_read(char *buf, size_t count, uint64_t offset)
{
    /* Confirm interface allows reading confirmation status */
    /* For now, return empty */

    (void)offset;

    buf[0] = '\0';

    return 0;
}

/*
 * confirm file - write accepts confirmations
 */
ssize_t factotum_confirm_write(const char *buf, size_t count,
                               uint64_t offset)
{
    char log_msg[512];

    (void)offset;

    snprintf(log_msg, sizeof(log_msg), "[%ld] confirm: %.*s\n",
             (long)time(NULL), (int)count, buf);
    log_add(log_msg);

    return count;
}

/*
 * needkey file - key prompting interface
 */
ssize_t factotum_needkey_read(char *buf, size_t count, uint64_t offset)
{
    /* Needkey interface allows reading key prompts */
    /* For now, return empty */

    (void)offset;

    buf[0] = '\0';

    return 0;
}

/*
 * needkey file - write prompts for keys
 */
ssize_t factotum_needkey_write(const char *buf, size_t count,
                               uint64_t offset)
{
    char log_msg[512];

    (void)offset;

    snprintf(log_msg, sizeof(log_msg), "[%ld] needkey: %.*s\n",
             (long)time(NULL), (int)count, buf);
    log_add(log_msg);

    return count;
}

/*
 * rpc file - RPC interface for authentication operations
 * (not fully implemented yet)
 */
ssize_t factotum_rpc_read(char *buf, size_t count, uint64_t offset)
{
    /* RPC interface allows performing authentication operations */
    /* For now, return empty */

    (void)offset;

    buf[0] = '\0';

    return 0;
}

/*
 * rpc file - write accepts RPC commands
 */
ssize_t factotum_rpc_write(const char *buf, size_t count, uint64_t offset)
{
    char log_msg[512];

    (void)offset;

    snprintf(log_msg, sizeof(log_msg), "[%ld] rpc: %.*s\n",
             (long)time(NULL), (int)count, buf);
    log_add(log_msg);

    return count;
}

/*
 * Wrapper functions for tree_create_file compatibility
 */
static ssize_t ctl_read_wrapper(char *buf, size_t count, uint64_t offset,
                                void *data)
{
    (void)data;
    return factotum_ctl_read(buf, count, offset);
}

static ssize_t ctl_write_wrapper(const char *buf, size_t count, uint64_t offset,
                                 void *data)
{
    (void)data;
    return factotum_ctl_write(buf, count, offset);
}

static ssize_t proto_read_wrapper(char *buf, size_t count, uint64_t offset,
                                  void *data)
{
    (void)data;
    return factotum_proto_read(buf, count, offset);
}

static ssize_t confirm_read_wrapper(char *buf, size_t count, uint64_t offset,
                                    void *data)
{
    (void)data;
    return factotum_confirm_read(buf, count, offset);
}

static ssize_t confirm_write_wrapper(const char *buf, size_t count,
                                     uint64_t offset, void *data)
{
    (void)data;
    return factotum_confirm_write(buf, count, offset);
}

static ssize_t needkey_read_wrapper(char *buf, size_t count, uint64_t offset,
                                    void *data)
{
    (void)data;
    return factotum_needkey_read(buf, count, offset);
}

static ssize_t needkey_write_wrapper(const char *buf, size_t count,
                                     uint64_t offset, void *data)
{
    (void)data;
    return factotum_needkey_write(buf, count, offset);
}

static ssize_t rpc_read_wrapper(char *buf, size_t count, uint64_t offset,
                                void *data)
{
    (void)data;
    return factotum_rpc_read(buf, count, offset);
}

static ssize_t rpc_write_wrapper(const char *buf, size_t count, uint64_t offset,
                                 void *data)
{
    (void)data;
    return factotum_rpc_write(buf, count, offset);
}

static ssize_t log_read_wrapper(char *buf, size_t count, uint64_t offset,
                                void *data)
{
    (void)data;
    return factotum_log_read(buf, count, offset);
}

static ssize_t log_write_wrapper(const char *buf, size_t count,
                                uint64_t offset, void *data)
{
    (void)data;
    return factotum_log_write(buf, count, offset);
}

/*
 * Initialize /mnt/factotum filesystem hierarchy
 * Returns 0 on success, -1 on error
 */
int factotum_init(void *root_dir)
{
    P9Node *root = (P9Node *)root_dir;
    P9Node *mnt_dir;
    P9Node *factotum_dir;
    P9Node *file;

    /* Find or create /mnt */
    mnt_dir = tree_walk(root, "mnt");
    if (mnt_dir == NULL) {
        mnt_dir = tree_create_dir(root, "mnt");
        if (mnt_dir == NULL) {
            fprintf(stderr, "factotum_init: failed to create /mnt\n");
            return -1;
        }
    }

    /* Create /mnt/factotum directory */
    factotum_dir = tree_create_dir(mnt_dir, "factotum");
    if (factotum_dir == NULL) {
        fprintf(stderr, "factotum_init: failed to create /mnt/factotum\n");
        return -1;
    }

    g_factotum_dir = factotum_dir;

    /* Create confirm file */
    file = tree_create_file(factotum_dir, "confirm", NULL,
                            (P9ReadFunc)confirm_read_wrapper,
                            (P9WriteFunc)confirm_write_wrapper);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create confirm file\n");
    }

    /* Create needkey file */
    file = tree_create_file(factotum_dir, "needkey", NULL,
                            (P9ReadFunc)needkey_read_wrapper,
                            (P9WriteFunc)needkey_write_wrapper);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create needkey file\n");
    }

    /* Create ctl file */
    file = tree_create_file(factotum_dir, "ctl", NULL,
                            (P9ReadFunc)ctl_read_wrapper,
                            (P9WriteFunc)ctl_write_wrapper);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create ctl file\n");
    }

    /* Create rpc file */
    file = tree_create_file(factotum_dir, "rpc", NULL,
                            (P9ReadFunc)rpc_read_wrapper,
                            (P9WriteFunc)rpc_write_wrapper);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create rpc file\n");
    }

    /* Create proto file (read-only) */
    file = tree_create_file(factotum_dir, "proto", NULL,
                            (P9ReadFunc)proto_read_wrapper,
                            NULL);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create proto file\n");
    }

    /* Create log file */
    file = tree_create_file(factotum_dir, "log", NULL,
                            (P9ReadFunc)log_read_wrapper,
                            (P9WriteFunc)log_write_wrapper);
    if (file == NULL) {
        fprintf(stderr, "factotum_init: failed to create log file\n");
    }

    /* Initialize log buffer */
    memset(g_log_buf, 0, sizeof(g_log_buf));
    g_log_pos = 0;
    g_log_len = 0;

    /* Log initialization */
    log_add("factotum: filesystem initialized\n");

    fprintf(stderr, "Created /mnt/factotum hierarchy\n");

    return 0;
}

/*
 * Get factotum directory (for external access)
 */
P9Node *factotum_get_dir(void)
{
    return g_factotum_dir;
}

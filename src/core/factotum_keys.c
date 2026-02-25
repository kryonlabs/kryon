/*
 * Kryon Authentication - Key Storage
 * C89/C90 compliant
 */

#include "devfactotum.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/*
 * Global key storage
 */
FactotumKey *g_keys = NULL;
static int g_key_count = 0;

/*
 * Parse attribute-value pair
 * Format: "name=value" or "!name=value" (private attribute)
 * Returns 0 on success, -1 on error
 */
static int parse_attr(const char *str, char **name_out, char **value_out,
                      int *is_private_out)
{
    const char *equals;
    const char *start;
    char *name;
    char *value;
    int is_private;
    size_t name_len;
    size_t value_len;

    /* Check for private attribute prefix (!) */
    start = str;
    is_private = 0;

    if (start[0] == '!') {
        is_private = 1;
        start++;
    }

    /* Find equals sign */
    equals = strchr(start, '=');
    if (equals == NULL) {
        return -1;
    }

    /* Extract name */
    name_len = equals - start;
    name = (char *)malloc(name_len + 1);
    if (name == NULL) {
        return -1;
    }
    memcpy(name, start, name_len);
    name[name_len] = '\0';

    /* Extract value */
    value_len = strlen(equals + 1);
    value = (char *)malloc(value_len + 1);
    if (value == NULL) {
        free(name);
        return -1;
    }
    strcpy(value, equals + 1);

    *name_out = name;
    *value_out = value;
    *is_private_out = is_private;

    return 0;
}

/*
 * Free attribute list
 */
static void free_attrs(FactotumAttr *attr)
{
    FactotumAttr *next;

    while (attr != NULL) {
        next = attr->next;
        if (attr->name != NULL) {
            free(attr->name);
        }
        if (attr->value != NULL) {
            free(attr->value);
        }
        free(attr);
        attr = next;
    }
}

/*
 * Get attribute value from list
 */
const char *factotum_get_attr(FactotumAttr *attr, const char *name)
{
    while (attr != NULL) {
        if (strcmp(attr->name, name) == 0) {
            return attr->value;
        }
        attr = attr->next;
    }
    return NULL;
}

/*
 * Check if key matches criteria
 */
static int key_matches(FactotumKey *key, const char *proto,
                       const char *dom, const char *user)
{
    const char *key_proto;
    const char *key_dom;
    const char *key_user;

    if (key == NULL) {
        return 0;
    }

    /* Check proto */
    if (proto != NULL) {
        key_proto = factotum_get_attr(key->attr, "proto");
        if (key_proto == NULL || strcmp(key_proto, proto) != 0) {
            return 0;
        }
    }

    /* Check domain */
    if (dom != NULL) {
        key_dom = factotum_get_attr(key->attr, "dom");
        if (key_dom == NULL || strcmp(key_dom, dom) != 0) {
            return 0;
        }
    }

    /* Check user */
    if (user != NULL) {
        key_user = factotum_get_attr(key->attr, "user");
        if (key_user == NULL || strcmp(key_user, user) != 0) {
            return 0;
        }
    }

    return 1;
}

/*
 * Get protocol type from string
 */
static ProtoType proto_from_string(const char *proto_str)
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
    } else if (strcmp(proto_str, "apop") == 0) {
        return PROTO_APOP;
    } else if (strcmp(proto_str, "chap") == 0) {
        return PROTO_CHAP;
    } else if (strcmp(proto_str, "cram") == 0) {
        return PROTO_CRAM;
    } else if (strcmp(proto_str, "httpdigest") == 0) {
        return PROTO_HTTPDIGEST;
    } else if (strcmp(proto_str, "mschap") == 0) {
        return PROTO_MSCHAP;
    } else if (strcmp(proto_str, "ntlm") == 0) {
        return PROTO_NTLM;
    } else if (strcmp(proto_str, "rsa") == 0) {
        return PROTO_RSA;
    } else if (strcmp(proto_str, "ecdsa") == 0) {
        return PROTO_ECDSA;
    } else if (strcmp(proto_str, "totp") == 0) {
        return PROTO_TOTP;
    } else if (strcmp(proto_str, "wpapsk") == 0) {
        return PROTO_WPAPSK;
    }

    return PROTO_NONE;
}

/*
 * Parse a single key line and add to global key list
 * Format: "key proto=dp9ik dom=localhost user=glenda !password=secret"
 * Returns 0 on success, -1 on error
 */
int factotum_parse_key_line(const char *line)
{
    char *line_copy;
    char *saveptr;
    char *token;
    FactotumKey *key;
    FactotumAttr **attr_ptr;
    FactotumAttr **privattr_ptr;
    const char *proto_str;

    /* Skip empty lines and comments */
    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }

    /* Make a copy of the line for strtok */
    line_copy = strdup(line);
    if (line_copy == NULL) {
        return -1;
    }

    /* Allocate key structure */
    key = (FactotumKey *)malloc(sizeof(FactotumKey));
    if (key == NULL) {
        free(line_copy);
        return -1;
    }
    memset(key, 0, sizeof(FactotumKey));

    attr_ptr = &key->attr;
    privattr_ptr = &key->privattr;

    /* Parse tokens */
    token = strtok_r(line_copy, " \t", &saveptr);

    /* First token should be "key" or "delkey" */
    if (token == NULL || strcmp(token, "key") != 0) {
        if (token != NULL && strcmp(token, "delkey") == 0) {
            /* Delete key command */
            char *proto = NULL;
            char *dom = NULL;
            char *user = NULL;

            while ((token = strtok_r(NULL, " \t", &saveptr)) != NULL) {
                char *name;
                char *value;
                int is_private;

                if (parse_attr(token, &name, &value, &is_private) == 0) {
                    if (strcmp(name, "proto") == 0) {
                        proto = value;
                    } else if (strcmp(name, "dom") == 0) {
                        dom = value;
                    } else if (strcmp(name, "user") == 0) {
                        user = value;
                    }
                    free(name);
                }
            }

            factotum_del_key(proto);  /* Will be implemented */
            free(line_copy);
            free(key);

            if (proto != NULL) free((void *)proto);
            if (dom != NULL) free((void *)dom);
            if (user != NULL) free((void *)user);

            return 0;
        }
        free(line_copy);
        free(key);
        return -1;
    }

    /* Parse attributes */
    while ((token = strtok_r(NULL, " \t", &saveptr)) != NULL) {
        char *name;
        char *value;
        int is_private;

        if (parse_attr(token, &name, &value, &is_private) == 0) {
            FactotumAttr *attr;
            FactotumAttr **target;

            attr = (FactotumAttr *)malloc(sizeof(FactotumAttr));
            if (attr == NULL) {
                free(name);
                free(value);
                continue;
            }

            attr->name = name;
            attr->value = value;
            attr->next = NULL;

            if (is_private) {
                target = privattr_ptr;
            } else {
                target = attr_ptr;
            }

            *target = attr;
            if (is_private) {
                privattr_ptr = &attr->next;
            } else {
                attr_ptr = &attr->next;
            }
        }
    }

    free(line_copy);

    /* Get protocol type */
    proto_str = factotum_get_attr(key->attr, "proto");
    key->proto_type = proto_from_string(proto_str);

    /* Add to global key list */
    key->next = g_keys;
    g_keys = key;
    g_key_count++;

    fprintf(stderr, "factotum: added key proto=%s dom=%s user=%s\n",
            proto_str ? proto_str : "?",
            factotum_get_attr(key->attr, "dom"),
            factotum_get_attr(key->attr, "user"));

    return 0;
}

/*
 * Add a key from a string format
 */
int factotum_add_key(const char *key_str)
{
    return factotum_parse_key_line(key_str);
}

/*
 * Delete a key matching the specification
 */
int factotum_del_key(const char *key_spec)
{
    FactotumKey *key, *prev;
    FactotumKey *to_delete = NULL;
    char *spec_copy;
    char *saveptr;
    char *token;
    const char *want_proto = NULL;
    const char *want_dom = NULL;
    const char *want_user = NULL;

    if (key_spec == NULL) {
        return -1;
    }

    /* Parse key specification */
    spec_copy = strdup(key_spec);
    if (spec_copy == NULL) {
        return -1;
    }

    while ((token = strtok_r(spec_copy, " \t", &saveptr)) != NULL) {
        char *name;
        char *value;
        int is_private;

        if (parse_attr(token, &name, &value, &is_private) == 0) {
            if (strcmp(name, "proto") == 0) {
                want_proto = value;
            } else if (strcmp(name, "dom") == 0) {
                want_dom = value;
            } else if (strcmp(name, "user") == 0) {
                want_user = value;
            }
            free(name);
        }
    }

    free(spec_copy);

    /* Find matching key */
    prev = NULL;
    key = g_keys;

    while (key != NULL) {
        if (key_matches(key, want_proto, want_dom, want_user)) {
            to_delete = key;
            break;
        }
        prev = key;
        key = key->next;
    }

    if (to_delete == NULL) {
        return -1;
    }

    /* Remove from list */
    if (prev == NULL) {
        g_keys = to_delete->next;
    } else {
        prev->next = to_delete->next;
    }

    factotum_free_key(to_delete);
    g_key_count--;

    if (want_proto != NULL) free((void *)want_proto);
    if (want_dom != NULL) free((void *)want_dom);
    if (want_user != NULL) free((void *)want_user);

    return 0;
}

/*
 * Find a key matching proto, domain, and user
 */
FactotumKey *factotum_find_key(const char *proto, const char *dom,
                                const char *user)
{
    FactotumKey *key;

    key = g_keys;

    while (key != NULL) {
        if (key_matches(key, proto, dom, user)) {
            return key;
        }
        key = key->next;
    }

    return NULL;
}

/*
 * Free a key structure
 */
void factotum_free_key(FactotumKey *key)
{
    if (key == NULL) {
        return;
    }

    free_attrs(key->attr);
    free_attrs(key->privattr);

    if (key->priv != NULL) {
        free(key->priv);
    }

    free(key);
}

/*
 * Load keys from file
 */
int factotum_load_keys(const char *path)
{
    FILE *f;
    char line[512];
    int line_num;

    if (path == NULL) {
        path = FACTOTUM_KEY_FILE;
    }

    f = fopen(path, "r");
    if (f == NULL) {
        fprintf(stderr, "factotum_load_keys: failed to open %s\n", path);
        return -1;
    }

    line_num = 0;

    while (fgets(line, sizeof(line), f) != NULL) {
        line_num++;

        /* Remove newline */
        line[strlen(line) - 1] = '\0';

        /* Parse and add key */
        if (factotum_parse_key_line(line) < 0) {
            fprintf(stderr, "factotum_load_keys: parse error at line %d\n",
                    line_num);
        }
    }

    fclose(f);

    fprintf(stderr, "factotum_load_keys: loaded %d keys from %s\n",
            g_key_count, path);

    return 0;
}

/*
 * Save keys to file
 */
int factotum_save_keys(const char *path)
{
    FILE *f;
    FactotumKey *key;
    FactotumAttr *attr;

    if (path == NULL) {
        path = FACTOTUM_KEY_FILE;
    }

    f = fopen(path, "w");
    if (f == NULL) {
        fprintf(stderr, "factotum_save_keys: failed to open %s\n", path);
        return -1;
    }

    key = g_keys;

    while (key != NULL) {
        fprintf(f, "key");

        /* Write public attributes */
        attr = key->attr;
        while (attr != NULL) {
            fprintf(f, " %s=%s", attr->name, attr->value);
            attr = attr->next;
        }

        /* Write private attributes (hide passwords) */
        attr = key->privattr;
        while (attr != NULL) {
            if (strcmp(attr->name, "password") == 0) {
                fprintf(f, " !%s=?", attr->name);
            } else {
                fprintf(f, " !%s=%s", attr->name, attr->value);
            }
            attr = attr->next;
        }

        fprintf(f, "\n");
        key = key->next;
    }

    fclose(f);

    return 0;
}

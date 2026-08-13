/*
 * k2b parser — reads raw .kry into a K2bFile (app metadata, state fields,
 * function bodies as classified statements). Independent of kc.
 *
 * A "logical line" joins physical lines while () / [] depth > 0 or a string is
 * open, so a multi-line widget call like
 *     Button((ButtonProps){
 *         .bounds = ...,
 *     })
 * becomes one statement. Block braces {} are NOT join triggers: a '{' seen at
 * paren depth 0 opens a block, so 'if Call(...) {' ends its logical line at
 * the call (parens balanced) and the body statements stay separate.
 */
#include "k2b.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define K2B_TEXT_MAX (1024 * 1024)

static char *
read_file(const char *path, long *out_len)
{
    FILE *f = fopen(path, "rb");
    char *buf;
    long len;

    if(f == NULL)
        die("%s: open failed: %s", path, strerror(errno));
    fseek(f, 0, SEEK_END);
    len = ftell(f);
    fseek(f, 0, SEEK_SET);
    buf = malloc(len + 1);
    if(buf == NULL)
        die("out of memory");
    if(fread(buf, 1, (size_t)len, f) != (size_t)len) {
        fclose(f);
        die("%s: read failed", path);
    }
    fclose(f);
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

static const char *
skip_ws(const char *s)
{
    while(*s == ' ' || *s == '\t')
        s++;
    return s;
}

static int
starts_word(const char *s, const char *w)
{
    return strncmp(s, w, strlen(w)) == 0 &&
           (s[strlen(w)] == '\0' || s[strlen(w)] == ' ' ||
            s[strlen(w)] == '\t' || s[strlen(w)] == '(' || s[strlen(w)] == '"');
}

static void
strip_trailing_ws(char *s)
{
    size_t n = strlen(s);

    while(n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                    s[n - 1] == '\r' || s[n - 1] == '\n'))
        s[--n] = '\0';
}

/* Does appending `line` leave an open ()/[] depth or string? Used to decide
 * whether the logical line continues onto the next physical line. */
static void
tally_depth(const char *s, int *paren, int *bracket, int *in_str)
{
    for(; *s != '\0'; s++) {
        if(*in_str) {
            if(*s == '\\' && s[1] != '\0')
                s++;
            else if(*s == '"')
                *in_str = 0;
            continue;
        }
        if(*s == '"') {
            *in_str = 1;
        } else if(*s == '(') {
            (*paren)++;
        } else if(*s == ')') {
            (*paren)--;
        } else if(*s == '[') {
            (*bracket)++;
        } else if(*s == ']') {
            (*bracket)--;
        }
    }
}

static int
classify_head(const char *t)
{
    if(starts_word(t, "if"))
        return K2B_STMT_IF;
    if(starts_word(t, "else if"))
        return K2B_STMT_IF;
    if(starts_word(t, "else"))
        return K2B_STMT_ELSE;
    if(starts_word(t, "while"))
        return K2B_STMT_WHILE;
    if(starts_word(t, "for"))
        return K2B_STMT_FOR;
    if(starts_word(t, "switch"))
        return K2B_STMT_SWITCH;
    if(starts_word(t, "case"))
        return K2B_STMT_CASE;
    if(starts_word(t, "return"))
        return K2B_STMT_RETURN;
    if(starts_word(t, "break"))
        return K2B_STMT_BREAK;
    if(starts_word(t, "continue"))
        return K2B_STMT_CONTINUE;
    if(strstr(t, ":=") != NULL)
        return K2B_STMT_DECL;
    if(strstr(t, " = ") != NULL || strncmp(t, "= ", 2) == 0)
        return K2B_STMT_ASSIGN;
    return K2B_STMT_EXPR;
}

static void
add_stmt(K2bFunction *fn, int kind, int depth, const char *text)
{
    K2bStmt *st;

    if(fn->stmt_count >= K2B_STMT_MAX)
        return;
    st = &fn->stmts[fn->stmt_count++];
    st->kind = kind;
    st->depth = depth;
    snprintf(st->text, sizeof(st->text), "%s", text);
}

/* Convert a `name: <rest>` state field to a C-style decl string that the
 * codegen's collect_state can parse (e.g. `flag: int = 0` -> `static int flag = 0;`,
 * `buf: [N] char = "x"` -> `static char buf[N] = "x";`). */
static void
format_state_decl(char *dst, size_t dst_size, const char *line)
{
    const char *colon = strchr(line, ':');
    char name[K2B_NAME_MAX];
    char sizebuf[32];
    const char *rest;
    const char *eq;
    const char *t;
    size_t n;

    if(colon == NULL)
        return;
    n = (size_t)(colon - line);
    while(n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t'))
        n--;
    if(n >= sizeof(name))
        n = sizeof(name) - 1;
    memcpy(name, line, n);
    name[n] = '\0';
    rest = skip_ws(colon + 1);
    sizebuf[0] = '\0';
    if(*rest == '[') {
        const char *cl = strchr(rest, ']');
        if(cl != NULL) {
            snprintf(sizebuf, sizeof(sizebuf), "%.*s", (int)(cl - rest + 1), rest);
            rest = skip_ws(cl + 1);
        }
    }
    eq = strchr(rest, '=');
    t = eq;
    if(t != NULL) {
        while(t > rest && (t[-1] == ' ' || t[-1] == '\t' || t[-1] == '*'))
            t--;
    }
    if(eq != NULL) {
        snprintf(dst, dst_size, "static %.*s %s%s %s;",
                 (int)(t - rest), rest, name, sizebuf, eq);
    } else {
        snprintf(dst, dst_size, "static %.*s %s%s;",
                 (int)((t ? t : rest + strlen(rest)) - rest), rest, name, sizebuf);
    }
}

int
k2b_parse_file(K2bFile *file, const char *path, const char *root)
{
    char *src;
    long srclen;
    char pending[K2B_LINE_MAX];
    int pending_len = 0;
    int paren = 0;
    int bracket = 0;
    int in_str = 0;
    enum { TOP, APP, STATE, FUNC } mode = TOP;
    int depth = 0;             /* block depth within a function body */
    K2bFunction *fn = NULL;
    const char *p;

    memset(file, 0, sizeof(*file));
    snprintf(file->path, sizeof(file->path), "%s", path);
    snprintf(file->root, sizeof(file->root), "%s", root == NULL ? "" : root);
    src = read_file(path, &srclen);

    p = src;
    while(*p != '\0') {
        char line[K2B_LINE_MAX];
        size_t ln = 0;
        const char *t;
        char *nl;
        int complete;

        /* extract one physical line */
        nl = strchr(p, '\n');
        ln = (nl != NULL) ? (size_t)(nl - p) : strlen(p);
        if(ln >= sizeof(line))
            ln = sizeof(line) - 1;
        memcpy(line, p, ln);
        line[ln] = '\0';
        if(nl != NULL)
            p = nl + 1;
        else
            p += ln;

        strip_trailing_ws(line);
        t = skip_ws(line);

        /* skip blank lines, #directives, line comments */
        if(t[0] == '\0' || t[0] == '#') {
            if(pending_len == 0)
                continue;
        }

        /* accumulate into pending */
        if(pending_len > 0) {
            if((size_t)pending_len + strlen(line) + 2 >= sizeof(pending))
                pending_len = 0;
            else {
                pending[pending_len++] = ' ';
                pending[pending_len] = '\0';
            }
        }
        strncat(pending, line, sizeof(pending) - pending_len - 1);
        pending_len = (int)strlen(pending);

        tally_depth(line, &paren, &bracket, &in_str);
        complete = (paren == 0 && bracket == 0 && in_str == 0);
        if(!complete)
            continue;

        /* logical line ready in `pending` */
        t = skip_ws(pending);
        if(t[0] == '\0' || t[0] == '#') {
            pending[0] = '\0';
            pending_len = 0;
            continue;
        }

        if(mode == TOP) {
            if(starts_word(t, "app")) {
                mode = APP;
            } else if(starts_word(t, "state") && strstr(t, "{") != NULL) {
                mode = STATE;
            } else if((starts_word(t, "screen") || starts_word(t, "preview") ||
                       starts_word(t, "page") || starts_word(t, "scene") ||
                       starts_word(t, "frame") || starts_word(t, "fn")) &&
                      file->function_count < K2B_FUNC_MAX) {
                const char *q = strchr(t, ' ');
                fn = &file->functions[file->function_count++];
                fn->stmt_count = 0;
                fn->screen[0] = '\0';
                if(q != NULL) {
                    size_t m = 0;
                    q = skip_ws(q);
                    while(*q && (isalnum((unsigned char)*q) || *q == '_') &&
                          m + 1 < sizeof(fn->screen))
                        fn->screen[m++] = *q++;
                    fn->screen[m] = '\0';
                }
                mode = FUNC;
                depth = 1;         /* the header '{' opened the function body */
            } else if(file->function_count < K2B_FUNC_MAX) {
                /* Jai-style function def: Name :: (args) { body } */
                char nm[K2B_NAME_MAX];
                size_t m = 0;
                const char *q = t;

                while(*q && (isalnum((unsigned char)*q) || *q == '_') &&
                      m + 1 < sizeof(nm))
                    nm[m++] = *q++;
                nm[m] = '\0';
                q = skip_ws(q);
                if(nm[0] != '\0' && strncmp(q, "::", 2) == 0) {
                    q = skip_ws(q + 2);
                    if(*q == '(' || *q == '{') {
                        fn = &file->functions[file->function_count++];
                        fn->stmt_count = 0;
                        snprintf(fn->screen, sizeof(fn->screen), "%s", nm);
                        mode = FUNC;
                        depth = 1;
                    }
                }
            }
            /* anything else at TOP (#extern, types, #global): ignore */
        } else if(mode == APP) {
            if(t[0] == '}') {
                mode = TOP;
            } else if(starts_word(t, "size")) {
                sscanf(t, "size %d %d", &file->app_width, &file->app_height);
            } else if(starts_word(t, "fps")) {
                file->app_fps = atoi(skip_ws(t + 3));
            } else if(starts_word(t, "theme")) {
                char mode2[32] = "";
                sscanf(t, "theme %127s %31s", file->app_theme, mode2);
                file->app_dark_mode = strcmp(mode2, "dark") == 0;
            } else if(starts_word(t, "font") && strstr(t, "examples")) {
                file->app_font_examples = 1;
            } else if(starts_word(t, "frame")) {
                sscanf(t, "frame %127s", file->app_frame);
            } else if(starts_word(t, "init")) {
                sscanf(t, "init %127s", file->app_init);
            } else if(starts_word(t, "scene")) {
                sscanf(t, "scene %127s", file->app_scene);
            } else if(starts_word(t, "shutdown")) {
                sscanf(t, "shutdown %127s", file->app_shutdown);
            }
        } else if(mode == STATE) {
            if(t[0] == '}') {
                mode = TOP;
            } else if(file->state_count < K2B_STATE_MAX) {
                format_state_decl(file->state[file->state_count],
                                  K2B_LINE_MAX, t);
                if(file->state[file->state_count][0] != '\0')
                    file->state_count++;
            }
        } else if(mode == FUNC) {
            if(t[0] == '}') {
                depth--;
                if(depth <= 0) {
                    mode = TOP;
                    depth = 0;
                } else {
                    add_stmt(fn, K2B_STMT_BLOCK_CLOSE, depth, "}");
                }
            } else {
                int opens = (pending_len > 0 &&
                             pending[pending_len - 1] == '{');
                char head[K2B_LINE_MAX];
                size_t hl;

                snprintf(head, sizeof(head), "%s", t);
                hl = strlen(head);
                if(opens && hl > 0 && head[hl - 1] == '{')
                    head[hl - 1] = '\0';
                strip_trailing_ws(head);
                add_stmt(fn, classify_head(skip_ws(head)), depth, t);
                if(opens)
                    depth++;
            }
        }

        pending[0] = '\0';
        pending_len = 0;
    }

    free(src);
    return 0;
}

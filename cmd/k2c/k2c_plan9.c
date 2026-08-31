/*
 * k2c_plan9.c - 8c-safe post-pass for generated C.
 *
 * The native Plan 9 compiler accepts C89 plus a few extensions, but not
 * __auto_type, compound literals with designated initializers, partial
 * positional aggregate initializers, or declarations inside for(). k2c
 * lowers Kry statements textually and has no type checker, so the
 * default output leans on those GNU forms. Under --plan9 this pass
 * rewrites the generated C before it is written:
 *
 *   __auto_type x = (Type){...};   ->  Type x = ...;   (type from cast)
 *   __auto_type x = fn(...);       ->  Ret x = fn(...); (header map)
 *   (Type){.a = 1, .b = 2}         ->  zeroed temp + field assignments
 *   (Type){0}                      ->  zeroed temp
 *   for(int i = 0; ...; ...) {...} ->  { int i; for(i = 0; ...) {...} }
 *
 * Each generated statement sits on one line, so the pass works
 * line-by-line exactly like the per-app scripts it replaces.
 */
#include "k2c_plan9.h"

#include <ctype.h>
#include <dirent.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PLAN9_MAX_INCLUDE_DIRS 16
#define PLAN9_MAX_PROTOS 8192
#define PLAN9_NAME_MAX 128
#define PLAN9_TYPE_MAX 128
#define PLAN9_LINE_MAX 65536
#define PLAN9_TEMP_PREFIX "__k2c_p9"

typedef struct {
    char name[PLAN9_NAME_MAX];
    char type[PLAN9_TYPE_MAX];
} Plan9Proto;

static char include_dirs[PLAN9_MAX_INCLUDE_DIRS][1024];
static int include_dir_count;
static Plan9Proto protos[PLAN9_MAX_PROTOS];
static int proto_count;
static int protos_loaded;
static int unresolved_count;
static int plan9_enabled;

void
k2c_plan9_set_enabled(int enabled)
{
    plan9_enabled = enabled;
    unresolved_count = 0;
}

int
k2c_plan9_enabled(void)
{
    return plan9_enabled;
}

/* ------------------------------------------------------------------ */
/* Growable output buffer                                            */
/* ------------------------------------------------------------------ */

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buf;

static int
buf_init(Buf *b)
{
    b->cap = 8192;
    b->len = 0;
    b->data = malloc(b->cap);
    if(b->data != NULL)
        b->data[0] = '\0';
    return b->data != NULL;
}

static int
buf_append(Buf *b, const char *text, size_t n)
{
    if(b->data == NULL)
        return -1;
    if(b->len + n + 1 > b->cap) {
        size_t cap = b->cap;
        char *grown;
        while(b->len + n + 1 > cap)
            cap *= 2;
        grown = realloc(b->data, cap);
        if(grown == NULL)
            return -1;
        b->data = grown;
        b->cap = cap;
    }
    memcpy(b->data + b->len, text, n);
    b->len += n;
    b->data[b->len] = '\0';
    return 0;
}

static int
buf_puts(Buf *b, const char *text)
{
    return buf_append(b, text, strlen(text));
}

static int
buf_printf(Buf *b, const char *fmt, ...)
{
    char tmp[16384];
    va_list ap;
    int n;

    va_start(ap, fmt);
    n = vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    if(n < 0)
        return -1;
    if((size_t)n >= sizeof(tmp))
        n = (int)sizeof(tmp) - 1;
    return buf_append(b, tmp, (size_t)n);
}

/* ------------------------------------------------------------------ */
/* Prototype map: name -> return type, from project headers          */
/* ------------------------------------------------------------------ */

static int
skip_head_word(const char *p, const char *word)
{
    size_t n = strlen(word);

    if(strncmp(p, word, n) != 0)
        return 0;
    if(p[n] == ' ' || p[n] == '\t')
        return (int)n + 1;
    return 0;
}

static int
proto_line_parse(const char *line, char *name, size_t name_size,
                 char *type, size_t type_size)
{
    const char *p = line;
    const char *args;
    const char *semi;
    const char *name_start;
    const char *name_end;
    const char *type_end;
    size_t n;
    int skipped;

    while(*p == ' ' || *p == '\t')
        p++;
    if(*p == '\0' || *p == '#' || *p == '/' || *p == '*')
        return 0;
    /* cut at a body opener: covers "RET name(args) {" headers and
     * one-line inline definitions alike */

    skipped = skip_head_word(p, "RLAPI");
    if(skipped == 0)
        skipped = skip_head_word(p, "KRYAPI");
    if(skipped == 0)
        skipped = skip_head_word(p, "extern");
    if(skipped == 0)
        skipped = skip_head_word(p, "static");
    if(skipped != 0)
        p += skipped;

    semi = strchr(p, ';');
    {
        const char *brace = strchr(p, '{');

        if(brace != NULL && (semi == NULL || brace < semi))
            semi = brace;
    }
    if(semi == NULL)
        return 0;
    args = strchr(p, '(');
    if(args == NULL || args > semi || args == p)
        return 0;

    name_end = args;
    while(name_end > p && (name_end[-1] == ' ' || name_end[-1] == '\t'
                           || name_end[-1] == '*'))
        name_end--;
    name_start = name_end;
    while(name_start > p && (isalnum((unsigned char)name_start[-1])
                             || name_start[-1] == '_'))
        name_start--;
    if(name_start == name_end)
        return 0;
    n = (size_t)(name_end - name_start);
    if(n >= name_size)
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';

    type_end = name_start;
    while(type_end > p && (type_end[-1] == ' ' || type_end[-1] == '\t'))
        type_end--;
    n = (size_t)(type_end - p);
    if(n == 0 || n >= type_size)
        return 0;
    memcpy(type, p, n);
    type[n] = '\0';
    if(strcmp(type, "void") == 0 || strcmp(type, "static") == 0)
        return 0;
    /* the return type must read like one: identifiers, qualifiers,
     * struct/enum tags, stars. Anything else means the line was
     * expression text from an inline body (raymath-style headers). */
    if(strpbrk(type, "=().,+/%<>!&|[]") != NULL)
        return 0;
    if(!(isalpha((unsigned char)type[0]) || type[0] == '_'))
        return 0;
    return 1;
}

/* Literal classification for an initializer expression. */
static int
init_is_int_literal(const char *s)
{
    int digits = 0;

    if(*s == '+' || *s == '-')
        s++;
    while(isdigit((unsigned char)*s)) {
        s++;
        digits++;
    }
    if(digits == 0)
        return 0;
    if(*s == 'u' || *s == 'U')
        s++;
    if(*s == 'l' || *s == 'L')
        s++;
    return *s == '\0' || *s == ';' || *s == ' ' || *s == '\t';
}

static int
expr_looks_float(const char *s)
{
    const char *p;

    for(p = s; *p != '\0' && *p != ';'; p++) {
        if(isdigit((unsigned char)p[0]) && p[1] == '.')
            return 1;
        if(isdigit((unsigned char)p[0]) && (p[1] == 'f' || p[1] == 'F')
           && !isalnum((unsigned char)p[2]) && p[2] != '_')
            return 1;
    }
    return 0;
}

static void
proto_add_macro(const char *line)
{
    /* #define NAME 123  (object-like, numeric) */
    const char *p = line;
    const char *name_start;
    const char *name_end;
    char name[PLAN9_NAME_MAX];
    char value[64];
    size_t n;
    int i;

    if(strncmp(p, "#define ", 8) != 0)
        return;
    p += 8;
    while(*p == ' ' || *p == '\t')
        p++;
    name_start = p;
    while(*p != '\0' && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    if(*p != '(') {
        name_end = p;
        n = (size_t)(name_end - name_start);
        if(n == 0 || n >= sizeof(name))
            return;
        memcpy(name, name_start, n);
        name[n] = '\0';
        while(*p == ' ' || *p == '\t')
            p++;
        for(i = 0; p[i] != '\0' && p[i] != '\n' && i < (int)sizeof(value) - 1;
            i++)
            value[i] = p[i];
        value[i] = '\0';
        if(init_is_int_literal(value) || expr_looks_float(value)) {
            int j;
            int taken = 0;

            for(j = 0; j < proto_count; j++) {
                if(strcmp(protos[j].name, name) == 0) {
                    taken = 1;
                    break;
                }
            }
            if(!taken && proto_count < PLAN9_MAX_PROTOS) {
                snprintf(protos[proto_count].name,
                         sizeof(protos[0].name), "%s", name);
                snprintf(protos[proto_count].type,
                         sizeof(protos[0].type), "%s",
                         expr_looks_float(value) ? "float" : "int");
                proto_count++;
            }
        }
    }
}

static void
proto_add_file(const char *path, int allow_c)
{
    FILE *f;
    char pending[16384];
    char pending_text[16384];
    char line[8192];

    pending[0] = '\0';
    f = fopen(path, "r");
    if(f == NULL)
        return;
    while(fgets(line, sizeof(line), f) != NULL) {
        char joined[16384];
        char name[PLAN9_NAME_MAX];
        char type[PLAN9_TYPE_MAX];
        int i;

        if(!allow_c) {
            proto_add_macro(line);
        } else {
            /* in .c files only static definition headers are recorded,
             * before any continuation joining so body lines never
             * accumulate into a candidate */
            const char *nospace = line + strspn(line, " \t");

            if(strncmp(nospace, "static ", 7) != 0)
                continue;
        }
        if(pending[0] != '\0') {
            snprintf(pending_text, sizeof(pending_text), "%s", pending);
            pending[0] = '\0';
            if(snprintf(joined, sizeof(joined), "%s%s", pending_text, line)
               >= (int)sizeof(joined))
                continue;
            if(strchr(joined, '{') != NULL)
                continue;
            if(strchr(joined, ';') == NULL) {
                if(strlen(joined) + 1 < sizeof(pending))
                    snprintf(pending, sizeof(pending), "%s", joined);
                continue;
            }
        } else {
            snprintf(joined, sizeof(joined), "%s", line);
            if(strchr(joined, '(') != NULL && strchr(joined, ';') == NULL
               && strchr(joined, '{') == NULL
               && strlen(joined) + 1 < sizeof(pending)) {
                snprintf(pending, sizeof(pending), "%s", joined);
                continue;
            }
        }
        if(!proto_line_parse(joined, name, sizeof(name), type, sizeof(type)))
            continue;
        for(i = 0; i < proto_count; i++) {
            if(strcmp(protos[i].name, name) == 0)
                break;
        }
        if(i < proto_count)
            continue;
        if(proto_count >= PLAN9_MAX_PROTOS)
            break;
        snprintf(protos[proto_count].name, sizeof(protos[0].name), "%s", name);
        snprintf(protos[proto_count].type, sizeof(protos[0].type), "%s", type);
        proto_count++;
    }
    fclose(f);
}

static void
proto_scan_dir(const char *path, int depth)
{
    DIR *dir;
    struct dirent *ent;

    if(depth > 4)
        return;
    dir = opendir(path);
    if(dir == NULL)
        return;
    while((ent = readdir(dir)) != NULL) {
        char child[2048];
        const char *dot;

        if(ent->d_name[0] == '.')
            continue;
        if(snprintf(child, sizeof(child), "%s/%s", path, ent->d_name)
           >= (int)sizeof(child))
            continue;
        dot = strrchr(ent->d_name, '.');
        if(dot != NULL && strcmp(dot, ".h") == 0)
            proto_add_file(child, 0);
        else if(dot != NULL && strcmp(dot, ".c") == 0)
            proto_add_file(child, 1);
        else if(dot == NULL)
            proto_scan_dir(child, depth + 1);
    }
    closedir(dir);
}

static const char *
proto_return_type(const char *name)
{
    static const struct {
        const char *name;
        const char *type;
    } builtins[] = {
        {"localtime", "struct tm *"},
        {"gmtime", "struct tm *"},
        {"clock", "long"},
        {"time", "long"},
        {"malloc", "void *"},
        {"calloc", "void *"},
        {"realloc", "void *"},
    };
    int i;

    for(i = 0; (size_t)i < sizeof(builtins) / sizeof(builtins[0]); i++) {
        if(strcmp(builtins[i].name, name) == 0)
            return builtins[i].type;
    }
    if(!protos_loaded) {
        int d;
        for(d = 0; d < include_dir_count; d++)
            proto_scan_dir(include_dirs[d], 0);
        protos_loaded = 1;
    }
    for(i = 0; i < proto_count; i++) {
        if(strcmp(protos[i].name, name) == 0)
            return protos[i].type;
    }
    return NULL;
}

void
k2c_plan9_add_include_dir(const char *dir)
{
    if(include_dir_count >= PLAN9_MAX_INCLUDE_DIRS)
        return;
    snprintf(include_dirs[include_dir_count],
             sizeof(include_dirs[0]), "%s", dir);
    include_dir_count++;
}

int
k2c_plan9_unresolved(void)
{
    return unresolved_count;
}

/* ------------------------------------------------------------------ */
/* Text helpers                                                      */
/* ------------------------------------------------------------------ */

static int
line_passthrough(const char *line)
{
    const char *p = line;

    while(*p == ' ' || *p == '\t')
        p++;
    return *p == '\0' || *p == '\n' || *p == '#';
}

static int
brace_delta(const char *line)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    const char *p;

    for(p = line; *p != '\0'; p++) {
        char ch = *p;
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '{')
            depth++;
        else if(ch == '}')
            depth--;
    }
    return depth;
}

static int
find_matching_brace(const char *text, int open_index)
{
    int depth = 0;
    int in_string = 0;
    int escape = 0;
    int i;

    for(i = open_index; text[i] != '\0'; i++) {
        char ch = text[i];
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '{')
            depth++;
        else if(ch == '}') {
            depth--;
            if(depth == 0)
                return i;
        }
    }
    return -1;
}

static int
find_top_level_comma(const char *text, int start, int end)
{
    int paren = 0, brace = 0, bracket = 0;
    int in_string = 0;
    int escape = 0;
    int i;

    for(i = start; i < end; i++) {
        char ch = text[i];
        if(in_string) {
            if(escape)
                escape = 0;
            else if(ch == '\\')
                escape = 1;
            else if(ch == '"')
                in_string = 0;
            continue;
        }
        if(ch == '"')
            in_string = 1;
        else if(ch == '(')
            paren++;
        else if(ch == ')')
            paren--;
        else if(ch == '{')
            brace++;
        else if(ch == '}')
            brace--;
        else if(ch == '[')
            bracket++;
        else if(ch == ']')
            bracket--;
        else if(ch == ',' && paren == 0 && brace == 0 && bracket == 0)
            return i;
    }
    return -1;
}

static int
copy_trimmed(const char *text, int start, int end, char *out, size_t out_size)
{
    size_t n;

    while(start < end && isspace((unsigned char)text[start]))
        start++;
    while(end > start && isspace((unsigned char)text[end - 1]))
        end--;
    n = (size_t)(end - start);
    if(n >= out_size)
        return -1;
    memcpy(out, text + start, n);
    out[n] = '\0';
    return 0;
}

/* ------------------------------------------------------------------ */
/* Compound literal flattening                                       */
/* ------------------------------------------------------------------ */

/* Types the lowering nests as braced values inside designated fields. */
static const char *
nested_field_type(const char *field)
{
    if(strcmp(field, "bounds") == 0 || strcmp(field, "rect") == 0)
        return "Rectangle";
    if(strcmp(field, "color") == 0 || strcmp(field, "tint") == 0)
        return "Color";
    return NULL;
}

static int
body_all_designated(const char *body, int len)
{
    int pos = 0;

    while(pos < len) {
        int comma = find_top_level_comma(body, pos, len);
        int stop = comma < 0 ? len : comma;
        int i = pos;

        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '.')
            return 0;
        if(comma < 0)
            break;
        pos = comma + 1;
    }
    return 1;
}

static int
body_is_zero(const char *body, int len)
{
    int i;
    int seen = 0;

    for(i = 0; i < len; i++) {
        if(isspace((unsigned char)body[i]))
            continue;
        if(body[i] != '0')
            return 0;
        if(seen)
            return 0;
        seen = 1;
    }
    return seen;
}

/* Emit statements that build `Type temp` from the literal body and
 * append them to out. Returns 0 on success. */
static int
emit_compound_temp(Buf *out, const char *indent, const char *type,
                   const char *body, int body_len, const char *temp)
{
    char value[PLAN9_LINE_MAX];
    char field[PLAN9_NAME_MAX];
    int pos = 0;
    const char *bracket = strchr(type, '[');

    if(bracket != NULL) {
        /* array literal: declare the temporary with its size */
        char base[PLAN9_TYPE_MAX];
        size_t n = (size_t)(bracket - type);

        if(n == 0 || n >= sizeof(base))
            return -1;
        memcpy(base, type, n);
        base[n] = '\0';
        if(buf_printf(out, "%s%s %s%s;\n", indent, base, temp, bracket) < 0)
            return -1;
        return buf_printf(out, "%smemset(%s, 0, sizeof(%s));\n",
                          indent, temp, temp);
    }

    if(body_is_zero(body, body_len)) {
        if(buf_printf(out, "%s%s %s;\n", indent, type, temp) < 0)
            return -1;
        return buf_printf(out, "%smemset(&%s, 0, sizeof(%s));\n",
                          indent, temp, temp);
    }

    if(!body_all_designated(body, body_len)) {
        /* full positional initializer in a declaration is 8c-safe */
        if(copy_trimmed(body, 0, body_len, value, sizeof(value)) < 0)
            return -1;
        return buf_printf(out, "%s%s %s = {%s};\n", indent, type, temp, value);
    }

    if(buf_printf(out, "%s%s %s;\n", indent, type, temp) < 0)
        return -1;
    if(buf_printf(out, "%smemset(&%s, 0, sizeof(%s));\n", indent, temp, temp) < 0)
        return -1;

    while(pos < body_len) {
        int comma = find_top_level_comma(body, pos, body_len);
        int stop = comma < 0 ? body_len : comma;
        const char *nested;
        int i = pos;
        int fstart;
        int fend;

        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '.')
            return -1;
        i++;
        fstart = i;
        while(i < stop && (isalnum((unsigned char)body[i]) || body[i] == '_'))
            i++;
        fend = i;
        if(fend == fstart || (size_t)(fend - fstart) >= sizeof(field))
            return -1;
        memcpy(field, body + fstart, (size_t)(fend - fstart));
        field[fend - fstart] = '\0';
        while(i < stop && isspace((unsigned char)body[i]))
            i++;
        if(i >= stop || body[i] != '=')
            return -1;
        i++;
        if(copy_trimmed(body, i, stop, value, sizeof(value)) < 0)
            return -1;

        nested = nested_field_type(field);
        if(nested != NULL && value[0] == '{' && value[strlen(value) - 1] == '}') {
            char inner[PLAN9_LINE_MAX];
            size_t n = strlen(value) - 2;

            if(n >= sizeof(inner))
                return -1;
            memcpy(inner, value + 1, n);
            inner[n] = '\0';
            if(buf_printf(out, "%s%s %s_%s = {%s};\n%s%s.%s = %s_%s;\n",
                          indent, nested, temp, field, inner,
                          indent, temp, field, temp, field) < 0)
                return -1;
        } else {
            if(buf_printf(out, "%s%s.%s = %s;\n",
                          indent, temp, field, value) < 0)
                return -1;
        }
        if(comma < 0)
            break;
        pos = comma + 1;
    }
    return 0;
}

/* Find the next cast-shaped compound literal "(Ident]){{" on the line,
 * starting at *cursor. Sets the cast span and brace span. */
static int
find_cast_literal(const char *line, int cursor, int *cast_start, int *cast_end,
                  int *brace_open, char *type, size_t type_size)
{
    int i = cursor;
    int tstart;
    int tend;
    size_t n;
    int k;

    while(line[i] != '\0') {
        if(line[i] == '"' || line[i] == '\'') {
            char quote = line[i];
            i++;
            while(line[i] != '\0' && line[i] != quote) {
                if(line[i] == '\\')
                    i++;
                i++;
            }
            if(line[i] != '\0')
                i++;
            continue;
        }
        if(line[i] == '(') {
            tstart = i + 1;
            tend = tstart;
            while(line[tend] != '\0'
                  && (isalnum((unsigned char)line[tend]) || line[tend] == '_'
                      || line[tend] == ' ' || line[tend] == '\t'
                      || line[tend] == '*' || line[tend] == '['
                      || line[tend] == ']'))
                tend++;
            n = (size_t)(tend - tstart);
            if(n > 0 && n < type_size && line[tend] == ')'
               && line[tend + 1] == '{'
               && (isalpha((unsigned char)line[tstart])
                   || line[tstart] == '_')) {
                int ok = 1;
                for(k = 0; k < (int)n; k++) {
                    char ch = line[tstart + k];
                    if(!(isalnum((unsigned char)ch) || ch == '_' || ch == ' '
                         || ch == '\t' || ch == '*')) {
                        ok = 0;
                        break;
                    }
                }
                if(ok) {
                    memcpy(type, line + tstart, n);
                    type[n] = '\0';
                    *cast_start = i;
                    *cast_end = tend;
                    *brace_open = tend + 1;
                    return 1;
                }
            }
        }
        i++;
    }
    return 0;
}

/* Rewrite every cast literal on one line into temps + rebuilt line. */
static int
rewrite_line_compound(Buf *out, const char *line, int lineno, int *rewrote)
{
    char indent[256];
    size_t ilen = 0;
    int cursor = 0;
    int temp_index = 0;
    Buf temps;
    Buf rebuilt;
    char type[PLAN9_TYPE_MAX];
    int cast_start;
    int cast_end;
    int brace_open;
    int brace_close;
    char body[PLAN9_LINE_MAX];
    char temp[PLAN9_NAME_MAX];
    int n;

    while(ilen + 1 < sizeof(indent) &&
          (line[ilen] == ' ' || line[ilen] == '\t')) {
        indent[ilen] = line[ilen];
        ilen++;
    }
    indent[ilen] = '\0';

    if(!buf_init(&temps) || !buf_init(&rebuilt)) {
        free(temps.data);
        free(rebuilt.data);
        return -1;
    }

    while(find_cast_literal(line, cursor, &cast_start, &cast_end,
                            &brace_open, type, sizeof(type))) {
        brace_close = find_matching_brace(line, brace_open);
        if(brace_close < 0)
            break; /* multi-line literal: leave the rest untouched */
        n = brace_close - brace_open - 1;
        if((size_t)n >= sizeof(body))
            break;
        memcpy(body, line + brace_open + 1, (size_t)n);
        body[n] = '\0';

        snprintf(temp, sizeof(temp), "%s%d_%d", PLAN9_TEMP_PREFIX,
                 lineno, temp_index);
        temp_index++;

        if(emit_compound_temp(&temps, indent, type, body, n, temp) < 0)
            break;
        if(buf_append(&rebuilt, line + cursor,
                      (size_t)(cast_start - cursor)) < 0)
            break;
        if(buf_puts(&rebuilt, temp) < 0)
            break;
        cursor = brace_close + 1;
        (*rewrote)++;
    }
    if(*rewrote > 0) {
        int failed = buf_append(&rebuilt, line + cursor,
                                strlen(line + cursor)) < 0
                     || buf_puts(out, temps.data) < 0
                     || buf_puts(out, rebuilt.data) < 0;
        free(temps.data);
        free(rebuilt.data);
        return failed ? -1 : 0;
    }
    free(temps.data);
    free(rebuilt.data);
    return 0;
}

/* ------------------------------------------------------------------ */
/* __auto_type resolution                                            */
/* ------------------------------------------------------------------ */


/* Classify an initializer expression by resolving every identifier and
 * call head in it: uniform int stays int, any float makes float, a
 * single non-int resolved type wins, mixed types fail. */
static int
expr_type(const char *expr, size_t len, char *type, size_t type_size);

static int
head_type(const char *head, size_t len, char *type, size_t type_size)
{
    char name[PLAN9_NAME_MAX];
    const char *ret;

    if(len == 0 || len >= sizeof(name))
        return 0;
    memcpy(name, head, len);
    name[len] = '\0';
    if(init_is_int_literal(name)) {
        snprintf(type, type_size, "int");
        return 1;
    }
    ret = proto_return_type(name);
    if(ret == NULL || strlen(ret) >= type_size)
        return 0;
    snprintf(type, type_size, "%s", ret);
    return 1;
}

static int
merge_type(char *acc, int *have, const char *next)
{
    if(!*have) {
        snprintf(acc, PLAN9_TYPE_MAX, "%s", next);
        *have = 1;
        return 1;
    }
    if(strcmp(acc, next) == 0)
        return 1;
    if((strcmp(acc, "int") == 0 && strcmp(next, "float") == 0)
       || (strcmp(acc, "float") == 0 && strcmp(next, "int") == 0)) {
        snprintf(acc, PLAN9_TYPE_MAX, "float");
        return 1;
    }
    return 0;
}

static int
expr_type(const char *expr, size_t len, char *type, size_t type_size)
{
    char acc[PLAN9_TYPE_MAX];
    char one[PLAN9_TYPE_MAX];
    int have = 0;
    size_t i = 0;

    while(i < len && (expr[i] == ' ' || expr[i] == '\t'))
        i++;
    if(i >= len)
        return 0;
    if(expr[i] == '&' || expr[i] == '*' || expr[i] == '(')
        return 0; /* pointers and casts handled by the caller */

    while(i < len) {
        if(isdigit((unsigned char)expr[i])) {
            size_t j = i;

            while(j < len && (isdigit((unsigned char)expr[j])
                              || expr[j] == '.' || expr[j] == 'u'
                              || expr[j] == 'U' || expr[j] == 'l'
                              || expr[j] == 'L' || expr[j] == 'f'
                              || expr[j] == 'F'))
                j++;
            if(!head_type(expr + i, j - i, one, sizeof(one)))
                return 0;
            if(!merge_type(acc, &have, one))
                return 0;
            i = j;
            continue;
        }
        if(isalpha((unsigned char)expr[i]) || expr[i] == '_') {
            size_t j = i;

            while(j < len && (isalnum((unsigned char)expr[j])
                              || expr[j] == '_'))
                j++;
            if(j < len && expr[j] == '(') {
                size_t depth = 0;
                size_t k = j;

                while(k < len) {
                    if(expr[k] == '(')
                        depth++;
                    else if(expr[k] == ')') {
                        depth--;
                        if(depth == 0)
                            break;
                    }
                    k++;
                }
                if(k >= len)
                    return 0;
                if(!head_type(expr + i, j - i, one, sizeof(one)))
                    return 0;
                if(!merge_type(acc, &have, one))
                    return 0;
                i = k + 1;
                continue;
            }
            /* the base identifier: a macro constant or an int local */
            if(!head_type(expr + i, j - i, one, sizeof(one)))
                snprintf(one, sizeof(one), "int");
            if(!merge_type(acc, &have, one))
                return 0;

            i = j;
            continue;
        }
        i++;
    }
    if(!have)
        return 0;
    if(strlen(acc) >= type_size)
        return 0;
    snprintf(type, type_size, "%s", acc);
    return 1;
}

static int
autotype_type_for_init(const char *init, char *type, size_t type_size)
{
    size_t i = 0;
    size_t tstart;
    size_t tend;
    size_t n;
    const char *call;
    const char *call_end;
    char name[PLAN9_NAME_MAX];
    const char *ret;

    while(init[i] == ' ' || init[i] == '\t')
        i++;

    /* single dereference: layout pointers are int in the lowered UI
     * code; anything more complex stays unresolved */
    if(init[i] == '*') {
        const char *s = init + i + 1;
        size_t k = 0;

        while(s[k] == ' ')
            k++;
        while(s[k] != '\0' && (isalnum((unsigned char)s[k]) || s[k] == '_'))
            k++;
        while(s[k] == ' ')
            k++;
        if(s[k] == ';' || s[k] == '+' || s[k] == '-' || s[k] == ')') {
            if(type_size < 4)
                return 0;
            snprintf(type, type_size, "int");
            return 1;
        }
        return 0;
    }

    /* cast expression, compound literal or not: the type is spelled */
    if(init[i] == '(') {
        tstart = i + 1;
        tend = tstart;
        while(init[tend] != '\0' && (isalnum((unsigned char)init[tend])
                                     || init[tend] == '_' || init[tend] == ' '
                                     || init[tend] == '*'
                                     || init[tend] == '['
                                     || init[tend] == ']'))
            tend++;
        n = tend - tstart;
        if(n > 0 && init[tend] == ')' && n < type_size
           && (isalpha((unsigned char)init[tstart])
               || init[tstart] == '_')) {
            size_t k;
            int ok = 1;

            for(k = 0; k < n; k++) {
                char ch = init[tstart + k];
                if(!(isalnum((unsigned char)ch) || ch == '_' || ch == ' '
                     || ch == '\t' || ch == '*' || ch == '['
                     || ch == ']')) {
                    ok = 0;
                    break;
                }
            }
            if(ok) {
                memcpy(type, init + tstart, n);
                type[n] = '\0';
                return 1;
            }
        }
    }

    /* string literal */
    if(init[i] == '"') {
        if(type_size < 12)
            return 0;
        snprintf(type, type_size, "const char *");
        return 1;
    }

    /* numeric literal or constant expression over numbers and macros */
    {
        char head[256];
        size_t len = strlen(init + i);
        const char *semi = strchr(init + i, ';');

        if(semi != NULL)
            len = (size_t)(semi - (init + i));
        if(len == 0 || len >= sizeof(head))
            len = sizeof(head) - 1;
        memcpy(head, init + i, len);
        head[len] = '\0';
        if(init_is_int_literal(head)) {
            if(type_size < 4)
                return 0;
            snprintf(type, type_size, "int");
            return 1;
        }
        if(strchr(head, '&') == NULL && head[0] != '*') {
            /* expression over literals, macros, locals, and calls */
            if(expr_type(head, strlen(head), type, type_size))
                return 1;
        }
        (void)name;
        (void)ret;
    }

    /* function call: resolve the return type from the prototype map */
    call = init + i;
    call_end = call;
    while(*call_end != '\0' && (isalnum((unsigned char)*call_end)
                                || *call_end == '_'))
        call_end++;
    n = (size_t)(call_end - call);
    if(n == 0 || *call_end != '(' || n >= sizeof(name))
        return 0;
    memcpy(name, call, n);
    name[n] = '\0';
    ret = proto_return_type(name);
    if(ret == NULL || strlen(ret) >= type_size)
        return 0;
    snprintf(type, type_size, "%s", ret);
    return 1;
}

static int
rewrite_autotype(const char *line, char *out, size_t out_size)
{
    const char *p = line;
    const char *name_start;
    const char *name_end;
    const char *init;
    char name[PLAN9_NAME_MAX];
    char type[PLAN9_TYPE_MAX];
    size_t n;

    while(*p == ' ' || *p == '\t')
        p++;
    if(strncmp(p, "__auto_type ", 12) != 0)
        return 0;
    p += 12;
    name_start = p;
    while(*p != '\0' && (isalnum((unsigned char)*p) || *p == '_'))
        p++;
    name_end = p;
    n = (size_t)(name_end - name_start);
    if(n == 0 || n >= sizeof(name))
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';
    while(*p == ' ')
        p++;
    if(*p != '=')
        return 0;
    init = p + 1;
    if(!autotype_type_for_init(init, type, sizeof(type)))
        return 0;
    if(snprintf(out, out_size, "%.*s%s %s = %s",
                (int)(name_start - line - 12), line, type, name, init)
       >= (int)out_size)
        return 0;
    return 1;
}

/* ------------------------------------------------------------------ */
/* for-declaration hoisting                                          */
/* ------------------------------------------------------------------ */

static int
for_decl_split(const char *line, char *type, size_t type_size,
               char *name, size_t name_size, const char **rest)
{
    const char *p = line;
    const char *q;
    const char *type_start;
    const char *type_end;
    const char *name_start;
    const char *name_end;
    size_t n;

    while(*p == ' ' || *p == '\t')
        p++;
    if(strncmp(p, "for(", 4) != 0)
        return 0;
    q = p + 4;
    type_start = q;
    if(strncmp(q, "unsigned ", 9) == 0)
        q += 9;
    if(strncmp(q, "int ", 4) != 0 && strncmp(q, "long ", 5) != 0
       && strncmp(q, "size_t ", 7) != 0)
        return 0;
    while(*q != '\0' && *q != ' ' && *q != '\t')
        q++;
    type_end = q;
    n = (size_t)(type_end - type_start);
    if(n == 0 || n >= type_size)
        return 0;
    memcpy(type, type_start, n);
    type[n] = '\0';
    while(*q == ' ' || *q == '\t')
        q++;
    name_start = q;
    while(*q != '\0' && (isalnum((unsigned char)*q) || *q == '_'))
        q++;
    name_end = q;
    n = (size_t)(name_end - name_start);
    if(n == 0 || n >= name_size)
        return 0;
    memcpy(name, name_start, n);
    name[n] = '\0';
    while(*q == ' ' || *q == '\t')
        q++;
    if(*q != '=')
        return 0;
    /* rest keeps " = init; cond; step) {" */
    *rest = name_end;
    return 1;
}

/* ------------------------------------------------------------------ */
/* Public entry points                                               */
/* ------------------------------------------------------------------ */

int
k2c_plan9_rewrite_file(const char *path, int is_project)
{
    FILE *f;
    char *text;
    long size;
    char *rewritten;
    const char *write_text;

    if(!plan9_enabled)
        return 0;
    f = fopen(path, "rb");
    if(f == NULL)
        return -1;
    fseek(f, 0, SEEK_END);
    size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size < 0) {
        fclose(f);
        return -1;
    }
    text = malloc((size_t)size + 1);
    if(text == NULL) {
        fclose(f);
        return -1;
    }
    if(fread(text, 1, (size_t)size, f) != (size_t)size) {
        free(text);
        fclose(f);
        return -1;
    }
    text[size] = '\0';
    fclose(f);

    write_text = text;
    rewritten = NULL;
    if(is_project) {
        char *unguarded = k2c_plan9_rewrite_project(text);

        if(unguarded != NULL) {
            char *generic = k2c_plan9_rewrite(unguarded);

            free(unguarded);
            if(generic == NULL) {
                free(text);
                return -1;
            }
            rewritten = generic;
            write_text = generic;
        }
    } else {
        char *generic = k2c_plan9_rewrite(text);

        if(generic == NULL) {
            free(text);
            return -1;
        }
        rewritten = generic;
        write_text = generic;
    }

    if(write_text != text) {
        f = fopen(path, "wb");
        if(f == NULL) {
            free(rewritten);
            free(text);
            return -1;
        }
        fwrite(write_text, 1, strlen(write_text), f);
        fclose(f);
    }
    free(rewritten);
    free(text);
    return 0;
}

char *
k2c_plan9_rewrite_project(const char *text)
{
    static const char guarded[] =
        "#if defined(__GNUC__) || defined(__clang__)\n"
        "void *CreateApp(const char *project_path) __attribute__((weak));\n"
        "void DestroyApp(void *app) __attribute__((weak));\n"
        "void ApplyRoute(void *app, const AppRouteInfo *route) "
        "__attribute__((weak));\n"
        "void BeginScreenDraw(void *app, Rectangle viewport) "
        "__attribute__((weak));\n"
        "#endif\n";
    static const char exposed[] =
        "void *CreateApp(const char *project_path) __attribute__((weak));\n"
        "void DestroyApp(void *app) __attribute__((weak));\n"
        "void ApplyRoute(void *app, const AppRouteInfo *route) "
        "__attribute__((weak));\n"
        "void BeginScreenDraw(void *app, Rectangle viewport) "
        "__attribute__((weak));\n";
    const char *hit;
    size_t len = strlen(text);
    size_t guarded_len = strlen(guarded);
    size_t exposed_len = strlen(exposed);
    char *out;

    hit = strstr(text, guarded);
    if(hit == NULL)
        return NULL;
    out = malloc(len - guarded_len + exposed_len + 1);
    if(out == NULL)
        return NULL;
    memcpy(out, text, (size_t)(hit - text));
    memcpy(out + (size_t)(hit - text), exposed, exposed_len);
    memcpy(out + (size_t)(hit - text) + exposed_len, hit + guarded_len,
           len - (size_t)(hit - text) - guarded_len);
    out[len - guarded_len + exposed_len] = '\0';
    return out;
}

char *
k2c_plan9_rewrite(const char *text)
{
    Buf out;
    char *line;
    const char *cursor = text;
    int lineno = 0;
    int pending_depths[256];
    int pending_count = 0;
    int depth = 0;

    line = malloc(PLAN9_LINE_MAX);
    if(line == NULL)
        return NULL;
    if(!buf_init(&out)) {
        free(line);
        return NULL;
    }

    while(*cursor != '\0') {
        const char *nl = strchr(cursor, '\n');
        size_t n = nl != NULL ? (size_t)(nl - cursor) + 1 : strlen(cursor);
        char current[PLAN9_LINE_MAX];
        char rewritten[PLAN9_LINE_MAX];
        char indent[256];
        size_t ilen = 0;
        char ftype[64];
        char fname[PLAN9_NAME_MAX];
        const char *rest;
        int emitted = 0;

        if(n >= PLAN9_LINE_MAX)
            n = PLAN9_LINE_MAX - 1;
        memcpy(current, cursor, n);
        current[n] = '\0';
        cursor += n;
        lineno++;

        while(ilen + 1 < sizeof(indent) &&
              (current[ilen] == ' ' || current[ilen] == '\t')) {
            indent[ilen] = current[ilen];
            ilen++;
        }
        indent[ilen] = '\0';

        /* close for-decl blocks that ended at this depth */
        while(pending_count > 0 && depth == pending_depths[pending_count - 1]) {
            pending_count--;
            if(buf_printf(&out, "%s}\n", indent) < 0)
                goto fail;
        }

        if(line_passthrough(current)) {
            if(buf_puts(&out, current) < 0)
                goto fail;
            continue;
        }

        /* 1. __auto_type: resolve to a concrete type where possible */
        if(strstr(current, "__auto_type") != NULL) {
            if(rewrite_autotype(current, rewritten, sizeof(rewritten))) {
                snprintf(current, sizeof(current), "%s", rewritten);
            } else {
                unresolved_count++;
            }
        }

        /* 2. for-declaration hoisting */
        if(for_decl_split(current, ftype, sizeof(ftype), fname,
                          sizeof(fname), &rest)) {
            if(buf_printf(&out, "%s{\n", indent) < 0)
                goto fail;
            if(buf_printf(&out, "%s    %s %s;\n", indent, ftype, fname) < 0)
                goto fail;
            if(buf_printf(&out, "%s    for(%s%s", indent, fname, rest) < 0)
                goto fail;
            if(pending_count < 256) {
                pending_depths[pending_count] = depth;
                pending_count++;
            }
            depth += brace_delta(current);
            emitted = 1; /* statement fully emitted above */
        }

        /* 3. compound literals in the statement */
        if(!emitted) {
            int rewrote = 0;
            if(strstr(current, "){") != NULL
               && strchr(current, '(') != NULL) {
                if(rewrite_line_compound(&out, current, lineno, &rewrote) < 0)
                    goto fail;
            }
            if(rewrote == 0) {
                if(buf_puts(&out, current) < 0)
                    goto fail;
            }
            depth += brace_delta(current);
        }

        while(pending_count > 0 && depth == pending_depths[pending_count - 1]) {
            pending_count--;
            if(buf_printf(&out, "%s}\n", indent) < 0)
                goto fail;
        }
    }
    free(line);

    /* the pass emits memset() calls; make sure the declaration is
     * included even when the lowered source never needed <string.h> */
    if(strstr(out.data, "memset(") != NULL
       && strstr(text, "#include <string.h>") == NULL) {
        const char *scan = out.data;
        const char *last_include = NULL;
        int guard = 0;

        while((scan = strstr(scan, "#include ")) != NULL) {
            const char *eol = strchr(scan, '\n');

            if(eol != NULL) {
                last_include = eol + 1;
                scan = eol + 1;
            } else {
                break;
            }
            if(++guard > 64)
                break;
        }
        if(last_include != NULL) {
            size_t at = (size_t)(last_include - out.data);
            const char *inc = "#include <string.h>\n";
            size_t inc_len = strlen(inc);
            char *grown = realloc(out.data, out.len + inc_len + 1);

            if(grown != NULL) {
                out.data = grown;
                memmove(out.data + at + inc_len, out.data + at,
                        out.len - at + 1);
                memcpy(out.data + at, inc, inc_len);
                out.len += inc_len;
                out.cap = out.len + 1;
            }
        }
    }
    return out.data;

fail:
    free(line);
    free(out.data);
    return NULL;
}

#include <ctype.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

enum {
    KC_PATH_MAX = 1024,
    KC_TEXT_MAX = 1024 * 1024,
    KC_NAME_MAX = 128,
    KC_INCLUDE_MAX = 64,
    KC_FUNCTION_MAX = 32,
    KC_CALL_MAX = 64,
    KC_BODY_MAX = 512,
    KC_BODY_LINE_MAX = 1024,
};

typedef struct KryFunction {
    char screen[KC_NAME_MAX];
    char args[512];
    char return_type[KC_NAME_MAX];
    int exact_name;
    int is_public;
    char calls[KC_CALL_MAX][512];
    char body[KC_BODY_MAX][KC_BODY_LINE_MAX];
    int body_line[KC_BODY_MAX];
    int call_count;
    int body_count;
} KryFunction;

typedef struct KryFile {
    char *path;
    char *text;
    char includes[KC_INCLUDE_MAX][KC_PATH_MAX];
    KryFunction functions[KC_FUNCTION_MAX];
    int include_count;
    int function_count;
    int current_line;
    KryFunction *current;
} KryFile;

static void die(const char *fmt, ...);
static void strip_kry_ext(char *dst, size_t dst_size, const char *path);

static void
add_body_line(KryFile *file, int source_line, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    if(fn->body_count >= KC_BODY_MAX)
        die("%s: generated body is too large", file->path);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count],
              sizeof(fn->body[fn->body_count]), fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = source_line;
    fn->body_count++;
}

static void
add_body(KryFile *file, const char *fmt, ...)
{
    KryFunction *fn;
    va_list args;

    fn = file->current;
    if(fn == NULL)
        die("%s: statement outside function", file->path);
    if(fn->body_count >= KC_BODY_MAX)
        die("%s: generated body is too large", file->path);
    va_start(args, fmt);
    vsnprintf(fn->body[fn->body_count],
              sizeof(fn->body[fn->body_count]), fmt, args);
    va_end(args);
    fn->body_line[fn->body_count] = file->current_line;
    fn->body_count++;
}

static void
die(const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
    exit(1);
}

static void
warn_at(const char *path, int line, const char *fmt, ...)
{
    va_list args;

    fprintf(stderr, "%s:%d: ", path, line);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fputc('\n', stderr);
}

static char *
trim(char *s)
{
    char *end;

    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    end = s + strlen(s);
    while(end > s &&
          (end[-1] == ' ' || end[-1] == '\t' ||
           end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static int
starts_word(const char *s, const char *word)
{
    size_t n = strlen(word);

    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || isspace((unsigned char)s[n]));
}

static int
parse_ident(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(!((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') || *s == '_'))
        return 0;
    while((*s >= 'A' && *s <= 'Z') || (*s >= 'a' && *s <= 'z') ||
          (*s >= '0' && *s <= '9') || *s == '_') {
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    dst[n] = '\0';
    *sp = s;
    return 1;
}

static int
parse_quoted(char **sp, char *dst, size_t dst_size)
{
    char *s = *sp;
    size_t n = 0;

    while(*s == ' ' || *s == '\t')
        s++;
    if(*s != '"')
        return 0;
    s++;
    while(*s != '\0' && *s != '"') {
        if(*s == '\\' && s[1] != '\0') {
            if(n + 1 < dst_size)
                dst[n++] = *s;
            s++;
        }
        if(n + 1 < dst_size)
            dst[n++] = *s;
        s++;
    }
    if(*s != '"')
        return 0;
    dst[n] = '\0';
    *sp = s + 1;
    return 1;
}

static void
c_string_literal(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '"';
    for(const char *p = src != NULL ? src : ""; *p != '\0' && n + 2 < dst_size; p++) {
        if(*p == '"' || *p == '\\') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = *p;
        } else if(*p == '\n') {
            if(n + 2 >= dst_size)
                break;
            dst[n++] = '\\';
            dst[n++] = 'n';
        } else {
            dst[n++] = *p;
        }
    }
    if(n + 1 < dst_size)
        dst[n++] = '"';
    dst[n] = '\0';
}

static void
emit_source_push(KryFile *file, int line_no)
{
    char path[KC_PATH_MAX + 8];

    c_string_literal(path, sizeof(path), file->path);
    add_body_line(file, 0, "    PushUIInspectSource(%s, %d);", path, line_no);
}

static void
emit_source_pop(KryFile *file)
{
    add_body_line(file, 0, "    PopUIInspectSource();");
}

static const char *
native_widget_name(const char *name)
{
    if(strcmp(name, "CenteredColumn") == 0)
        return "GetUICenteredColumn";
    if(strcmp(name, "ScrollEnd") == 0)
        return "EndUIScrollPage";
    if(strcmp(name, "Text") == 0)
        return "DrawUIText";
    if(strcmp(name, "TextButton") == 0)
        return "DrawUITextButton";
    return name;
}

static void
emit_call(KryFile *file, const char *prefix, const char *expr,
          const char *suffix)
{
    char name[KC_NAME_MAX];
    const char *open = strchr(expr, '(');
    size_t n;

    if(open == NULL) {
        add_body(file, "%s%s%s", prefix, expr, suffix);
        return;
    }
    n = (size_t)(open - expr);
    if(n >= sizeof(name))
        n = sizeof(name) - 1;
    memcpy(name, expr, n);
    name[n] = '\0';
    add_body(file, "%s%s%s%s", prefix, native_widget_name(name), open, suffix);
}

static void
convert_var_decl(char *dst, size_t dst_size, const char *name,
                 const char *type)
{
    char tmp[512];
    char *t;

    snprintf(tmp, sizeof(tmp), "%s", type != NULL ? type : "");
    t = trim(tmp);
    if(t[0] == '[') {
        char *end = strchr(t, ']');

        if(end != NULL) {
            char *size;
            char *element;

            *end = '\0';
            size = trim(t + 1);
            element = trim(end + 1);
            snprintf(dst, dst_size, "%s %s[%s]", element, name, size);
            return;
        }
    }
    if(t[0] == '*') {
        snprintf(dst, dst_size, "%s *%s", trim(t + 1), name);
        return;
    }
    snprintf(dst, dst_size, "%s %s", t, name);
}

static void
convert_arg_list(char *dst, size_t dst_size, const char *src)
{
    char tmp[512];
    char out[512] = "";
    char *part;
    char *save = NULL;

    snprintf(tmp, sizeof(tmp), "%s", src);
    for(part = strtok_r(tmp, ",", &save); part != NULL;
        part = strtok_r(NULL, ",", &save)) {
        char *name;
        char *type;
        char *colon;
        char item[256];

        part = trim(part);
        colon = strchr(part, ':');
        if(colon == NULL) {
            snprintf(item, sizeof(item), "%s", part);
        } else {
            *colon = '\0';
            name = trim(part);
            type = trim(colon + 1);
            convert_var_decl(item, sizeof(item), name, type);
        }
        if(out[0] != '\0')
            strncat(out, ", ", sizeof(out) - strlen(out) - 1);
        strncat(out, item, sizeof(out) - strlen(out) - 1);
    }
    snprintf(dst, dst_size, "%s", out);
}

static int
line_is_close(const char *line)
{
    return strcmp(line, "}") == 0;
}

static int
line_is_else(const char *line)
{
    return strcmp(line, "else {") == 0 || strcmp(line, "} else {") == 0;
}

static int
starts_else_if(const char *line)
{
    return starts_word(line, "else if") || starts_word(line, "} else if");
}

static int
line_is_assignment_statement(const char *line)
{
    const char *p;

    if(line == NULL || line[0] == '\0')
        return 0;
    if(strstr(line, "+=") != NULL || strstr(line, "-=") != NULL ||
       strstr(line, "*=") != NULL || strstr(line, "/=") != NULL ||
       strstr(line, "++") != NULL || strstr(line, "--") != NULL)
        return 1;
    for(p = line; *p != '\0'; p++) {
        if(*p != '=')
            continue;
        if(p > line && (p[-1] == '=' || p[-1] == '<' || p[-1] == '>' ||
                        p[-1] == '!'))
            continue;
        if(p[1] == '=')
            continue;
        return 1;
    }
    return 0;
}

static void
parse_statement(KryFile *file, int line_no, char *line)
{
    if(line_is_close(line)) {
        add_body(file, "    }");
    } else if(starts_else_if(line)) {
        char *q = starts_word(line, "else if")
                      ? trim(line + strlen("else if"))
                      : trim(line + strlen("} else if"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected else if condition ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected else if condition", file->path, line_no);
        add_body(file, "    } else if(%s) {", q);
    } else if(line_is_else(line)) {
        add_body(file, "    } else {");
    } else if(starts_word(line, "guard")) {
        char *q = trim(line + strlen("guard"));

        if(q[0] == '\0')
            die("%s:%d: expected guard condition", file->path, line_no);
        add_body(file, "    if(%s)", q);
        add_body(file, "        return;");
    } else if(starts_word(line, "return")) {
        char *q = trim(line + strlen("return"));

        if(q[0] == '\0')
            add_body(file, "    return;");
        else
            add_body(file, "    return %s;", q);
    } else if(starts_word(line, "var")) {
        char *q = trim(line + strlen("var"));
        char name[KC_NAME_MAX];
        char decl[512];
        char *type;
        char *eq;
        char *expr = NULL;

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected variable name", file->path, line_no);
        q = trim(q);
        if(*q != ':')
            die("%s:%d: expected ':' in variable declaration", file->path,
                line_no);
        type = trim(q + 1);
        eq = strchr(type, '=');
        if(eq != NULL) {
            *eq = '\0';
            expr = trim(eq + 1);
        }
        type = trim(type);
        if(type[0] == '\0')
            die("%s:%d: expected variable type", file->path, line_no);
        convert_var_decl(decl, sizeof(decl), name, type);
        if(expr != NULL && expr[0] != '\0' && strchr(expr, '(') != NULL)
            emit_source_push(file, line_no);
        if(expr != NULL && expr[0] != '\0')
            add_body(file, "    %s = %s;", decl, expr);
        else
            add_body(file, "    %s;", decl);
        if(expr != NULL && expr[0] != '\0' && strchr(expr, '(') != NULL)
            emit_source_pop(file);
    } else if(starts_word(line, "let")) {
        char *q = trim(line + strlen("let"));

        if(q[0] == '\0')
            die("%s:%d: expected declaration", file->path, line_no);
        if(strchr(q, '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    %s;", q);
        if(strchr(q, '(') != NULL)
            emit_source_pop(file);
    } else if(starts_word(line, "set")) {
        char *q = trim(line + strlen("set"));

        if(q[0] == '\0')
            die("%s:%d: expected assignment", file->path, line_no);
        if(strchr(q, '(') != NULL)
            emit_source_push(file, line_no);
        add_body(file, "    %s;", q);
        if(strchr(q, '(') != NULL)
            emit_source_pop(file);
    } else if(starts_word(line, "native")) {
        char *q = trim(line + strlen("native"));

        if(q[0] == '\0')
            die("%s:%d: expected native expression", file->path, line_no);
        add_body(file, "    %s;", q);
    } else if(starts_word(line, "c")) {
        char *q = trim(line + strlen("c"));

        if(q[0] == '\0')
            die("%s:%d: expected raw C line", file->path, line_no);
        add_body(file, "    %s", q);
    } else if(starts_word(line, "draw") || starts_word(line, "do") ||
              starts_word(line, "widget")) {
        char *q = starts_word(line, "draw")
                      ? trim(line + strlen("draw"))
                      : (starts_word(line, "widget")
                             ? trim(line + strlen("widget"))
                             : trim(line + strlen("do")));

        if(q[0] == '\0')
            die("%s:%d: expected expression", file->path, line_no);
        emit_source_push(file, line_no);
        emit_call(file, "    ", q, ";");
        emit_source_pop(file);
    } else if(starts_word(line, "advance")) {
        char *q = trim(line + strlen("advance"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected variable name", file->path, line_no);
        q = trim(q);
        if(!starts_word(q, "by"))
            die("%s:%d: expected 'by' in advance statement", file->path,
                line_no);
        q = trim(q + strlen("by"));
        if(q[0] == '\0')
            die("%s:%d: expected advance expression", file->path, line_no);
        add_body(file, "    %s += %s;", name, q);
    } else if(starts_word(line, "clamp_min") ||
              starts_word(line, "clamp_max")) {
        int is_min = starts_word(line, "clamp_min");
        char *q = trim(line + strlen(is_min ? "clamp_min" : "clamp_max"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected variable name", file->path, line_no);
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected clamp expression", file->path, line_no);
        add_body(file, "    if(%s %c %s) {", name, is_min ? '<' : '>', q);
        add_body(file, "        %s = %s;", name, q);
        add_body(file, "    }");
    } else if(starts_word(line, "rect")) {
        char *q = trim(line + strlen("rect"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected rectangle name", file->path, line_no);
        q = trim(q);
        if(*q != '=')
            die("%s:%d: expected '=' in rect statement", file->path,
                line_no);
        q = trim(q + 1);
        if(q[0] == '\0')
            die("%s:%d: expected rectangle expression list", file->path,
                line_no);
        add_body(file, "    Rectangle %s = (Rectangle){%s};", name, q);
    } else if(starts_word(line, "texture")) {
        char *q = trim(line + strlen("texture"));
        char name[KC_NAME_MAX];

        if(!parse_ident(&q, name, sizeof(name)))
            die("%s:%d: expected texture name", file->path, line_no);
        q = trim(q);
        if(*q != '=')
            die("%s:%d: expected '=' in texture statement", file->path,
                line_no);
        q = trim(q + 1);
        if(q[0] == '\0')
            die("%s:%d: expected texture expression", file->path, line_no);
        add_body(file, "    Texture2D %s = %s;", name, q);
    } else if(starts_word(line, "if")) {
        char *q = trim(line + strlen("if"));
        size_t n = strlen(q);

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected if condition ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected if condition", file->path, line_no);
        add_body(file, "    if(%s) {", q);
    } else if(starts_word(line, "for")) {
        char *q = trim(line + strlen("for"));
        size_t n = strlen(q);
        char *in;

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected for expression ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        if(q[0] == '\0')
            die("%s:%d: expected for expression", file->path, line_no);
        in = strstr(q, " in ");
        if(in != NULL) {
            char name[KC_NAME_MAX];
            char *range;
            char *dots;

            *in = '\0';
            range = trim(in + strlen(" in "));
            dots = strstr(range, "..");
            if(!parse_ident(&q, name, sizeof(name)) || dots == NULL)
                die("%s:%d: expected for NAME in START..END", file->path,
                    line_no);
            *dots = '\0';
            add_body(file, "    for(int %s = %s; %s < %s; %s++) {",
                     name, trim(range), name, trim(dots + 2), name);
            return;
        }
        add_body(file, "    for(%s) {", q);
    } else if(starts_word(line, "button")) {
        char *q = trim(line + strlen("button"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected button arguments ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        add_body_line(file, 0, "    %s = DrawUIGenericButton(%s);", hit, q);
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else if(starts_word(line, "event") || starts_word(line, "on")) {
        char *q = starts_word(line, "event")
                      ? trim(line + strlen("event"))
                      : trim(line + strlen("on"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected event expression ending with {", file->path,
                line_no);
        q[n - 1] = '\0';
        q = trim(q);
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        {
            char prefix[KC_NAME_MAX + 8];

            snprintf(prefix, sizeof(prefix), "    %s = ", hit);
            emit_call(file, prefix, q, ";");
            file->current->body_line[file->current->body_count - 1] = 0;
        }
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else if(starts_word(line, "icon_button")) {
        char *q = trim(line + strlen("icon_button"));
        size_t n = strlen(q);
        char hit[KC_NAME_MAX];

        if(n == 0 || q[n - 1] != '{')
            die("%s:%d: expected icon_button arguments ending with {",
                file->path, line_no);
        q[n - 1] = '\0';
        q = trim(q);
        snprintf(hit, sizeof(hit), "__kry_hit_%d", line_no);
        add_body_line(file, 0, "    int %s;", hit);
        emit_source_push(file, line_no);
        add_body_line(file, 0,
                      "    %s = DrawUIIconButton((UIIconButton){%s});",
                      hit, q);
        emit_source_pop(file);
        add_body_line(file, 0, "    if(%s) {", hit);
    } else {
        size_t n = strlen(line);

        if(n > 2 && line[n - 1] == ')' && strchr(line, '(') != NULL) {
            emit_source_push(file, line_no);
            emit_call(file, "    ", line, ";");
            emit_source_pop(file);
            return;
        }
        if(line_is_assignment_statement(line)) {
            if(strchr(line, '(') != NULL)
                emit_source_push(file, line_no);
            add_body(file, "    %s;", line);
            if(strchr(line, '(') != NULL)
                emit_source_pop(file);
            return;
        }
        die("%s:%d: unknown statement: %s", file->path, line_no, line);
    }
}

static char *
read_text_file(const char *path)
{
    FILE *file;
    long size;
    char *text;

    file = fopen(path, "rb");
    if(file == NULL)
        die("%s: open failed: %s", path, strerror(errno));
    if(fseek(file, 0, SEEK_END) != 0)
        die("%s: seek failed", path);
    size = ftell(file);
    if(size < 0 || size > KC_TEXT_MAX)
        die("%s: file too large", path);
    if(fseek(file, 0, SEEK_SET) != 0)
        die("%s: seek failed", path);
    text = calloc((size_t)size + 1, 1);
    if(text == NULL)
        die("out of memory");
    if(fread(text, 1, (size_t)size, file) != (size_t)size)
        die("%s: read failed", path);
    fclose(file);
    return text;
}

static void
parse_kry(KryFile *file)
{
    char *text;
    char *line_start;
    int line_no = 1;
    int depth = 0;
    int in_screen = 0;

    file->text = read_text_file(file->path);
    text = file->text;
    line_start = text;
    for(char *p = text;; p++) {
        if(*p != '\n' && *p != '\0')
            continue;
        char saved = *p;
        char *line;

        *p = '\0';
        line = trim(line_start);
        if(line[0] != '\0' && line[0] != '#') {
            int opens = 0;
            int closes = 0;

            for(char *q = line; *q != '\0'; q++) {
                if(*q == '{')
                    opens++;
                else if(*q == '}')
                    closes++;
            }
            if(starts_word(line, "include")) {
                char *q = line + strlen("include");

                if(file->include_count >= KC_INCLUDE_MAX)
                    die("%s:%d: too many includes", file->path, line_no);
                if(!parse_quoted(&q, file->includes[file->include_count],
                                 sizeof(file->includes[file->include_count])))
                    die("%s:%d: expected quoted include path", file->path,
                        line_no);
                file->include_count++;
            } else if(starts_word(line, "import")) {
                char *q = line + strlen("import");
                char import_path[KC_PATH_MAX];
                char import_base[KC_PATH_MAX];

                if(file->include_count >= KC_INCLUDE_MAX)
                    die("%s:%d: too many imports", file->path, line_no);
                if(!parse_quoted(&q, import_path, sizeof(import_path)))
                    die("%s:%d: expected quoted import path", file->path,
                        line_no);
                strip_kry_ext(import_base, sizeof(import_base), import_path);
                snprintf(file->includes[file->include_count],
                         sizeof(file->includes[file->include_count]),
                         "%s.h", import_base);
                file->include_count++;
            } else if(starts_word(line, "screen") ||
                      starts_word(line, "page") ||
                      starts_word(line, "fn") ||
                      starts_word(line, "pub")) {
                int is_pub = 0;
                int is_fn;
                KryFunction *fn;
                char *decl = line;
                char *q;

                if(starts_word(decl, "pub")) {
                    is_pub = 1;
                    decl = trim(decl + strlen("pub"));
                    if(!starts_word(decl, "fn"))
                        die("%s:%d: expected fn after pub", file->path,
                            line_no);
                }
                is_fn = starts_word(decl, "fn");
                q = starts_word(decl, "screen")
                        ? decl + strlen("screen")
                        : (starts_word(decl, "page")
                               ? decl + strlen("page")
                               : decl + strlen("fn"));

                if(depth != 0)
                    die("%s:%d: nested functions are not supported",
                        file->path, line_no);
                if(file->function_count >= KC_FUNCTION_MAX)
                    die("%s:%d: too many functions", file->path, line_no);
                fn = &file->functions[file->function_count++];
                memset(fn, 0, sizeof(*fn));
                file->current = fn;
                if(!parse_ident(&q, fn->screen, sizeof(fn->screen)))
                    die("%s:%d: expected screen name", file->path, line_no);
                fn->exact_name = is_fn;
                fn->is_public = is_pub || !is_fn;
                q = trim(q);
                if(q[0] == '(') {
                    char *end = strrchr(q, ')');

                    if(end == NULL)
                        die("%s:%d: expected ) in page argument list",
                            file->path, line_no);
                    *end = '\0';
                    convert_arg_list(fn->args, sizeof(fn->args), trim(q + 1));
                    q = trim(end + 1);
                }
                if(q[0] == '-' && q[1] == '>') {
                    q = trim(q + 2);
                    if(q[0] == '\0')
                        die("%s:%d: expected return type", file->path,
                            line_no);
                    if(q[strlen(q) - 1] == '{')
                        q[strlen(q) - 1] = '\0';
                    snprintf(fn->return_type, sizeof(fn->return_type),
                             "%s", trim(q));
                }
                in_screen = 1;
            } else if(in_screen && depth > 0 && starts_word(line, "args")) {
                char *q = trim(line + strlen("args"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: args outside function", file->path, line_no);
                snprintf(fn->args, sizeof(fn->args), "%s", q);
            } else if(in_screen && depth > 0 && starts_word(line, "returns")) {
                char *q = trim(line + strlen("returns"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: returns outside function", file->path, line_no);
                if(q[0] == '\0')
                    die("%s:%d: expected return type", file->path, line_no);
                snprintf(fn->return_type, sizeof(fn->return_type), "%s", q);
            } else if(in_screen && depth > 0 && starts_word(line, "call")) {
                char *q = trim(line + strlen("call"));
                KryFunction *fn = file->current;

                if(fn == NULL)
                    die("%s:%d: call outside function", file->path, line_no);
                if(fn->call_count >= KC_CALL_MAX)
                    die("%s:%d: too many calls", file->path, line_no);
                if(q[0] == '\0')
                    die("%s:%d: expected call expression", file->path,
                        line_no);
                snprintf(fn->calls[fn->call_count],
                         sizeof(fn->calls[fn->call_count]), "%s", q);
                fn->call_count++;
            } else if(in_screen && depth > 0) {
                if(line_is_close(line) && depth == 1) {
                    /* closing the page */
                } else {
                    file->current_line = line_no;
                    parse_statement(file, line_no, line);
                    file->current_line = 0;
                }
            }
            depth += opens;
            depth -= closes;
            if(depth < 0)
                die("%s:%d: unexpected }", file->path, line_no);
            if(depth == 0) {
                in_screen = 0;
                file->current = NULL;
            }
        }
        *p = saved;
        if(saved == '\0')
            break;
        line_start = p + 1;
        line_no++;
    }
    if(file->function_count == 0)
        die("%s: missing screen declaration", file->path);
    if(depth != 0)
        warn_at(file->path, line_no, "unbalanced braces");
}

static const char *
relative_path(const char *root, const char *path)
{
    size_t n;

    if(root == NULL || root[0] == '\0')
        return path;
    n = strlen(root);
    if(strncmp(path, root, n) == 0 && path[n] == '/')
        return path + n + 1;
    return path;
}

static void
path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(a == NULL || a[0] == '\0')
        snprintf(dst, dst_size, "%s", b);
    else if(a[strlen(a) - 1] == '/')
        snprintf(dst, dst_size, "%s%s", a, b);
    else
        snprintf(dst, dst_size, "%s/%s", a, b);
}

static void
strip_kry_ext(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    if(path == NULL) {
        if(dst_size > 0)
            dst[0] = '\0';
        return;
    }
    len = strlen(path);
    if(len > 4 && strcmp(path + len - 4, ".kry") == 0)
        len -= 4;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

static void
mkdir_parent(const char *path)
{
    char tmp[KC_PATH_MAX];

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            die("%s: mkdir failed: %s", tmp, strerror(errno));
        *p = '/';
    }
}

static void
header_guard(char *dst, size_t dst_size, const char *rel)
{
    size_t n = 0;

    for(const char *p = rel; *p != '\0' && n + 1 < dst_size; p++) {
        unsigned char ch = (unsigned char)*p;

        if((ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9'))
            dst[n++] = (char)ch;
        else if(ch >= 'a' && ch <= 'z')
            dst[n++] = (char)(ch - 'a' + 'A');
        else
            dst[n++] = '_';
    }
    if(n + 7 < dst_size) {
        memcpy(dst + n, "_H", 3);
        n += 2;
    }
    dst[n] = '\0';
}

static const char *
skip_indent(const char *line)
{
    while(*line == ' ' || *line == '\t')
        line++;
    return line;
}

static int
brace_delta(const char *line)
{
    int delta = 0;

    if(line[0] == '#')
        return 0;
    for(const char *p = line; *p != '\0'; p++) {
        if(*p == '{')
            delta++;
        else if(*p == '}')
            delta--;
    }
    return delta;
}

static void
write_body_line(FILE *out, const char *line, int *indent)
{
    const char *text = skip_indent(line);
    int delta;

    if(text[0] == '}')
        (*indent)--;
    if(*indent < 0)
        *indent = 0;
    if(text[0] != '#') {
        for(int i = 0; i < *indent; i++)
            fputs("    ", out);
    }
    fprintf(out, "%s\n", text);
    delta = brace_delta(text);
    if(text[0] == '}')
        delta++;
    *indent += delta;
    if(*indent < 0)
        *indent = 0;
}

static void
function_name(char *dst, size_t dst_size, const KryFunction *fn)
{
    if(fn->exact_name)
        snprintf(dst, dst_size, "%s", fn->screen);
    else
        snprintf(dst, dst_size, "%s_kry_draw", fn->screen);
}

static void
write_generated(const KryFile *file, const char *root, const char *out_dir)
{
    const char *rel = relative_path(root, file->path);
    char gen_rel[KC_PATH_MAX];
    char crel[KC_PATH_MAX];
    char hrel[KC_PATH_MAX];
    char cpath[KC_PATH_MAX];
    char hpath[KC_PATH_MAX];
    char guard[KC_NAME_MAX * 2];
    char hbase[KC_PATH_MAX];
    FILE *out;

    strip_kry_ext(gen_rel, sizeof(gen_rel), rel);
    snprintf(crel, sizeof(crel), "%s.c", gen_rel);
    snprintf(hrel, sizeof(hrel), "%s.h", gen_rel);
    path_join(cpath, sizeof(cpath), out_dir, crel);
    path_join(hpath, sizeof(hpath), out_dir, hrel);
    mkdir_parent(cpath);
    mkdir_parent(hpath);
    header_guard(guard, sizeof(guard), gen_rel);
    snprintf(hbase, sizeof(hbase), "%s.h", gen_rel);

    out = fopen(hpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", hpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#ifndef %s\n#define %s\n\n", guard, guard);
    for(int i = 0; i < file->include_count; i++)
        fprintf(out, "#include \"%s\"\n", file->includes[i]);
    if(file->include_count > 0)
        fputc('\n', out);
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];

        if(!fn->is_public)
            continue;
        function_name(name, sizeof(name), fn);
        fprintf(out, "%s %s(%s);\n", return_type, name, args);
    }
    fputc('\n', out);
    fprintf(out, "#endif\n");
    fclose(out);

    out = fopen(cpath, "wb");
    if(out == NULL)
        die("%s: open failed: %s", cpath, strerror(errno));
    fprintf(out, "/* Generated by kc from %s. */\n", rel);
    fprintf(out, "#include \"%s\"\n", hbase);
    fprintf(out, "#include \"ui_inspect.h\"\n");
    for(int i = 0; i < file->function_count; i++) {
        const KryFunction *fn = &file->functions[i];
        const char *args = fn->args[0] != '\0' ? fn->args : "void";
        const char *return_type = fn->return_type[0] != '\0'
                                      ? fn->return_type
                                      : "void";
        char name[KC_NAME_MAX + 16];
        int indent = 1;

        function_name(name, sizeof(name), fn);
        fprintf(out, "\n%s%s\n%s(%s)\n{\n",
                fn->is_public ? "" : "static ", return_type, name, args);
        for(int j = 0; j < fn->body_count; j++)
            write_body_line(out, fn->body[j], &indent);
        for(int j = 0; j < fn->call_count; j++) {
            const char *call = fn->calls[j];
            size_t len = strlen(call);

            for(int k = 0; k < indent; k++)
                fputs("    ", out);
            fprintf(out, "%s", call);
            if(len == 0 || call[len - 1] != ';')
                fputc(';', out);
            fputc('\n', out);
        }
        fprintf(out, "}\n");
    }
    fclose(out);
}

static void
usage(void)
{
    fprintf(stderr, "usage: kc --root DIR -o DIR file.kry ...\n");
}

int
main(int argc, char **argv)
{
    const char *root = ".";
    const char *out_dir = NULL;
    int first_file = 0;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--root") == 0) {
            if(++i >= argc)
                die("--root needs a directory");
            root = argv[i];
        } else if(strcmp(argv[i], "-o") == 0) {
            if(++i >= argc)
                die("-o needs a directory");
            out_dir = argv[i];
        } else if(argv[i][0] == '-') {
            usage();
            return 1;
        } else {
            first_file = i;
            break;
        }
    }
    if(out_dir == NULL || first_file == 0) {
        usage();
        return 1;
    }
    for(int i = first_file; i < argc; i++) {
        KryFile file = {0};

        file.path = argv[i];
        parse_kry(&file);
        write_generated(&file, root, out_dir);
        free(file.text);
    }
    return 0;
}

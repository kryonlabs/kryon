#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#define KT_LINE_MAX 4096
#define KT_PATH_MAX 1024
#define KT_MAX_TESTS 512

typedef struct KTOptions {
    int headless;
    int snap;
    int bless;
    const char *files[KT_MAX_TESTS];
    int file_count;
} KTOptions;

typedef struct KTRun {
    char root[KT_PATH_MAX];
    FILE *log;
    int steps;
    int failures;
} KTRun;

static void
kt_usage(FILE *out)
{
    fprintf(out,
            "usage: kt [-headless] [-snap] [-bless] [tests/file.kt ...]\n");
}

static char *
kt_trim(char *s)
{
    char *end;

    while(*s != '\0' && isspace((unsigned char)*s))
        s++;
    end = s + strlen(s);
    while(end > s && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return s;
}

static int
kt_ends_with(const char *s, const char *suffix)
{
    size_t n;
    size_t m;

    if(s == NULL || suffix == NULL)
        return 0;
    n = strlen(s);
    m = strlen(suffix);
    return n >= m && strcmp(s + n - m, suffix) == 0;
}

static int
kt_mkdir(const char *path)
{
    if(mkdir(path, 0775) == 0)
        return 1;
    return errno == EEXIST;
}

static int
kt_join(char *out, size_t out_size, const char *a, const char *b)
{
    if(a == NULL || a[0] == '\0')
        a = ".";
    if(b == NULL)
        b = "";
    return snprintf(out, out_size, "%s/%s", a, b) < (int)out_size;
}

static int
kt_parse_word(char **cursor, char *out, size_t out_size)
{
    char *s = kt_trim(*cursor);
    size_t n = 0;

    if(*s == '\0')
        return 0;
    while(*s != '\0' && !isspace((unsigned char)*s)) {
        if(n + 1 < out_size)
            out[n++] = *s;
        s++;
    }
    out[n] = '\0';
    *cursor = s;
    return n > 0;
}

static int
kt_parse_arg(char **cursor, char *out, size_t out_size, char *err,
             size_t err_size)
{
    char *s = kt_trim(*cursor);
    size_t n = 0;

    if(*s == '\0') {
        snprintf(err, err_size, "missing argument");
        return 0;
    }
    if(*s == '"') {
        s++;
        while(*s != '\0' && *s != '"') {
            if(*s == '\\' && s[1] != '\0')
                s++;
            if(n + 1 < out_size)
                out[n++] = *s;
            s++;
        }
        if(*s != '"') {
            snprintf(err, err_size, "unterminated string");
            return 0;
        }
        s++;
        if(*kt_trim(s) != '\0') {
            snprintf(err, err_size, "unexpected trailing text");
            return 0;
        }
    } else {
        while(*s != '\0' && !isspace((unsigned char)*s)) {
            if(n + 1 < out_size)
                out[n++] = *s;
            s++;
        }
        if(*kt_trim(s) != '\0') {
            snprintf(err, err_size, "unexpected trailing text");
            return 0;
        }
    }
    out[n] = '\0';
    *cursor = s;
    return 1;
}

static int
kt_open_log(KTRun *run)
{
    char path[KT_PATH_MAX];

    if(!kt_join(path, sizeof(path), run->root, "logs"))
        return 0;
    if(!kt_mkdir(path))
        return 0;
    if(!kt_join(path, sizeof(path), run->root, "logs/kt.log"))
        return 0;
    run->log = fopen(path, "a");
    return run->log != NULL;
}

static void
kt_log(KTRun *run, const char *file, int line_no, const char *cmd,
       const char *arg)
{
    if(run->log == NULL)
        return;
    fprintf(run->log, "%s:%d: %s %s\n", file, line_no, cmd,
            arg != NULL ? arg : "");
}

static int
kt_run_command(KTRun *run, const char *file, int line_no, char *line)
{
    char *cursor = line;
    char cmd[32];
    char arg[KT_LINE_MAX];
    char err[128];
    char snapshots[KT_PATH_MAX];

    if(!kt_parse_word(&cursor, cmd, sizeof(cmd)))
        return 1;
    if(strcmp(cmd, "open") == 0 || strcmp(cmd, "tap") == 0 ||
       strcmp(cmd, "type") == 0 || strcmp(cmd, "key") == 0 ||
       strcmp(cmd, "see") == 0 || strcmp(cmd, "shot") == 0) {
        if(!kt_parse_arg(&cursor, arg, sizeof(arg), err, sizeof(err))) {
            fprintf(stderr, "%s:%d: %s\n", file, line_no, err);
            return 0;
        }
        if(strcmp(cmd, "open") == 0) {
            struct stat st;

            if(stat(arg, &st) != 0 || !S_ISDIR(st.st_mode)) {
                fprintf(stderr, "%s:%d: open target is not a directory: %s\n",
                        file, line_no, arg);
                return 0;
            }
            snprintf(run->root, sizeof(run->root), "%s", arg);
            if(run->log == NULL && !kt_open_log(run)) {
                fprintf(stderr, "%s:%d: could not open logs/kt.log\n", file,
                        line_no);
                return 0;
            }
        }
        if(strcmp(cmd, "shot") == 0) {
            if(!kt_join(snapshots, sizeof(snapshots), run->root, "snapshots") ||
               !kt_mkdir(snapshots)) {
                fprintf(stderr, "%s:%d: could not create snapshots/\n", file,
                        line_no);
                return 0;
            }
        }
        kt_log(run, file, line_no, cmd, arg);
        run->steps++;
        return 1;
    }
    fprintf(stderr, "%s:%d: unknown command: %s\n", file, line_no, cmd);
    return 0;
}

static int
kt_run_file(const KTOptions *options, const char *path)
{
    FILE *f;
    KTRun run = {0};
    char line[KT_LINE_MAX];
    int line_no = 0;
    int ok = 1;

    (void)options;
    snprintf(run.root, sizeof(run.root), ".");
    if(!kt_open_log(&run)) {
        fprintf(stderr, "%s: could not open logs/kt.log\n", path);
        return 0;
    }
    f = fopen(path, "r");
    if(f == NULL) {
        fprintf(stderr, "%s: %s\n", path, strerror(errno));
        fclose(run.log);
        return 0;
    }
    while(fgets(line, sizeof(line), f) != NULL) {
        char *trimmed;

        line_no++;
        trimmed = kt_trim(line);
        if(trimmed[0] == '\0' || trimmed[0] == '#')
            continue;
        if(!kt_run_command(&run, path, line_no, trimmed)) {
            ok = 0;
            run.failures++;
        }
    }
    if(run.log != NULL) {
        fprintf(run.log, "%s: %d steps, %d failures\n", path, run.steps,
                run.failures);
        fclose(run.log);
    }
    fclose(f);
    return ok;
}

static int
kt_compare_strings(const void *a, const void *b)
{
    const char *const *sa = a;
    const char *const *sb = b;

    return strcmp(*sa, *sb);
}

static int
kt_discover_tests(KTOptions *options)
{
    DIR *dir;
    struct dirent *entry;
    char *names[KT_MAX_TESTS];
    int count = 0;

    dir = opendir("tests");
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL && count < KT_MAX_TESTS) {
        char path[KT_PATH_MAX];

        if(!kt_ends_with(entry->d_name, ".kt"))
            continue;
        if(!kt_join(path, sizeof(path), "tests", entry->d_name))
            continue;
        names[count] = strdup(path);
        if(names[count] != NULL)
            count++;
    }
    closedir(dir);
    qsort(names, (size_t)count, sizeof(names[0]), kt_compare_strings);
    for(int i = 0; i < count; i++)
        options->files[options->file_count++] = names[i];
    return count > 0;
}

int
main(int argc, char **argv)
{
    KTOptions options = {0};
    int ok = 1;

    for(int i = 1; i < argc; i++) {
        if(strcmp(argv[i], "-headless") == 0) {
            options.headless = 1;
        } else if(strcmp(argv[i], "-snap") == 0) {
            options.snap = 1;
        } else if(strcmp(argv[i], "-bless") == 0) {
            options.bless = 1;
        } else if(strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            kt_usage(stdout);
            return 0;
        } else if(argv[i][0] == '-') {
            kt_usage(stderr);
            return 2;
        } else if(options.file_count < KT_MAX_TESTS) {
            options.files[options.file_count++] = argv[i];
        } else {
            fprintf(stderr, "kt: too many test files\n");
            return 2;
        }
    }
    if(options.file_count == 0 && !kt_discover_tests(&options)) {
        fprintf(stderr, "kt: no tests supplied and no tests/*.kt found\n");
        return 2;
    }
    for(int i = 0; i < options.file_count; i++) {
        if(!kt_run_file(&options, options.files[i]))
            ok = 0;
    }
    return ok ? 0 : 1;
}

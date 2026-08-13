#include "kryon.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#define KP_PATH_MAX 1024

typedef struct PreviewOptions {
    const char *command;
    const char *project;
    const char *source;
    const char *source_dir;
    const char *out;
    int width;
    int height;
    int count;
} PreviewOptions;

typedef struct PreviewSession {
    void *dylib;
    AppHost *host;
    DestroyAppHostCallback destroy_host;
    unsigned long generation;
} PreviewSession;

static void
usage(void)
{
    fprintf(stderr,
            "usage:\n"
            "  kryon-preview capture --project ROOT --source REL --output PNG [--width W --height H]\n"
            "  kryon-preview reload --project ROOT --source REL [--count N --width W --height H]\n"
            "  kryon-preview capture-all --project ROOT --source-dir DIR --out-dir DIR [--width W --height H]\n"
            "  kryon-preview cartridge --source FILE.kry|FILE.krb --output PNG [--project ROOT] [--width W --height H]\n");
}

static const char *
arg_value(int argc, char **argv, int *i)
{
    if(*i + 1 >= argc)
        return NULL;
    (*i)++;
    return argv[*i];
}

static int
parse_args(int argc, char **argv, PreviewOptions *opt)
{
    memset(opt, 0, sizeof(*opt));
    opt->width = 640;
    opt->height = 480;
    opt->count = 2;
    if(argc < 2)
        return 0;
    opt->command = argv[1];
    for(int i = 2; i < argc; i++) {
        const char *value = NULL;

        if(strcmp(argv[i], "--project") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->project = value;
        } else if(strcmp(argv[i], "--source") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->source = value;
        } else if(strcmp(argv[i], "--source-dir") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->source_dir = value;
        } else if(strcmp(argv[i], "--output") == 0 ||
                  strcmp(argv[i], "--out-dir") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->out = value;
        } else if(strcmp(argv[i], "--width") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->width = atoi(value);
        } else if(strcmp(argv[i], "--height") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->height = atoi(value);
        } else if(strcmp(argv[i], "--count") == 0) {
            value = arg_value(argc, argv, &i);
            if(value == NULL)
                return 0;
            opt->count = atoi(value);
        } else {
            return 0;
        }
    }
    if(opt->width <= 0)
        opt->width = 640;
    if(opt->height <= 0)
        opt->height = 480;
    if(opt->count <= 0)
        opt->count = 1;
    if(strcmp(opt->command, "capture") == 0)
        return opt->project != NULL && opt->source != NULL && opt->out != NULL;
    if(strcmp(opt->command, "reload") == 0)
        return opt->project != NULL && opt->source != NULL;
    if(strcmp(opt->command, "capture-all") == 0)
        return opt->project != NULL && opt->source_dir != NULL && opt->out != NULL;
    if(strcmp(opt->command, "cartridge") == 0)
        return opt->source != NULL && opt->out != NULL;
    return 0;
}

static void
path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(a == NULL || a[0] == '\0')
        snprintf(dst, dst_size, "%s", b != NULL ? b : "");
    else if(b == NULL || b[0] == '\0')
        snprintf(dst, dst_size, "%s", a);
    else
        snprintf(dst, dst_size, "%s/%s", a, b);
}

static int
has_suffix(const char *path, const char *suffix)
{
    size_t path_len = strlen(path);
    size_t suffix_len = strlen(suffix);

    return path_len >= suffix_len &&
           strcmp(path + path_len - suffix_len, suffix) == 0;
}

static int
copy_file(const char *src, const char *dst)
{
    FILE *in = fopen(src, "rb");
    FILE *out;
    char buf[16384];
    size_t n;

    if(in == NULL)
        return 0;
    out = fopen(dst, "wb");
    if(out == NULL) {
        fclose(in);
        return 0;
    }
    while((n = fread(buf, 1, sizeof(buf), in)) > 0) {
        if(fwrite(buf, 1, n, out) != n) {
            fclose(out);
            fclose(in);
            return 0;
        }
    }
    if(ferror(in)) {
        fclose(out);
        fclose(in);
        return 0;
    }
    fclose(out);
    fclose(in);
    return 1;
}

static int
mkdir_p(const char *path)
{
    char tmp[KP_PATH_MAX];

    snprintf(tmp, sizeof(tmp), "%s", path);
    for(char *p = tmp + 1; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        if(mkdir(tmp, 0755) != 0 && errno != EEXIST)
            return 0;
        *p = '/';
    }
    return mkdir(tmp, 0755) == 0 || errno == EEXIST;
}

static int
build_host(const char *project)
{
    char command[KP_PATH_MAX * 4];
    const char *kryon_dir = getenv("KRYON_DIR");
    int rc;

    if(kryon_dir == NULL || kryon_dir[0] == '\0')
        kryon_dir = "/usr/home/wao/src/kryon";
    snprintf(command, sizeof(command),
             "cd '%s' && gmake kryon-host KRYON_DIR='%s' >/tmp/kryon-preview-build.log 2>&1",
             project, kryon_dir);
    rc = system(command);
    return rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0;
}

static void
preview_close(PreviewSession *session)
{
    if(session->destroy_host != NULL && session->host != NULL)
        session->destroy_host(session->host);
    if(session->dylib != NULL)
        kry_dylib_close(session->dylib);
    session->dylib = NULL;
    session->host = NULL;
    session->destroy_host = NULL;
}

static int
preview_open(PreviewSession *session, const char *project)
{
    char host_path[KP_PATH_MAX];
    char copy_path[KP_PATH_MAX];
    CreateAppHostCallback create_host;

    preview_close(session);
    if(!build_host(project)) {
        fprintf(stderr, "kryon-preview: host build failed, see /tmp/kryon-preview-build.log\n");
        return 0;
    }
    path_join(host_path, sizeof(host_path), project, "build/kryon/app_host.so");
    snprintf(copy_path, sizeof(copy_path), "%s.preview.%lu.so", host_path,
             ++session->generation);
    if(!copy_file(host_path, copy_path)) {
        fprintf(stderr, "kryon-preview: could not copy %s\n", host_path);
        return 0;
    }
    session->dylib = kry_dylib_load(copy_path);
    if(session->dylib == NULL) {
        fprintf(stderr, "kryon-preview: load failed: %s\n",
                kry_dylib_error() != NULL ? kry_dylib_error() : copy_path);
        return 0;
    }
    create_host = (CreateAppHostCallback)kry_dylib_sym(session->dylib,
                                                       "CreateAppHost");
    session->destroy_host =
        (DestroyAppHostCallback)kry_dylib_sym(session->dylib,
                                              "DestroyAppHost");
    if(create_host == NULL || session->destroy_host == NULL) {
        fprintf(stderr, "kryon-preview: host missing app symbols\n");
        preview_close(session);
        return 0;
    }
    session->host = create_host(APP_HOST_ABI_VERSION, project);
    if(session->host == NULL) {
        fprintf(stderr, "kryon-preview: host rejected ABI %d\n",
                APP_HOST_ABI_VERSION);
        preview_close(session);
        return 0;
    }
    return 1;
}

static int
capture_source(PreviewSession *session, const char *source, const char *out,
               int width, int height)
{
    RenderTexture2D target;
    Image image;
    int selected;
    int saved;

    selected = SetAppScreenBySourcePath(session->host, source);
    if(!selected)
        SetAppScreen(session->host, 0);
    target = LoadRenderTexture(width, height);
    BeginTextureMode(target);
    ClearBackground(GetThemeBackground());
    BeginUIFrame(width, height, 1.0f);
    DrawAppScreen(session->host, (Rectangle){0, 0, (float)width, (float)height});
    EndUIFocus();
    EndTextureMode();
    image = LoadImageFromTexture(target.texture);
    UnloadRenderTexture(target);
    if(image.data == NULL)
        return 0;
    ImageFlipVertical(&image);
    saved = ExportImage(image, out);
    UnloadImage(image);
    if(!selected)
        fprintf(stderr, "kryon-preview: %s delegates to app preview\n", source);
    return saved;
}

static int
run_capture(const PreviewOptions *opt)
{
    PreviewSession session = {0};
    int ok;

    if(!preview_open(&session, opt->project))
        return 1;
    ok = capture_source(&session, opt->source, opt->out,
                        opt->width, opt->height);
    preview_close(&session);
    return ok ? 0 : 1;
}

static int
run_reload(const PreviewOptions *opt)
{
    PreviewSession session = {0};

    for(int i = 0; i < opt->count; i++) {
        if(!preview_open(&session, opt->project))
            return 1;
        if(!capture_source(&session, opt->source, "/tmp/kryon-preview-reload.png",
                           opt->width, opt->height)) {
            preview_close(&session);
            return 1;
        }
    }
    preview_close(&session);
    return 0;
}

static void
png_name(char *dst, size_t dst_size, const char *rel)
{
    size_t n = 0;

    for(const char *p = rel; *p != '\0' && n + 5 < dst_size; p++)
        dst[n++] = (*p == '/' || *p == '.') ? '_' : *p;
    memcpy(dst + n, ".png", 5);
}

static int
capture_all_dir(PreviewSession *session, const PreviewOptions *opt,
                const char *rel_dir, int *total)
{
    char abs_dir[KP_PATH_MAX];
    DIR *dir;
    struct dirent *entry;
    int ok = 1;

    path_join(abs_dir, sizeof(abs_dir), opt->project, rel_dir);
    dir = opendir(abs_dir);
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL) {
        char child_rel[KP_PATH_MAX];
        char child_abs[KP_PATH_MAX];
        struct stat st;

        if(strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)
            continue;
        path_join(child_rel, sizeof(child_rel), rel_dir, entry->d_name);
        path_join(child_abs, sizeof(child_abs), opt->project, child_rel);
        if(stat(child_abs, &st) != 0)
            continue;
        if(S_ISDIR(st.st_mode)) {
            if(!capture_all_dir(session, opt, child_rel, total))
                ok = 0;
        } else if(S_ISREG(st.st_mode) && has_suffix(entry->d_name, ".kry")) {
            char file[KP_PATH_MAX];
            char out_path[KP_PATH_MAX];

            png_name(file, sizeof(file), child_rel);
            path_join(out_path, sizeof(out_path), opt->out, file);
            (*total)++;
            if(!capture_source(session, child_rel, out_path,
                               opt->width, opt->height)) {
                fprintf(stderr, "kryon-preview: capture failed: %s\n", child_rel);
                ok = 0;
            }
        }
    }
    closedir(dir);
    return ok;
}

static int
run_capture_all(const PreviewOptions *opt)
{
    PreviewSession session = {0};
    int total = 0;
    int ok;

    if(!mkdir_p(opt->out))
        return 1;
    if(!preview_open(&session, opt->project))
        return 1;
    ok = capture_all_dir(&session, opt, opt->source_dir, &total);
    preview_close(&session);
    fprintf(stderr, "kryon-preview: captured=%d failed=%d\n", total, ok ? 0 : 1);
    return ok ? 0 : 1;
}

static int
file_exists(const char *path)
{
    struct stat st;

    return path != NULL && stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static int
find_k2b(char *dst, size_t dst_size)
{
    const char *kryon_dir = getenv("KRYON_DIR");
    const char *path;

    if(kryon_dir != NULL && kryon_dir[0] != '\0') {
        snprintf(dst, dst_size, "%s/build/bin/k2b", kryon_dir);
        if(file_exists(dst))
            return 1;
        snprintf(dst, dst_size, "%s/build/linux-x86_64/bin/k2b", kryon_dir);
        if(file_exists(dst))
            return 1;
    }
    if(file_exists("build/bin/k2b")) {
        snprintf(dst, dst_size, "build/bin/k2b");
        return 1;
    }
    if(file_exists("build/linux-x86_64/bin/k2b")) {
        snprintf(dst, dst_size, "build/linux-x86_64/bin/k2b");
        return 1;
    }
    path = getenv("PATH");
    if(path != NULL) {
        char buf[KP_PATH_MAX];
        const char *start = path;

        while(*start != '\0') {
            const char *colon = strchr(start, ':');
            size_t n = colon != NULL ? (size_t)(colon - start) : strlen(start);

            if(n > 0 && n + 4 < sizeof(buf)) {
                memcpy(buf, start, n);
                buf[n] = '\0';
                path_join(dst, dst_size, buf, "k2b");
                if(file_exists(dst))
                    return 1;
            }
            if(colon == NULL)
                break;
            start = colon + 1;
        }
    }
    return 0;
}

static int
compile_krb(const char *kc, const char *project, const char *source,
            char *out_krb, size_t out_size)
{
    char command[KP_PATH_MAX * 4];
    char out_dir[KP_PATH_MAX];
    char rel[KP_PATH_MAX];
    char src_abs[KP_PATH_MAX];
    int rc;

    snprintf(out_dir, sizeof(out_dir), "%s/kryon-preview-krb",
             getenv("TMPDIR") != NULL ? getenv("TMPDIR") : "/tmp");
    snprintf(rel, sizeof(rel), "%s", source);
    if(has_suffix(rel, ".kry"))
        rel[strlen(rel) - 4] = '\0';
    path_join(out_krb, out_size, out_dir, rel);
    {
        size_t n = strlen(out_krb);
        if(n + 5 < out_size)
            memcpy(out_krb + n, ".krb", 5);
    }
    {
        char parent[KP_PATH_MAX];
        char *slash;

        snprintf(parent, sizeof(parent), "%s", out_krb);
        slash = strrchr(parent, '/');
        if(slash != NULL) {
            *slash = '\0';
            if(!mkdir_p(parent))
                return 0;
        }
    }
    if(source[0] == '/')
        snprintf(src_abs, sizeof(src_abs), "%s", source);
    else
        path_join(src_abs, sizeof(src_abs), project, source);
    snprintf(command, sizeof(command),
             "'%s' --no-main --root '%s' -o '%s' '%s'",
             kc, project, out_dir, src_abs);
    rc = system(command);
    if(!(rc != -1 && WIFEXITED(rc) && WEXITSTATUS(rc) == 0))
        return 0;
    return file_exists(out_krb);
}

static int
capture_cartridge(const char *krb_path, const char *out, int width, int height)
{
    KrbImage img;
    RenderTexture2D target;
    Image image;
    int saved;

    memset(&img, 0, sizeof(img));
    if(KrbLoadFile(&img, krb_path) != 0) {
        fprintf(stderr, "kryon-preview: could not load %s\n", krb_path);
        return 0;
    }
    target = LoadRenderTexture(width, height);
    BeginTextureMode(target);
    ClearBackground(GetThemeBackground());
    BeginUIFrame(width, height, 1.0f);
    KrbDraw(&img, 0, 0, width, height);
    EndUIFocus();
    EndTextureMode();
    KrbFree(&img);
    image = LoadImageFromTexture(target.texture);
    UnloadRenderTexture(target);
    if(image.data == NULL)
        return 0;
    ImageFlipVertical(&image);
    saved = ExportImage(image, out);
    UnloadImage(image);
    return saved;
}

static int
run_cartridge(const PreviewOptions *opt)
{
    char krb_path[KP_PATH_MAX];
    const char *source = opt->source;

    if(has_suffix(source, ".krb")) {
        if(source[0] == '/' || opt->project == NULL)
            snprintf(krb_path, sizeof(krb_path), "%s", source);
        else
            path_join(krb_path, sizeof(krb_path), opt->project, source);
        return capture_cartridge(krb_path, opt->out, opt->width, opt->height)
                   ? 0
                   : 1;
    }
    if(has_suffix(source, ".kry")) {
        char kc[KP_PATH_MAX];
        const char *project = opt->project != NULL ? opt->project : ".";

        if(!find_k2b(kc, sizeof(kc))) {
            fprintf(stderr, "kryon-preview: k2b not found for cartridge compile\n");
            return 1;
        }
        if(!compile_krb(kc, project, source, krb_path, sizeof(krb_path))) {
            fprintf(stderr, "kryon-preview: k2b failed\n");
            return 1;
        }
        return capture_cartridge(krb_path, opt->out, opt->width, opt->height)
                   ? 0
                   : 1;
    }
    fprintf(stderr, "kryon-preview: cartridge source must be .kry or .krb\n");
    return 1;
}

int
main(int argc, char **argv)
{
    PreviewOptions opt;
    int rc;

    if(!parse_args(argc, argv, &opt)) {
        usage();
        return 2;
    }
    SetTraceLogLevel(LOG_WARNING);
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    InitWindow(opt.width, opt.height, "Kryon Preview");
    SetTargetFPS(60);
    InitUI(opt.width, opt.height, 1.0f);
    SetCurrentTheme(THEME_MONO, 0);
    setenv("KRYON_INSPECT", "1", 1);
    if(strcmp(opt.command, "capture") == 0)
        rc = run_capture(&opt);
    else if(strcmp(opt.command, "reload") == 0)
        rc = run_reload(&opt);
    else if(strcmp(opt.command, "cartridge") == 0)
        rc = run_cartridge(&opt);
    else
        rc = run_capture_all(&opt);
    CloseWindow();
    return rc;
}

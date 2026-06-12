#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FLINT_VERSION "0.1.0"

typedef struct FlintProject {
    char root[PATH_MAX];
    char name[128];
    char app_id[256];
    char android_activity[256];
    char android_project[PATH_MAX];
    char android_keystore[PATH_MAX];
    char android_key_alias[128];
    char linux_arches[128];
    char windows_arches[128];
    char core_dir[PATH_MAX];
    char version_header[PATH_MAX];
    char itch_user[128];
    char itch_game[128];
    char itch_web_channel[64];
    char itch_windows_channel[64];
    char itch_linux_channel[64];
    char itch_android_channel[64];
    int has_linux;
    int has_windows;
    int has_web;
    int has_android;
} FlintProject;

static void usage(FILE *out)
{
    fprintf(out,
            "flint %s\n"
            "\n"
            "Usage:\n"
            "  flint doctor\n"
            "  flint devices\n"
            "  flint make-vars\n"
            "  flint build [native|linux|windows|web|android]\n"
            "  flint build-all\n"
            "  flint dist [all|linux|windows|android|android-apk|android-bundle|itch]\n"
            "  flint run [native|android|web] [--device ID] [--port PORT]\n"
            "  flint clean\n"
            "  flint help\n",
            FLINT_VERSION);
}

static int path_exists(const char *path)
{
    return access(path, F_OK) == 0;
}

static int join_path(char *out, size_t out_size, const char *a, const char *b)
{
    int n = snprintf(out, out_size, "%s/%s", a, b);
    return n > 0 && (size_t)n < out_size;
}

static char *trim(char *s)
{
    char *end;
    while(*s && isspace((unsigned char)*s))
        s++;
    if(*s == '\0')
        return s;
    end = s + strlen(s) - 1;
    while(end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static int parse_quoted_value(const char *line, char *out, size_t out_size)
{
    const char *start = strchr(line, '"');
    const char *end;
    size_t len;
    if(start == NULL)
        return 0;
    start++;
    end = strchr(start, '"');
    if(end == NULL)
        return 0;
    len = (size_t)(end - start);
    if(len >= out_size)
        len = out_size - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    return 1;
}

static int parse_quoted_list_value(const char *line, char *out, size_t out_size)
{
    const char *p = line;
    size_t used = 0;

    while((p = strchr(p, '"')) != NULL) {
        const char *end = strchr(p + 1, '"');
        size_t len;

        if(end == NULL)
            break;

        len = (size_t)(end - (p + 1));
        if(len > 0) {
            if(used > 0) {
                if(used + 1 >= out_size)
                    break;
                out[used++] = ' ';
            }
            if(len >= out_size - used)
                len = out_size - used - 1;
            memcpy(out + used, p + 1, len);
            used += len;
            out[used] = '\0';
        }

        p = end + 1;
    }

    return used > 0;
}

static int find_project_root(char *out, size_t out_size)
{
    char current[PATH_MAX];
    if(getcwd(current, sizeof(current)) == NULL)
        return 0;

    for(;;) {
        char candidate[PATH_MAX];
        if(!join_path(candidate, sizeof(candidate), current, "flint.toml"))
            return 0;
        if(path_exists(candidate)) {
            snprintf(out, out_size, "%s", current);
            return 1;
        }

        if(strcmp(current, "/") == 0)
            break;

        char *slash = strrchr(current, '/');
        if(slash == NULL)
            break;
        if(slash == current)
            slash[1] = '\0';
        else
            *slash = '\0';
    }

    return 0;
}

static void project_defaults(FlintProject *project)
{
    memset(project, 0, sizeof(*project));
    snprintf(project->name, sizeof(project->name), "app");
    snprintf(project->app_id, sizeof(project->app_id), "app");
    snprintf(project->android_activity, sizeof(project->android_activity), "android.app.NativeActivity");
    snprintf(project->android_project, sizeof(project->android_project), "droid");
    snprintf(project->android_keystore, sizeof(project->android_keystore), "flint-release.keystore");
    snprintf(project->linux_arches, sizeof(project->linux_arches), "x86_64 aarch64");
    snprintf(project->windows_arches, sizeof(project->windows_arches), "x86_64 i686");
    snprintf(project->version_header, sizeof(project->version_header), "src/version.h");
    snprintf(project->itch_web_channel, sizeof(project->itch_web_channel), "html5");
    snprintf(project->itch_windows_channel, sizeof(project->itch_windows_channel), "windows");
    snprintf(project->itch_linux_channel, sizeof(project->itch_linux_channel), "linux");
    snprintf(project->itch_android_channel, sizeof(project->itch_android_channel), "android");
}

static int load_project(FlintProject *project, int required)
{
    char path[PATH_MAX];
    char section[128] = "";
    FILE *file;
    char line[1024];

    project_defaults(project);
    if(!find_project_root(project->root, sizeof(project->root))) {
        if(required)
            fprintf(stderr, "flint: no flint.toml found in this directory or its parents\n");
        return 0;
    }

    if(!join_path(path, sizeof(path), project->root, "flint.toml"))
        return 0;

    file = fopen(path, "r");
    if(file == NULL) {
        if(required)
            fprintf(stderr, "flint: failed to open %s: %s\n", path, strerror(errno));
        return 0;
    }

    while(fgets(line, sizeof(line), file) != NULL) {
        char *s = trim(line);
        char value[256];
        if(*s == '\0' || *s == '#')
            continue;
        if(*s == '[') {
            char *end = strchr(s, ']');
            if(end != NULL) {
                *end = '\0';
                snprintf(section, sizeof(section), "%s", s + 1);
                if(strcmp(section, "platform.linux") == 0)
                    project->has_linux = 1;
                else if(strcmp(section, "platform.windows") == 0)
                    project->has_windows = 1;
                else if(strcmp(section, "platform.web") == 0)
                    project->has_web = 1;
                else if(strcmp(section, "platform.android") == 0)
                    project->has_android = 1;
            }
            continue;
        }
        if(strncmp(section, "app", sizeof(section)) == 0) {
            if(strncmp(s, "name", 4) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->name, sizeof(project->name), "%s", value);
            else if(strncmp(s, "id", 2) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->app_id, sizeof(project->app_id), "%s", value);
            else if(strncmp(s, "version_header", 14) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->version_header, sizeof(project->version_header), "%s", value);
        } else if(strncmp(section, "paths", sizeof(section)) == 0) {
            if(strncmp(s, "android_project", 15) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->android_project, sizeof(project->android_project), "%s", value);
            else if(strncmp(s, "core_library", 12) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->core_dir, sizeof(project->core_dir), "%s", value);
        } else if(strncmp(section, "platform.android", sizeof(section)) == 0) {
            if(strncmp(s, "activity", 8) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->android_activity, sizeof(project->android_activity), "%s", value);
            else if(strncmp(s, "keystore", 8) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->android_keystore, sizeof(project->android_keystore), "%s", value);
            else if(strncmp(s, "key_alias", 9) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->android_key_alias, sizeof(project->android_key_alias), "%s", value);
        } else if(strncmp(section, "platform.linux", sizeof(section)) == 0) {
            if(strncmp(s, "arches", 6) == 0 && parse_quoted_list_value(s, value, sizeof(value)))
                snprintf(project->linux_arches, sizeof(project->linux_arches), "%s", value);
        } else if(strncmp(section, "platform.windows", sizeof(section)) == 0) {
            if(strncmp(s, "arches", 6) == 0 && parse_quoted_list_value(s, value, sizeof(value)))
                snprintf(project->windows_arches, sizeof(project->windows_arches), "%s", value);
        } else if(strncmp(section, "dist.itch", sizeof(section)) == 0) {
            if(strncmp(s, "user", 4) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_user, sizeof(project->itch_user), "%s", value);
            else if(strncmp(s, "game", 4) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_game, sizeof(project->itch_game), "%s", value);
            else if(strncmp(s, "web_channel", 11) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_web_channel, sizeof(project->itch_web_channel), "%s", value);
            else if(strncmp(s, "windows_channel", 15) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_windows_channel, sizeof(project->itch_windows_channel), "%s", value);
            else if(strncmp(s, "linux_channel", 13) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_linux_channel, sizeof(project->itch_linux_channel), "%s", value);
            else if(strncmp(s, "android_channel", 15) == 0 && parse_quoted_value(s, value, sizeof(value)))
                snprintf(project->itch_android_channel, sizeof(project->itch_android_channel), "%s", value);
        }
    }

    if(project->android_key_alias[0] == '\0')
        snprintf(project->android_key_alias, sizeof(project->android_key_alias), "%s-key", project->name);

    fclose(file);
    return 1;
}

static void print_make_value(const char *key, const char *value)
{
    const char *p;
    printf("%s := ", key);
    for(p = value; *p != '\0'; p++) {
        if(*p == '$')
            printf("$$$$");
        else if(*p == '#')
            printf("\\#");
        else
            putchar(*p);
    }
    putchar('\n');
}

static int command_make_vars(void)
{
    FlintProject project;
    if(!load_project(&project, 1))
        return 1;

    print_make_value("APP_NAME", project.name);
    print_make_value("ANDROID_APP_ID", project.app_id);
    print_make_value("ANDROID_ACTIVITY", project.android_activity);
    print_make_value("ANDROID_DIR", project.android_project);
    print_make_value("ANDROID_KEYSTORE", project.android_keystore);
    print_make_value("ANDROID_KEY_ALIAS", project.android_key_alias);
    print_make_value("LINUX_ARCHES", project.linux_arches);
    print_make_value("WINDOWS_ARCHES", project.windows_arches);
    if(project.core_dir[0] != '\0')
        print_make_value("CORE_DIR", project.core_dir);
    return 0;
}

static int run_process(char *const argv[])
{
    pid_t pid = fork();
    int status;

    if(pid < 0) {
        fprintf(stderr, "flint: fork failed: %s\n", strerror(errno));
        return 1;
    }

    if(pid == 0) {
        execvp(argv[0], argv);
        fprintf(stderr, "flint: failed to run %s: %s\n", argv[0], strerror(errno));
        _exit(127);
    }

    if(waitpid(pid, &status, 0) < 0) {
        fprintf(stderr, "flint: wait failed: %s\n", strerror(errno));
        return 1;
    }

    if(WIFEXITED(status))
        return WEXITSTATUS(status);
    if(WIFSIGNALED(status))
        return 128 + WTERMSIG(status);
    return 1;
}

static int capture_lines(char *const argv[], char *buffer, size_t buffer_size)
{
    FILE *pipe;
    size_t used = 0;
    char command[1024] = "";
    int i;

    for(i = 0; argv[i] != NULL; i++) {
        if(i > 0)
            strncat(command, " ", sizeof(command) - strlen(command) - 1);
        strncat(command, argv[i], sizeof(command) - strlen(command) - 1);
    }

    pipe = popen(command, "r");
    if(pipe == NULL)
        return 0;

    while(used + 1 < buffer_size) {
        size_t n = fread(buffer + used, 1, buffer_size - used - 1, pipe);
        used += n;
        if(n == 0)
            break;
    }
    buffer[used] = '\0';
    return pclose(pipe) == 0;
}

static const char *make_target_for_build(const char *target)
{
    if(target == NULL || strcmp(target, "native") == 0)
        return "native";
    if(strcmp(target, "android") == 0)
        return "android-debug";
    if(strcmp(target, "linux") == 0 || strcmp(target, "windows") == 0 || strcmp(target, "web") == 0)
        return target;
    return NULL;
}

static const char *make_target_for_dist(const char *target)
{
    if(target == NULL || strcmp(target, "all") == 0)
        return "dist";
    if(strcmp(target, "linux") == 0)
        return "dist-linux";
    if(strcmp(target, "windows") == 0)
        return "dist-windows";
    if(strcmp(target, "android") == 0 || strcmp(target, "android-apk") == 0)
        return "android-release";
    if(strcmp(target, "android-bundle") == 0)
        return "android-bundle";
    if(strcmp(target, "itch") == 0)
        return "itch";
    return NULL;
}

static int run_make(FlintProject *project, const char *target)
{
    char flake_path[PATH_MAX];
    char *make_argv[] = {"make", (char *)target, NULL};
    char *nix_argv[] = {"nix", "develop", "--command", "make", (char *)target, NULL};
    char **argv = make_argv;

    if(chdir(project->root) != 0) {
        fprintf(stderr, "flint: failed to enter %s: %s\n", project->root, strerror(errno));
        return 1;
    }

    if(strcmp(target, "clean") != 0 && getenv("RAY_CFLAGS") == NULL &&
       join_path(flake_path, sizeof(flake_path), project->root, "flake.nix") && path_exists(flake_path)) {
        printf("flint: entering nix develop for '%s'\n", target);
        argv = nix_argv;
    }

    return run_process(argv);
}

static int command_build_all(void)
{
    FlintProject project;

    if(!load_project(&project, 1))
        return 1;

    if(project.has_linux) {
        int rc;
        printf("flint: build linux\n");
        rc = run_make(&project, "linux");
        if(rc != 0)
            return rc;
    }
    if(project.has_windows) {
        int rc;
        printf("flint: build windows\n");
        rc = run_make(&project, "windows");
        if(rc != 0)
            return rc;
    }
    if(project.has_web) {
        int rc;
        printf("flint: build web\n");
        rc = run_make(&project, "web");
        if(rc != 0)
            return rc;
    }
    if(project.has_android) {
        int rc;
        printf("flint: build android\n");
        rc = run_make(&project, "android-debug");
        if(rc != 0)
            return rc;
    }

    return 0;
}

static int command_build(int argc, char **argv)
{
    FlintProject project;
    const char *requested = argc > 0 ? argv[0] : "native";
    const char *target = make_target_for_build(requested);
    if(target == NULL) {
        fprintf(stderr, "flint: unknown build target '%s'\n", argc > 0 ? argv[0] : "");
        return 2;
    }
    if(!load_project(&project, 1))
        return 1;
    if(strcmp(requested, "linux") == 0 && !project.has_linux) {
        fprintf(stderr, "flint: project does not declare platform.linux\n");
        return 2;
    }
    if(strcmp(requested, "windows") == 0 && !project.has_windows) {
        fprintf(stderr, "flint: project does not declare platform.windows\n");
        return 2;
    }
    if(strcmp(requested, "web") == 0 && !project.has_web) {
        fprintf(stderr, "flint: project does not declare platform.web\n");
        return 2;
    }
    if(strcmp(requested, "android") == 0 && !project.has_android) {
        fprintf(stderr, "flint: project does not declare platform.android\n");
        return 2;
    }
    return run_make(&project, target);
}

static int read_project_version(FlintProject *project, char *out, size_t out_size)
{
    char path[PATH_MAX];
    FILE *file;
    char line[1024];

    if(!join_path(path, sizeof(path), project->root, project->version_header))
        return 0;

    file = fopen(path, "r");
    if(file == NULL)
        return 0;

    while(fgets(line, sizeof(line), file) != NULL) {
        if(strstr(line, "VERSION_STRING") != NULL && parse_quoted_value(line, out, out_size)) {
            fclose(file);
            return 1;
        }
    }

    fclose(file);
    return 0;
}

static int require_artifact(const char *path, const char *label, const char *hint)
{
    if(path_exists(path))
        return 1;
    fprintf(stderr, "flint: %s not found at %s\n", label, path);
    fprintf(stderr, "flint: run '%s' first\n", hint);
    return 0;
}

static int find_android_release_artifact(FlintProject *project, const char *version, char *out, size_t out_size)
{
    char path[PATH_MAX];
    char command[1024];
    char output[PATH_MAX];
    char *newline;

    snprintf(path, sizeof(path), "%s/build/android/%s-%s.apk", project->root, project->name, version);
    if(path_exists(path)) {
        snprintf(out, out_size, "%s", path);
        return 1;
    }

    snprintf(path, sizeof(path), "%s/build/android/%s-latest.apk", project->root, project->name);
    if(path_exists(path)) {
        snprintf(out, out_size, "%s", path);
        return 1;
    }

    snprintf(command, sizeof(command),
             "find '%s/build/android' -maxdepth 1 -type f \\( -name '%s-*.apk' -o -name '*-release.apk' \\) ! -name '*debug*' 2>/dev/null | sort -V | tail -n 1",
             project->root, project->name);

    FILE *pipe = popen(command, "r");
    if(pipe == NULL)
        return 0;
    if(fgets(output, sizeof(output), pipe) == NULL) {
        pclose(pipe);
        return 0;
    }
    pclose(pipe);

    newline = strchr(output, '\n');
    if(newline != NULL)
        *newline = '\0';
    if(output[0] == '\0')
        return 0;
    snprintf(out, out_size, "%s", output);
    return 1;
}

static int butler_push(const char *artifact, const char *target, const char *version)
{
    char *argv[] = {"butler", "push", (char *)artifact, (char *)target, "--userversion", (char *)version, NULL};
    printf("flint: publishing %s to %s\n", artifact, target);
    return run_process(argv);
}

static int command_dist_itch(FlintProject *project)
{
    char version[128];
    char web_dir[PATH_MAX];
    char linux_artifact[PATH_MAX];
    char windows_artifact[PATH_MAX];
    char android_artifact[PATH_MAX];
    char target[512];
    int rc;

    if(project->itch_user[0] == '\0' || project->itch_game[0] == '\0') {
        fprintf(stderr, "flint: flint.toml needs [dist.itch] user and game for itch publishing\n");
        return 1;
    }

    if(system("command -v butler >/dev/null 2>&1") != 0) {
        fprintf(stderr, "flint: butler command not found\n");
        return 1;
    }

    if(!read_project_version(project, version, sizeof(version))) {
        fprintf(stderr, "flint: could not determine version from %s\n", project->version_header);
        return 1;
    }

    rc = run_make(project, "dist");
    if(rc != 0)
        return rc;

    snprintf(web_dir, sizeof(web_dir), "%s/build/web", project->root);
    snprintf(linux_artifact, sizeof(linux_artifact), "%s/build/dist/linux/%s-linux.tar.gz", project->root, project->name);
    snprintf(windows_artifact, sizeof(windows_artifact), "%s/build/dist/windows/%s-windows.zip", project->root, project->name);

    if(!require_artifact(web_dir, "Web build", "flint build web"))
        return 1;
    if(!require_artifact(linux_artifact, "Linux package", "flint dist linux"))
        return 1;
    if(!require_artifact(windows_artifact, "Windows package", "flint dist windows"))
        return 1;
    if(!find_android_release_artifact(project, version, android_artifact, sizeof(android_artifact)) ||
       !require_artifact(android_artifact, "Signed Android APK", "flint dist android"))
        return 1;

    printf("flint: publishing %s v%s to itch.io\n", project->name, version);

    snprintf(target, sizeof(target), "%s/%s:%s", project->itch_user, project->itch_game, project->itch_web_channel);
    rc = butler_push(web_dir, target, version);
    if(rc != 0)
        return rc;

    snprintf(target, sizeof(target), "%s/%s:%s", project->itch_user, project->itch_game, project->itch_windows_channel);
    rc = butler_push(windows_artifact, target, version);
    if(rc != 0)
        return rc;

    snprintf(target, sizeof(target), "%s/%s:%s", project->itch_user, project->itch_game, project->itch_linux_channel);
    rc = butler_push(linux_artifact, target, version);
    if(rc != 0)
        return rc;

    snprintf(target, sizeof(target), "%s/%s:%s", project->itch_user, project->itch_game, project->itch_android_channel);
    rc = butler_push(android_artifact, target, version);
    if(rc != 0)
        return rc;

    printf("flint: published to https://%s.itch.io/%s\n", project->itch_user, project->itch_game);
    return 0;
}

static int command_dist(int argc, char **argv)
{
    FlintProject project;
    const char *requested = argc > 0 ? argv[0] : "all";
    const char *target = make_target_for_dist(argc > 0 ? argv[0] : "all");
    if(target == NULL) {
        fprintf(stderr, "flint: unknown dist target '%s'\n", argc > 0 ? argv[0] : "");
        return 2;
    }
    if(!load_project(&project, 1))
        return 1;
    if(strcmp(requested, "linux") == 0 && !project.has_linux) {
        fprintf(stderr, "flint: project does not declare platform.linux\n");
        return 2;
    }
    if(strcmp(requested, "windows") == 0 && !project.has_windows) {
        fprintf(stderr, "flint: project does not declare platform.windows\n");
        return 2;
    }
    if((strcmp(requested, "android") == 0 || strcmp(requested, "android-apk") == 0 ||
        strcmp(requested, "android-bundle") == 0) && !project.has_android) {
        fprintf(stderr, "flint: project does not declare platform.android\n");
        return 2;
    }
    if(strcmp(requested, "itch") == 0 &&
       (!project.has_web || !project.has_linux || !project.has_windows || !project.has_android)) {
        fprintf(stderr, "flint: itch publishing requires web, linux, windows, and android platforms\n");
        return 2;
    }
    if(strcmp(target, "itch") == 0)
        return command_dist_itch(&project);
    return run_make(&project, target);
}

static int select_first_device(char *out, size_t out_size)
{
    char output[8192];
    char *line;
    char *save = NULL;
    char *argv[] = {"adb", "devices", NULL};

    if(!capture_lines(argv, output, sizeof(output)))
        return 0;

    for(line = strtok_r(output, "\n", &save); line != NULL; line = strtok_r(NULL, "\n", &save)) {
        char id[256], state[64];
        if(strncmp(line, "List of devices", 15) == 0)
            continue;
        if(sscanf(line, "%255s %63s", id, state) == 2 && strcmp(state, "device") == 0) {
            snprintf(out, out_size, "%s", id);
            return 1;
        }
    }
    return 0;
}

static int android_install_and_launch(FlintProject *project, const char *device)
{
    char component[512];
    char *install_argv[] = {"adb", "-s", (char *)device, "install", "-r",
                            "build/android/app-universal-debug.apk", NULL};
    char *start_argv[] = {"adb", "-s", (char *)device, "shell", "am", "start", "-n", component, NULL};
    int rc;

    snprintf(component, sizeof(component), "%s/%s", project->app_id, project->android_activity);

    rc = run_process(install_argv);
    if(rc != 0)
        return rc;
    return run_process(start_argv);
}

static const char *content_type_for(const char *path)
{
    const char *ext = strrchr(path, '.');
    if(ext == NULL)
        return "application/octet-stream";
    if(strcmp(ext, ".html") == 0)
        return "text/html; charset=utf-8";
    if(strcmp(ext, ".js") == 0)
        return "application/javascript";
    if(strcmp(ext, ".wasm") == 0)
        return "application/wasm";
    if(strcmp(ext, ".css") == 0)
        return "text/css";
    if(strcmp(ext, ".json") == 0)
        return "application/json";
    if(strcmp(ext, ".png") == 0)
        return "image/png";
    if(strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0)
        return "image/jpeg";
    if(strcmp(ext, ".svg") == 0)
        return "image/svg+xml";
    return "application/octet-stream";
}

static int send_all(int fd, const char *data, size_t len)
{
    while(len > 0) {
        ssize_t n = send(fd, data, len, 0);
        if(n <= 0)
            return 0;
        data += n;
        len -= (size_t)n;
    }
    return 1;
}

static void serve_one_client(int client_fd, const char *web_root)
{
    char request[2048];
    char method[16] = "";
    char url[1024] = "";
    char path[PATH_MAX];
    char header[512];
    FILE *file;
    long size;
    size_t nread;
    char buffer[8192];

    ssize_t n = recv(client_fd, request, sizeof(request) - 1, 0);
    if(n <= 0)
        return;
    request[n] = '\0';
    sscanf(request, "%15s %1023s", method, url);

    if(strcmp(method, "GET") != 0 && strcmp(method, "HEAD") != 0) {
        send_all(client_fd, "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n", 57);
        return;
    }

    if(strstr(url, "..") != NULL) {
        send_all(client_fd, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n", 45);
        return;
    }

    if(strcmp(url, "/") == 0)
        snprintf(path, sizeof(path), "%s/index.html", web_root);
    else
        snprintf(path, sizeof(path), "%s%s", web_root, url);

    char *query = strchr(path, '?');
    if(query != NULL)
        *query = '\0';

    file = fopen(path, "rb");
    if(file == NULL) {
        send_all(client_fd, "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n", 45);
        return;
    }

    fseek(file, 0, SEEK_END);
    size = ftell(file);
    fseek(file, 0, SEEK_SET);

    snprintf(header, sizeof(header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Length: %ld\r\n"
             "Content-Type: %s\r\n"
             "Cross-Origin-Opener-Policy: same-origin\r\n"
             "Cross-Origin-Embedder-Policy: require-corp\r\n"
             "Cache-Control: no-cache\r\n"
             "\r\n",
             size, content_type_for(path));
    send_all(client_fd, header, strlen(header));

    if(strcmp(method, "HEAD") == 0) {
        fclose(file);
        return;
    }

    while((nread = fread(buffer, 1, sizeof(buffer), file)) > 0)
        if(!send_all(client_fd, buffer, nread))
            break;

    fclose(file);
}

static int serve_web(const char *web_root, int port)
{
    int server_fd;
    int opt = 1;
    struct sockaddr_in addr;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        fprintf(stderr, "flint: socket failed: %s\n", strerror(errno));
        return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons((unsigned short)port);

    if(bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        fprintf(stderr, "flint: failed to bind 127.0.0.1:%d: %s\n", port, strerror(errno));
        close(server_fd);
        return 1;
    }

    if(listen(server_fd, 16) != 0) {
        fprintf(stderr, "flint: listen failed: %s\n", strerror(errno));
        close(server_fd);
        return 1;
    }

    printf("flint: serving %s\n", web_root);
    printf("flint: open http://127.0.0.1:%d/\n", port);

    for(;;) {
        int client_fd = accept(server_fd, NULL, NULL);
        if(client_fd < 0) {
            if(errno == EINTR)
                continue;
            fprintf(stderr, "flint: accept failed: %s\n", strerror(errno));
            break;
        }
        serve_one_client(client_fd, web_root);
        close(client_fd);
    }

    close(server_fd);
    return 1;
}

static int command_run(int argc, char **argv)
{
    FlintProject project;
    const char *target = argc > 0 ? argv[0] : "native";
    const char *device = NULL;
    int port = 8080;
    char selected_device[256];
    char web_root[PATH_MAX];
    int i;

    for(i = 1; i < argc; i++) {
        if(strcmp(argv[i], "--device") == 0 && i + 1 < argc) {
            device = argv[++i];
        } else if(strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            port = atoi(argv[++i]);
            if(port <= 0 || port > 65535) {
                fprintf(stderr, "flint: invalid port\n");
                return 2;
            }
        } else {
            fprintf(stderr, "flint: unknown run option '%s'\n", argv[i]);
            return 2;
        }
    }

    if(!load_project(&project, 1))
        return 1;

    if(strcmp(target, "native") == 0)
        return run_make(&project, "run");

    if(strcmp(target, "android") == 0) {
        int rc;
        if(!project.has_android) {
            fprintf(stderr, "flint: project does not declare platform.android\n");
            return 2;
        }
        rc = run_make(&project, "android-debug");
        if(rc != 0)
            return rc;
        if(device == NULL) {
            if(!select_first_device(selected_device, sizeof(selected_device))) {
                fprintf(stderr, "flint: no Android device found; run 'flint devices'\n");
                return 1;
            }
            device = selected_device;
        }
        return android_install_and_launch(&project, device);
    }

    if(strcmp(target, "web") == 0) {
        int rc;
        if(!project.has_web) {
            fprintf(stderr, "flint: project does not declare platform.web\n");
            return 2;
        }
        rc = run_make(&project, "web");
        if(rc != 0)
            return rc;
        if(!join_path(web_root, sizeof(web_root), project.root, "build/web")) {
            fprintf(stderr, "flint: web path is too long\n");
            return 1;
        }
        return serve_web(web_root, port);
    }

    fprintf(stderr, "flint: unknown run target '%s'\n", target);
    return 2;
}

static int command_devices(void)
{
    char *argv[] = {"adb", "devices", "-l", NULL};
    return run_process(argv);
}

static int command_doctor(void)
{
    FlintProject project;
    int ok = 1;

    printf("flint %s\n", FLINT_VERSION);
    if(load_project(&project, 0)) {
        printf("project: %s (%s)\n", project.name, project.root);
        printf("platforms:%s%s%s%s\n",
               project.has_linux ? " linux" : "",
               project.has_windows ? " windows" : "",
               project.has_web ? " web" : "",
               project.has_android ? " android" : "");
        if(project.has_android)
            printf("android: %s/%s\n", project.app_id, project.android_activity);
    } else {
        printf("project: no flint.toml found\n");
        ok = 0;
    }

    if(system("command -v make >/dev/null 2>&1") == 0)
        printf("make: ok\n");
    else {
        printf("make: missing\n");
        ok = 0;
    }

    if(system("command -v adb >/dev/null 2>&1") == 0)
        printf("adb: ok\n");
    else
        printf("adb: missing (needed for Android run/devices)\n");

    if(getenv("RAY_CFLAGS") != NULL)
        printf("raylib env: present\n");
    else
        printf("raylib env: missing; flint will use nix develop when flake.nix is present\n");

    return ok ? 0 : 1;
}

int main(int argc, char **argv)
{
    const char *cmd = argc > 1 ? argv[1] : "help";

    if(strcmp(cmd, "help") == 0 || strcmp(cmd, "--help") == 0 || strcmp(cmd, "-h") == 0) {
        usage(stdout);
        return 0;
    }
    if(strcmp(cmd, "version") == 0 || strcmp(cmd, "--version") == 0) {
        printf("%s\n", FLINT_VERSION);
        return 0;
    }
    if(strcmp(cmd, "doctor") == 0)
        return command_doctor();
    if(strcmp(cmd, "devices") == 0)
        return command_devices();
    if(strcmp(cmd, "make-vars") == 0)
        return command_make_vars();
    if(strcmp(cmd, "build") == 0)
        return command_build(argc - 2, argv + 2);
    if(strcmp(cmd, "build-all") == 0)
        return command_build_all();
    if(strcmp(cmd, "dist") == 0)
        return command_dist(argc - 2, argv + 2);
    if(strcmp(cmd, "run") == 0)
        return command_run(argc - 2, argv + 2);
    if(strcmp(cmd, "clean") == 0) {
        FlintProject project;
        if(!load_project(&project, 1))
            return 1;
        return run_make(&project, "clean");
    }

    fprintf(stderr, "flint: unknown command '%s'\n", cmd);
    usage(stderr);
    return 2;
}

#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700

#include "session.h"
#include "kry_dylib.h"
#include "kry_json.h"

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

static int
fail(PreviewSession *session, const char *format, ...)
{
    va_list args;

    va_start(args, format);
    vsnprintf(session->error, sizeof(session->error), format, args);
    va_end(args);
    fprintf(stderr, "kryon-preview: %s\n", session->error);
    return -1;
}

static int
path(char *output, size_t capacity, const char *directory, const char *name)
{
    int length = snprintf(output, capacity, "%s/%s", directory, name);

    return length >= 0 && (size_t)length < capacity;
}

static void
close_host(PreviewSession *session)
{
    if(session->host != NULL && session->destroy_host != NULL)
        session->destroy_host(session->host);
    if(session->dylib != NULL)
        kry_dylib_close(session->dylib);
    if(session->library_path[0] != '\0')
        unlink(session->library_path);
    session->host = NULL;
    session->dylib = NULL;
    session->destroy_host = NULL;
    session->library_path[0] = '\0';
}

static void
read_diagnostic(PreviewSession *session)
{
    KryJson *json;
    const char *message;
    const char *source;
    const char *code;
    const char *severity;
    PreviewDiagnostic *diagnostic;

    if(session->output_overflow || session->output_line[0] != '{')
        return;
    json = kry_json_parse(session->output_line);
    if(json == NULL)
        return;
    message = kry_json_string(kry_json_get(json, "message"));
    source = kry_json_string(kry_json_get(json, "path"));
    code = kry_json_string(kry_json_get(json, "code"));
    severity = kry_json_string(kry_json_get(json, "severity"));
    if(message != NULL && source != NULL && code != NULL && severity != NULL &&
       strcmp(severity, "error") == 0 && session->diagnostic_count < PREVIEW_DIAGNOSTIC_MAX) {
        diagnostic = &session->diagnostics[session->diagnostic_count++];
        snprintf(diagnostic->path, sizeof(diagnostic->path), "%s", source);
        snprintf(diagnostic->message, sizeof(diagnostic->message), "%s", message);
        snprintf(diagnostic->code, sizeof(diagnostic->code), "%s", code);
        diagnostic->line = (int)kry_json_number(kry_json_get(json, "line"));
        diagnostic->column = (int)kry_json_number(kry_json_get(json, "column"));
        diagnostic->end_line = (int)kry_json_number(kry_json_get(json, "end_line"));
        diagnostic->end_column = (int)kry_json_number(kry_json_get(json, "end_column"));
    }
    kry_json_free(json);
}

static void
read_output(PreviewSession *session)
{
    char buffer[4096];
    size_t length;

    if(session->build_output == NULL)
        return;
    clearerr(session->build_output);
    while((length = fread(buffer, 1, sizeof(buffer), session->build_output)) > 0) {
        fwrite(buffer, 1, length, stderr);
        for(size_t i = 0; i < length; i++) {
            if(buffer[i] == '\n') {
                session->output_line[session->output_length] = '\0';
                read_diagnostic(session);
                session->output_length = 0;
                session->output_overflow = 0;
            } else if(session->output_length + 1 < sizeof(session->output_line)) {
                session->output_line[session->output_length++] = buffer[i];
            } else {
                session->output_overflow = 1;
            }
        }
    }
}

void
PreviewClose(PreviewSession *session)
{
    char log_path[PREVIEW_PATH_MAX];

    if(session->build_pid > 0) {
        /* The build owns a process group, including compiler grandchildren. */
        kill(-session->build_pid, SIGKILL);
        kill(session->build_pid, SIGKILL);
        while(waitpid(session->build_pid, NULL, 0) < 0 && errno == EINTR)
            ;
        session->build_pid = 0;
    }
    read_output(session);
    if(session->build_output != NULL)
        fclose(session->build_output);
    session->build_output = NULL;
    close_host(session);
    if(session->directory[0] != '\0') {
        if(path(log_path, sizeof(log_path), session->directory, "build.log"))
            unlink(log_path);
        rmdir(session->directory);
    }
    session->directory[0] = '\0';
}

int
PreviewStartBuild(PreviewSession *session, const char *project)
{
    const char *temporary_root = getenv("TMPDIR");
    const char *make = getenv("MAKE");
    const char *kryon = getenv("KRYON_DIR");
    char log_path[PREVIEW_PATH_MAX];
    char kryon_argument[PREVIEW_PATH_MAX + 16];
    char *absolute_project;
    pid_t child;
    int output;

    if(session->build_pid > 0)
        return fail(session, "a preview build is already running");
    absolute_project = realpath(project, NULL);
    if(absolute_project == NULL)
        return fail(session, "cannot open project: %s", strerror(errno));
    if(strlen(absolute_project) >= sizeof(session->project)) {
        free(absolute_project);
        return fail(session, "project path is too long");
    }
    strcpy(session->project, absolute_project);
    free(absolute_project);
    if(temporary_root == NULL || temporary_root[0] == '\0')
        temporary_root = "/tmp";
    if(session->directory[0] == '\0') {
        if(!path(session->directory, sizeof(session->directory), temporary_root,
                 "kryon-preview.XXXXXX")) {
            session->directory[0] = '\0';
            return fail(session, "temporary directory path is too long");
        }
        if(mkdtemp(session->directory) == NULL) {
            session->directory[0] = '\0';
            return fail(session, "cannot create preview directory: %s", strerror(errno));
        }
    }
    if(!path(log_path, sizeof(log_path), session->directory, "build.log"))
        return fail(session, "build log path is too long");
    if(kryon != NULL && kryon[0] != '\0') {
        int length = snprintf(kryon_argument, sizeof(kryon_argument),
                              "KRYON_DIR=%s", kryon);
        if(length < 0 || (size_t)length >= sizeof(kryon_argument))
            return fail(session, "KRYON_DIR is too long");
    } else {
        kryon_argument[0] = '\0';
    }
    if(make == NULL || make[0] == '\0') {
#if defined(__FreeBSD__)
        make = "gmake";
#else
        make = "make";
#endif
    }
    output = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if(output < 0)
        return fail(session, "cannot create build log: %s", strerror(errno));
    session->build_output = fopen(log_path, "rb");
    if(session->build_output == NULL) {
        close(output);
        return fail(session, "cannot read build log: %s", strerror(errno));
    }
    fcntl(fileno(session->build_output), F_SETFD, FD_CLOEXEC);
    fflush(NULL);
    child = fork();
    if(child == 0) {
        setpgid(0, 0);
        if(dup2(output, STDOUT_FILENO) < 0 || dup2(output, STDERR_FILENO) < 0)
            _exit(126);
        close(output);
        if(kryon_argument[0] != '\0')
            execlp(make, make, "-C", session->project, "kryon-host", kryon_argument,
                   (char *)NULL);
        else
            execlp(make, make, "-C", session->project, "kryon-host", (char *)NULL);
        perror("kryon-preview: cannot start make");
        _exit(127);
    }
    close(output);
    if(child < 0) {
        fclose(session->build_output);
        session->build_output = NULL;
        return fail(session, "cannot start build: %s", strerror(errno));
    }
    setpgid(child, child);
    session->build_pid = child;
    session->error[0] = '\0';
    session->diagnostic_count = 0;
    session->output_length = 0;
    session->output_overflow = 0;
    return 1;
}

static int
copy_library(const char *source, const char *destination)
{
    FILE *input = fopen(source, "rb");
    FILE *output;
    char buffer[16384];
    size_t length;
    int ok = 1;

    if(input == NULL)
        return 0;
    output = fopen(destination, "wb");
    if(output == NULL) {
        fclose(input);
        return 0;
    }
    while((length = fread(buffer, 1, sizeof(buffer), input)) > 0) {
        if(fwrite(buffer, 1, length, output) != length) {
            ok = 0;
            break;
        }
    }
    if(ferror(input))
        ok = 0;
    fclose(input);
    if(fclose(output) != 0)
        ok = 0;
    return ok;
}

static int
replace_host(PreviewSession *session)
{
    PreviewSession candidate = {0};
    CreateAppHostCallback create_host;
    char source[PREVIEW_PATH_MAX];
    char name[80];
    const char *loader_error;

    snprintf(name, sizeof(name), "host-%lu.so", ++session->generation);
    if(!path(source, sizeof(source), session->project, "build/kryon/app_host.so") ||
       !path(candidate.library_path, sizeof(candidate.library_path),
             session->directory, name))
        return fail(session, "app host path is too long");
    if(!copy_library(source, candidate.library_path)) {
        close_host(&candidate);
        return fail(session, "cannot copy the built app host");
    }
    candidate.dylib = kry_dylib_load(candidate.library_path);
    if(candidate.dylib == NULL) {
        loader_error = kry_dylib_error();
        fail(session, "cannot load app host: %s",
             loader_error != NULL ? loader_error : "unknown loader error");
        close_host(&candidate);
        return -1;
    }
    create_host = (CreateAppHostCallback)kry_dylib_sym(candidate.dylib, "CreateAppHost");
    candidate.destroy_host =
        (DestroyAppHostCallback)kry_dylib_sym(candidate.dylib, "DestroyAppHost");
    if(create_host == NULL || candidate.destroy_host == NULL) {
        close_host(&candidate);
        return fail(session, "app host is missing CreateAppHost or DestroyAppHost");
    }
    candidate.host = create_host(APP_HOST_ABI_VERSION, session->project);
    if(candidate.host == NULL) {
        close_host(&candidate);
        return fail(session, "app host rejected ABI %d or failed initialization",
                    APP_HOST_ABI_VERSION);
    }
    if(candidate.host->draw == NULL) {
        close_host(&candidate);
        return fail(session, "app host has no draw callback");
    }
    /* Only a complete candidate may replace the working host. */
    close_host(session);
    session->host = candidate.host;
    session->dylib = candidate.dylib;
    session->destroy_host = candidate.destroy_host;
    strcpy(session->library_path, candidate.library_path);
    session->error[0] = '\0';
    return 1;
}

int
PreviewPollBuild(PreviewSession *session)
{
    int status;
    pid_t result;

    if(session->build_pid <= 0)
        return fail(session, "no preview build is running");
    read_output(session);
    result = waitpid(session->build_pid, &status, WNOHANG);
    if(result == 0 || (result < 0 && errno == EINTR))
        return 0;
    session->build_pid = 0;
    read_output(session);
    fclose(session->build_output);
    session->build_output = NULL;
    if(result < 0)
        return fail(session, "cannot wait for build: %s", strerror(errno));
    if(!WIFEXITED(status) || WEXITSTATUS(status) != 0)
        return fail(session, "build failed; keeping the previous preview");
    return replace_host(session);
}

#include "file_dialog.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char current_dir[PATH_MAX];
} FileDialogInternal;

typedef enum {
    DIALOG_HELPER_NONE,
    DIALOG_HELPER_KDIALOG,
    DIALOG_HELPER_YAD,
    DIALOG_HELPER_ZENITY
} DialogHelper;

static FileDialogInternal *
ensure_internal(FileDialog *dlg)
{
    FileDialogInternal *internal;

    if(dlg == NULL)
        return NULL;
    if(dlg->_internal != NULL)
        return (FileDialogInternal *)dlg->_internal;
    internal = (FileDialogInternal *)calloc(1, sizeof(FileDialogInternal));
    if(internal == NULL)
        return NULL;
    if(getcwd(internal->current_dir, sizeof(internal->current_dir)) == NULL)
        snprintf(internal->current_dir, sizeof(internal->current_dir), ".");
    dlg->_internal = internal;
    return internal;
}

static int
dir_exists(const char *path)
{
    struct stat st;

    return path != NULL && path[0] != '\0' &&
           stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int
command_exists(const char *name)
{
    const char *path;
    const char *start;
    const char *end;
    char candidate[PATH_MAX];
    size_t dir_len;

    if(name == NULL || name[0] == '\0')
        return 0;
    if(strchr(name, '/') != NULL)
        return access(name, X_OK) == 0;
    path = getenv("PATH");
    if(path == NULL || path[0] == '\0')
        path = "/bin:/usr/bin:/usr/local/bin";
    start = path;
    while(*start != '\0') {
        end = strchr(start, ':');
        dir_len = end != NULL ? (size_t)(end - start) : strlen(start);
        if(dir_len == 0)
            dir_len = 1;
        if(dir_len + 1 + strlen(name) < sizeof(candidate)) {
            if(dir_len == 1 && start[0] == ':')
                snprintf(candidate, sizeof(candidate), "./%s", name);
            else
                snprintf(candidate, sizeof(candidate), "%.*s/%s", (int)dir_len, start, name);
            if(access(candidate, X_OK) == 0)
                return 1;
        }
        if(end == NULL)
            break;
        start = end + 1;
    }
    return 0;
}

static DialogHelper
select_helper(void)
{
    const char *requested = getenv("FLINT_FILE_DIALOG_BACKEND");

    if(requested != NULL && requested[0] != '\0') {
        if(strcmp(requested, "kdialog") == 0 && command_exists("kdialog"))
            return DIALOG_HELPER_KDIALOG;
        if(strcmp(requested, "yad") == 0 && command_exists("yad"))
            return DIALOG_HELPER_YAD;
        if(strcmp(requested, "zenity") == 0 && command_exists("zenity"))
            return DIALOG_HELPER_ZENITY;
        if(strcmp(requested, "none") == 0)
            return DIALOG_HELPER_NONE;
    }
    if(command_exists("kdialog"))
        return DIALOG_HELPER_KDIALOG;
    if(command_exists("yad"))
        return DIALOG_HELPER_YAD;
    if(command_exists("zenity"))
        return DIALOG_HELPER_ZENITY;
    return DIALOG_HELPER_NONE;
}

static void
reset_dialog_result(FileDialog *dlg, FileDialogMode mode, const char *title,
                    const char *filter, const char *default_filename)
{
    if(dlg == NULL)
        return;
    dlg->active = 0;
    dlg->mode = mode;
    dlg->confirmed = 0;
    dlg->result_path[0] = '\0';
    snprintf(dlg->title, sizeof(dlg->title), "%s", title != NULL ? title : "");
    snprintf(dlg->filter, sizeof(dlg->filter), "%s", filter != NULL ? filter : "");
    snprintf(dlg->default_name, sizeof(dlg->default_name), "%s",
             default_filename != NULL ? default_filename : "");
}

static void
strip_line_end(char *text)
{
    size_t len;

    if(text == NULL)
        return;
    len = strlen(text);
    while(len > 0 && (text[len - 1] == '\n' || text[len - 1] == '\r')) {
        text[len - 1] = '\0';
        len--;
    }
}

static void
update_current_dir_from_path(FileDialogInternal *internal, const char *path)
{
    const char *slash;
    size_t len;

    if(internal == NULL || path == NULL || path[0] == '\0')
        return;
    slash = strrchr(path, '/');
    if(slash == NULL)
        return;
    len = (size_t)(slash - path);
    if(len == 0)
        len = 1;
    if(len >= sizeof(internal->current_dir))
        len = sizeof(internal->current_dir) - 1;
    memcpy(internal->current_dir, path, len);
    internal->current_dir[len] = '\0';
}

static void
append_filter_token(char *out, size_t out_size, const char *token, size_t token_len)
{
    char pattern[128];
    size_t len;

    if(out == NULL || out_size == 0 || token == NULL)
        return;
    while(token_len > 0 && (*token == ' ' || *token == '\t')) {
        token++;
        token_len--;
    }
    while(token_len > 0 && (token[token_len - 1] == ' ' || token[token_len - 1] == '\t'))
        token_len--;
    if(token_len == 0 || token_len >= sizeof(pattern) - 2)
        return;
    if(token[0] == '.')
        snprintf(pattern, sizeof(pattern), "*%.*s", (int)token_len, token);
    else
        snprintf(pattern, sizeof(pattern), "%.*s", (int)token_len, token);
    len = strlen(out);
    snprintf(out + len, out_size - len, "%s%s", len > 0 ? " " : "", pattern);
}

static void
build_filter_patterns(char *out, size_t out_size, const char *filter)
{
    const char *start;
    const char *cursor;

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    if(filter == NULL || filter[0] == '\0')
        return;
    start = filter;
    for(cursor = filter; ; cursor++) {
        if(*cursor == ',' || *cursor == ';' || *cursor == '\0') {
            append_filter_token(out, out_size, start, (size_t)(cursor - start));
            if(*cursor == '\0')
                break;
            start = cursor + 1;
        }
    }
}

static void
build_filter_arg(char *out, size_t out_size, const char *filter, DialogHelper helper)
{
    char patterns[256];

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    build_filter_patterns(patterns, sizeof(patterns), filter);
    if(patterns[0] != '\0') {
        if(helper == DIALOG_HELPER_KDIALOG)
            snprintf(out, out_size, "%s|Files", patterns);
        else
            snprintf(out, out_size, "Files | %s", patterns);
    }
}

static int
run_helper(char *const argv[], char *out, size_t out_size)
{
    int pipefd[2];
    pid_t pid;
    ssize_t got;
    size_t used = 0;
    int status = 0;
    char discard;

    if(argv == NULL || argv[0] == NULL || out == NULL || out_size == 0)
        return 0;
    out[0] = '\0';
    if(pipe(pipefd) != 0)
        return 0;
    pid = fork();
    if(pid < 0) {
        close(pipefd[0]);
        close(pipefd[1]);
        return 0;
    }
    if(pid == 0) {
        int devnull;

        close(pipefd[0]);
        if(dup2(pipefd[1], STDOUT_FILENO) < 0)
            _exit(127);
        close(pipefd[1]);
        devnull = open("/dev/null", O_WRONLY);
        if(devnull >= 0) {
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        execvp(argv[0], argv);
        _exit(errno == ENOENT ? 127 : 126);
    }
    close(pipefd[1]);
    while(used + 1 < out_size) {
        got = read(pipefd[0], out + used, out_size - used - 1);
        if(got <= 0)
            break;
        used += (size_t)got;
    }
    out[used] = '\0';
    while(read(pipefd[0], &discard, 1) > 0) {
    }
    close(pipefd[0]);
    if(waitpid(pid, &status, 0) < 0)
        return 0;
    strip_line_end(out);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0 && out[0] != '\0';
}

static void
build_start_path(char *out, size_t out_size, const char *dir, const char *default_filename)
{
    if(out == NULL || out_size == 0)
        return;
    if(default_filename != NULL && default_filename[0] != '\0') {
        snprintf(out, out_size, "%s/%s", dir != NULL && dir[0] != '\0' ? dir : ".",
                 default_filename);
    } else {
        snprintf(out, out_size, "%s/", dir != NULL && dir[0] != '\0' ? dir : ".");
    }
}

static int
run_external_dialog(FileDialog *dlg, FileDialogMode mode, const char *title,
                    const char *filter, const char *default_filename)
{
    FileDialogInternal *internal;
    DialogHelper helper;
    char path[PATH_MAX];
    char start_path[PATH_MAX];
    char filter_arg[320];
    char file_filter_arg[340];
    char title_arg[192];
    char filename_arg[PATH_MAX + 16];
    char *argv[16];
    int argc = 0;

    if(dlg == NULL)
        return 0;
    internal = ensure_internal(dlg);
    reset_dialog_result(dlg, mode, title, filter, default_filename);
    if(internal == NULL)
        return 0;

    helper = select_helper();
    if(helper == DIALOG_HELPER_NONE)
        return 0;

    build_start_path(start_path, sizeof(start_path), internal->current_dir, default_filename);
    build_filter_arg(filter_arg, sizeof(filter_arg), filter, helper);

    if(helper == DIALOG_HELPER_KDIALOG) {
        argv[argc++] = "kdialog";
        if(title != NULL && title[0] != '\0') {
            argv[argc++] = "--title";
            argv[argc++] = (char *)title;
        }
        if(mode == FILE_DIALOG_SAVE) {
            argv[argc++] = "--getsavefilename";
            argv[argc++] = start_path;
        } else if(mode == FILE_DIALOG_SELECT_FOLDER) {
            argv[argc++] = "--getexistingdirectory";
            argv[argc++] = internal->current_dir;
        } else {
            argv[argc++] = "--getopenfilename";
            argv[argc++] = internal->current_dir;
            if(filter_arg[0] != '\0')
                argv[argc++] = filter_arg;
        }
    } else {
        argv[argc++] = helper == DIALOG_HELPER_YAD ? "yad" : "zenity";
        argv[argc++] = "--file-selection";
        if(title != NULL && title[0] != '\0') {
            snprintf(title_arg, sizeof(title_arg), "--title=%s", title);
            argv[argc++] = title_arg;
        }
        if(mode == FILE_DIALOG_SAVE) {
            argv[argc++] = "--save";
            argv[argc++] = "--confirm-overwrite";
        } else if(mode == FILE_DIALOG_SELECT_FOLDER) {
            argv[argc++] = "--directory";
        }
        snprintf(filename_arg, sizeof(filename_arg), "--filename=%s", start_path);
        argv[argc++] = filename_arg;
        if(mode == FILE_DIALOG_LOAD && filter_arg[0] != '\0') {
            snprintf(file_filter_arg, sizeof(file_filter_arg), "--file-filter=%s", filter_arg);
            argv[argc++] = file_filter_arg;
        }
    }
    argv[argc] = NULL;

    if(!run_helper(argv, path, sizeof(path)))
        return 0;
    snprintf(dlg->result_path, sizeof(dlg->result_path), "%s", path);
    dlg->confirmed = 1;
    update_current_dir_from_path(internal, path);
    return 1;
}

void
InitFileDialog(FileDialog *dlg)
{
    if(dlg == NULL)
        return;
    memset(dlg, 0, sizeof(FileDialog));
    ensure_internal(dlg);
}

int
SetFileDialogCurrentDir(FileDialog *dlg, const char *path)
{
    FileDialogInternal *internal;

    if(dlg == NULL || !dir_exists(path))
        return 0;
    internal = ensure_internal(dlg);
    if(internal == NULL)
        return 0;
    snprintf(internal->current_dir, sizeof(internal->current_dir), "%s", path);
    return 1;
}

void
SetFileDialogThemeScope(const char *scope)
{
    (void)scope;
}

void
BeginLoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter)
{
    if(dlg == NULL)
        return;
    run_external_dialog(dlg, FILE_DIALOG_LOAD, title, filter, NULL);
}

void
BeginLoadFileDialog(FileDialog *dlg, const char *title)
{
    BeginLoadFilteredFileDialog(dlg, title, NULL);
}

int
LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter)
{
    return run_external_dialog(dlg, FILE_DIALOG_LOAD, title, filter, NULL);
}

int
LoadFileDialog(FileDialog *dlg, const char *title)
{
    return LoadFilteredFileDialog(dlg, title, NULL);
}

void
BeginSaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename)
{
    if(dlg == NULL)
        return;
    run_external_dialog(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
}

int
SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename)
{
    return run_external_dialog(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
}

void
BeginSelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    if(dlg == NULL)
        return;
    run_external_dialog(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
}

int
SelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    return run_external_dialog(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
}

int
UpdateFileDialog(FileDialog *dlg)
{
    if(dlg == NULL)
        return 0;
    return dlg->confirmed ? 1 : 0;
}

const char *
GetFileDialogPath(FileDialog *dlg)
{
    if(dlg == NULL || dlg->result_path[0] == '\0')
        return NULL;
    return dlg->result_path;
}

void
CloseFileDialog(FileDialog *dlg)
{
    if(dlg == NULL)
        return;
    free(dlg->_internal);
    memset(dlg, 0, sizeof(FileDialog));
}

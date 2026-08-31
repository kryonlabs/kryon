#include "file_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
copy_dialog_path(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    if(dst == NULL || dst_size == 0 || path == NULL)
        return 0;
    len = strlen(path);
    if(len >= dst_size) {
        dst[0] = '\0';
        return 0;
    }
    memcpy(dst, path, len + 1);
    return 1;
}

#if defined(PLATFORM_WEB)

#include <emscripten.h>

static int web_file_dialog_next_id = 1;

static void
web_clear_dialog_result(FileDialog *dlg)
{
    if(dlg == NULL)
        return;
    EM_ASM({
        if(Module.__kryonFileDialogResults)
            delete Module.__kryonFileDialogResults[String($0)];
    }, dlg);
}

static void
reset_dialog_result(FileDialog *dlg, FileDialogMode mode, const char *title,
                    const char *filter, const char *default_filename)
{
    if(dlg == NULL)
        return;
    web_clear_dialog_result(dlg);
    dlg->active = 0;
    dlg->mode = mode;
    dlg->confirmed = 0;
    dlg->result_path[0] = 0;
    snprintf(dlg->title, sizeof(dlg->title), "%s", title != NULL ? title : "");
    snprintf(dlg->filter, sizeof(dlg->filter), "%s", filter != NULL ? filter : "");
    snprintf(dlg->default_name, sizeof(dlg->default_name), "%s",
             default_filename != NULL ? default_filename : "");
}

static void
begin_web_load_dialog(FileDialog *dlg, const char *title, const char *filter)
{
    if(dlg == NULL)
        return;
    reset_dialog_result(dlg, FILE_DIALOG_LOAD, title, filter, NULL);
    dlg->active = 1;
    snprintf(dlg->result_path, sizeof(dlg->result_path), "/tmp/kryon-file-dialog-%d",
             web_file_dialog_next_id++);
    if(web_file_dialog_next_id <= 0)
        web_file_dialog_next_id = 1;

    EM_ASM({
        const key = String($0);
        const resultBasePath = UTF8ToString($1);
        const rawAccept = $2 ? UTF8ToString($2) : "";
        const results = Module.__kryonFileDialogResults = Module.__kryonFileDialogResults || {};
        results[key] = {status: 0};

        const normalizeAccept = (value) => String(value || "")
            .split(",")
            .map((entry) => entry.trim())
            .filter(Boolean)
            .map((entry) => entry.startsWith("*.") ? entry.substring(1) : entry)
            .join(",");

        const input = document.createElement("input");
        var settled = false;
        const finish = (status) => {
            if(settled)
                return;
            settled = true;
            results[key] = {status};
            input.remove();
        };

        input.type = "file";
        input.accept = normalizeAccept(rawAccept);
        input.style.display = "none";
        input.onchange = async function() {
            try {
                if(!input.files || input.files.length === 0) {
                    finish(2);
                    return;
                }
                const file = input.files[0];
                const name = (file.name || "selected-file")
                    .replace(new RegExp('[\\\\/]', 'g'), '_');
                const safeName = (name.slice(0, 180) || "selected-file");
                const resultPath = resultBasePath + "-" + safeName;
                const bytes = new Uint8Array(await file.arrayBuffer());
                try { FS.mkdirTree("/tmp"); } catch(e) {}
                try { FS.unlink(resultPath); } catch(e) {}
                FS.writeFile(resultPath, bytes);
                results[key] = {};
                results[key].status = 1;
                results[key].path = resultPath;
                settled = true;
                input.remove();
            } catch(e) {
                console.error("Kryon web file dialog failed:", e);
                finish(3);
            }
        };
        input.addEventListener("cancel", () => finish(2));

        document.body.appendChild(input);
        const finishCancelledAfterFocus = () => {
            setTimeout(() => {
                if(!settled && (!input.files || input.files.length === 0))
                    finish(2);
            }, 500);
        };
        window.addEventListener("focus", finishCancelledAfterFocus, {once: true});
        try {
            input.click();
        } catch(e) {
            console.error("Kryon web file dialog could not open:", e);
            finish(3);
        }
    }, dlg, dlg->result_path, dlg->filter);
}

void
InitFileDialog(FileDialog *dlg)
{
    if(dlg != NULL)
        memset(dlg, 0, sizeof(FileDialog));
}

int
SetFileDialogCurrentDir(FileDialog *dlg, const char *path)
{
    (void)dlg;
    (void)path;
    return 0;
}

void
SetFileDialogThemeScope(const char *scope)
{
    (void)scope;
}

const char *
GetFileDialogBackendName(void)
{
    return "web";
}

void
BeginLoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter)
{
    begin_web_load_dialog(dlg, title, filter);
}

void
BeginLoadFileDialog(FileDialog *dlg, const char *title)
{
    BeginLoadFilteredFileDialog(dlg, title, NULL);
}

int
LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter)
{
    BeginLoadFilteredFileDialog(dlg, title, filter);
    return 0;
}

int
LoadFileDialog(FileDialog *dlg, const char *title)
{
    return LoadFilteredFileDialog(dlg, title, NULL);
}

void
BeginSaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename)
{
    reset_dialog_result(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
}

int
SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename)
{
    reset_dialog_result(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
    return 0;
}

void
BeginSelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    reset_dialog_result(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
}

int
SelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    reset_dialog_result(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
    return 0;
}

int
UpdateFileDialog(FileDialog *dlg)
{
    int status;

    if(dlg == NULL)
        return 0;
    if(!dlg->active)
        return dlg->confirmed ? 1 : 0;

    status = EM_ASM_INT({
        const results = Module.__kryonFileDialogResults || {};
        const result = results[String($0)];
        return result ? (result.status | 0) : 0;
    }, dlg);
    if(status == 0)
        return -1;

    dlg->active = 0;
    if(status == 1) {
        dlg->confirmed = 1;
        EM_ASM({
            const results = Module.__kryonFileDialogResults || {};
            const result = results[String($0)];
            if(result && result.path)
                stringToUTF8(String(result.path), $1, $2);
        }, dlg, dlg->result_path, (int)sizeof(dlg->result_path));
        web_clear_dialog_result(dlg);
        return 1;
    }

    dlg->confirmed = 0;
    dlg->result_path[0] = 0;
    web_clear_dialog_result(dlg);
    return 0;
}

const char *
GetFileDialogPath(FileDialog *dlg)
{
    if(dlg == NULL || dlg->result_path[0] == 0)
        return NULL;
    return dlg->result_path;
}

void
CloseFileDialog(FileDialog *dlg)
{
    if(dlg != NULL) {
        web_clear_dialog_result(dlg);
        memset(dlg, 0, sizeof(FileDialog));
    }
}

#else

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#if defined(KRYON_NOTIFICATION_GDBUS)
#include <gio/gio.h>
#define FILE_DIALOG_PORTAL 1
#endif

#if defined(SYSTEM_THEME_GTK)
#include <gtk/gtk.h>
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char current_dir[PATH_MAX];
} FileDialogInternal;

typedef enum {
    DIALOG_BACKEND_AUTO,
    DIALOG_BACKEND_NONE,
#if defined(FILE_DIALOG_PORTAL)
    DIALOG_BACKEND_PORTAL,
#endif
#if defined(SYSTEM_THEME_GTK)
    DIALOG_BACKEND_GTK,
#endif
    DIALOG_BACKEND_ZENITY,
    DIALOG_BACKEND_KDIALOG,
    DIALOG_BACKEND_YAD
} DialogBackend;

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
        if(dir_len + 1 + strlen(name) < sizeof(candidate)) {
            if(dir_len == 0)
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

static DialogBackend
backend_from_name(const char *name)
{
    if(name == NULL || name[0] == '\0' || strcmp(name, "auto") == 0)
        return DIALOG_BACKEND_AUTO;
    if(strcmp(name, "none") == 0 || strcmp(name, "off") == 0 ||
       strcmp(name, "disabled") == 0)
        return DIALOG_BACKEND_NONE;
#if defined(FILE_DIALOG_PORTAL)
    if(strcmp(name, "portal") == 0 || strcmp(name, "xdg") == 0 ||
       strcmp(name, "xdg-desktop-portal") == 0)
        return DIALOG_BACKEND_PORTAL;
#endif
#if defined(SYSTEM_THEME_GTK)
    if(strcmp(name, "gtk") == 0)
        return DIALOG_BACKEND_GTK;
#endif
    if(strcmp(name, "zenity") == 0)
        return DIALOG_BACKEND_ZENITY;
    if(strcmp(name, "kdialog") == 0)
        return DIALOG_BACKEND_KDIALOG;
    if(strcmp(name, "yad") == 0)
        return DIALOG_BACKEND_YAD;
    return DIALOG_BACKEND_AUTO;
}

static int
portal_backend_available(void)
{
#if defined(FILE_DIALOG_PORTAL)
    const char *forced = getenv("KRYON_TEST_FILE_DIALOG_PORTAL_AVAILABLE");
    GDBusConnection *bus;
    GVariant *result;
    GError *error = NULL;
    gboolean owned = FALSE;

    if(forced != NULL && forced[0] != '\0')
        return strcmp(forced, "1") == 0 || strcmp(forced, "true") == 0;
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if(bus == NULL) {
        if(error != NULL)
            g_error_free(error);
        return 0;
    }
    result = g_dbus_connection_call_sync(bus,
            "org.freedesktop.DBus", "/org/freedesktop/DBus",
            "org.freedesktop.DBus", "NameHasOwner",
            g_variant_new("(s)", "org.freedesktop.portal.Desktop"),
            G_VARIANT_TYPE("(b)"),
            G_DBUS_CALL_FLAGS_NONE, 1000, NULL, &error);
    if(result != NULL) {
        g_variant_get(result, "(b)", &owned);
        g_variant_unref(result);
    } else if(error != NULL) {
        g_error_free(error);
    }
    g_object_unref(bus);
    return owned ? 1 : 0;
#else
    return 0;
#endif
}

static int
backend_available(DialogBackend backend)
{
    switch(backend) {
#if defined(FILE_DIALOG_PORTAL)
    case DIALOG_BACKEND_PORTAL:
        return portal_backend_available();
#endif
#if defined(SYSTEM_THEME_GTK)
    case DIALOG_BACKEND_GTK:
        return 1;
#endif
    case DIALOG_BACKEND_ZENITY:
        return command_exists("zenity");
    case DIALOG_BACKEND_KDIALOG:
        return command_exists("kdialog");
    case DIALOG_BACKEND_YAD:
        return command_exists("yad");
    default:
        return 0;
    }
}

static DialogBackend
select_backend_from_request(DialogBackend requested)
{
    if(requested == DIALOG_BACKEND_NONE)
        return DIALOG_BACKEND_NONE;
    if(requested != DIALOG_BACKEND_AUTO)
        return backend_available(requested) ? requested : DIALOG_BACKEND_NONE;

#if defined(FILE_DIALOG_PORTAL)
    if(backend_available(DIALOG_BACKEND_PORTAL))
        return DIALOG_BACKEND_PORTAL;
#endif
#if defined(SYSTEM_THEME_GTK)
    if(backend_available(DIALOG_BACKEND_GTK))
        return DIALOG_BACKEND_GTK;
#endif
    if(backend_available(DIALOG_BACKEND_ZENITY))
        return DIALOG_BACKEND_ZENITY;
    if(backend_available(DIALOG_BACKEND_KDIALOG))
        return DIALOG_BACKEND_KDIALOG;
    if(backend_available(DIALOG_BACKEND_YAD))
        return DIALOG_BACKEND_YAD;
    return DIALOG_BACKEND_NONE;
}

static DialogBackend
select_backend(void)
{
    const char *requested = getenv("KRYON_FILE_DIALOG_BACKEND");
    DialogBackend env_backend = backend_from_name(requested);

    return select_backend_from_request(env_backend);
}

static const char *
backend_name(DialogBackend backend)
{
    switch(backend) {
    case DIALOG_BACKEND_AUTO:
        return "auto";
    case DIALOG_BACKEND_NONE:
        return "none";
#if defined(FILE_DIALOG_PORTAL)
    case DIALOG_BACKEND_PORTAL:
        return "portal";
#endif
#if defined(SYSTEM_THEME_GTK)
    case DIALOG_BACKEND_GTK:
        return "gtk";
#endif
    case DIALOG_BACKEND_ZENITY:
        return "zenity";
    case DIALOG_BACKEND_KDIALOG:
        return "kdialog";
    case DIALOG_BACKEND_YAD:
        return "yad";
    default:
        return "unknown";
    }
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
build_filter_arg(char *out, size_t out_size, const char *filter, DialogBackend backend)
{
    char patterns[256];

    if(out == NULL || out_size == 0)
        return;
    out[0] = '\0';
    build_filter_patterns(patterns, sizeof(patterns), filter);
    if(patterns[0] != '\0') {
        if(backend == DIALOG_BACKEND_KDIALOG)
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
    const char *base;
    size_t base_len;
    size_t name_len;

    if(out == NULL || out_size == 0)
        return;
    base = dir != NULL && dir[0] != '\0' ? dir : ".";
    base_len = strlen(base);
    if(default_filename != NULL && default_filename[0] != '\0') {
        name_len = strlen(default_filename);
        if(base_len + 1 + name_len >= out_size) {
            out[0] = '\0';
            return;
        }
        memcpy(out, base, base_len);
        out[base_len] = '/';
        memcpy(out + base_len + 1, default_filename, name_len + 1);
    } else {
        if(base_len + 1 >= out_size) {
            out[0] = '\0';
            return;
        }
        memcpy(out, base, base_len);
        out[base_len] = '/';
        out[base_len + 1] = '\0';
    }
}

#if defined(FILE_DIALOG_PORTAL)
typedef struct PortalDialogWait {
    int done;
    int confirmed;
    char path[FILE_DIALOG_PATH_MAX];
} PortalDialogWait;

static void
on_portal_response(GDBusConnection *connection, const char *sender,
                   const char *object_path, const char *interface_name,
                   const char *signal_name, GVariant *parameters,
                   gpointer user_data)
{
    PortalDialogWait *wait = (PortalDialogWait *)user_data;
    guint32 response = 1;
    GVariant *results = NULL;
    GVariant *uris = NULL;
    const char *uri = NULL;
    char *filename = NULL;

    (void)connection; (void)sender; (void)object_path;
    (void)interface_name; (void)signal_name;
    if(wait == NULL)
        return;
    g_variant_get(parameters, "(u@a{sv})", &response, &results);
    if(response == 0 && results != NULL) {
        uris = g_variant_lookup_value(results, "uris", G_VARIANT_TYPE("as"));
        if(uris != NULL && g_variant_n_children(uris) > 0) {
            g_variant_get_child(uris, 0, "&s", &uri);
            filename = g_filename_from_uri(uri, NULL, NULL);
            if(filename != NULL && filename[0] != '\0') {
                snprintf(wait->path, sizeof(wait->path), "%s", filename);
                wait->confirmed = 1;
            }
        }
    }
    if(filename != NULL)
        g_free(filename);
    if(uris != NULL)
        g_variant_unref(uris);
    if(results != NULL)
        g_variant_unref(results);
    wait->done = 1;
}

static int
run_portal_dialog(FileDialog *dlg, FileDialogMode mode, const char *title,
                  const char *filter, const char *default_filename)
{
    FileDialogInternal *internal;
    GDBusConnection *bus;
    GVariantBuilder options;
    GVariant *result;
    GError *error = NULL;
    const char *method;
    const char *dialog_title;
    char *handle = NULL;
    guint sub_id;
    PortalDialogWait wait;

    (void)filter;
    internal = ensure_internal(dlg);
    reset_dialog_result(dlg, mode, title, filter, default_filename);
    if(internal == NULL)
        return 0;
    bus = g_bus_get_sync(G_BUS_TYPE_SESSION, NULL, &error);
    if(bus == NULL) {
        if(error != NULL)
            g_error_free(error);
        return 0;
    }
    if(!portal_backend_available()) {
        g_object_unref(bus);
        return 0;
    }

    g_variant_builder_init(&options, G_VARIANT_TYPE_VARDICT);
    if(mode == FILE_DIALOG_SELECT_FOLDER)
        g_variant_builder_add(&options, "{sv}", "directory",
                              g_variant_new_boolean(TRUE));
    if(mode == FILE_DIALOG_SAVE && default_filename != NULL &&
       default_filename[0] != '\0')
        g_variant_builder_add(&options, "{sv}", "current_name",
                              g_variant_new_string(default_filename));

    method = mode == FILE_DIALOG_SAVE ? "SaveFile" : "OpenFile";
    dialog_title = title != NULL && title[0] != '\0' ? title : "Select file";
    result = g_dbus_connection_call_sync(bus,
            "org.freedesktop.portal.Desktop",
            "/org/freedesktop/portal/desktop",
            "org.freedesktop.portal.FileChooser",
            method,
            g_variant_new("(ssa{sv})", "", dialog_title, &options),
            G_VARIANT_TYPE("(o)"),
            G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
    if(result == NULL) {
        if(error != NULL)
            g_error_free(error);
        g_object_unref(bus);
        return 0;
    }

    memset(&wait, 0, sizeof(wait));
    g_variant_get(result, "(o)", &handle);
    g_variant_unref(result);
    sub_id = g_dbus_connection_signal_subscribe(bus, NULL,
            "org.freedesktop.portal.Request", "Response", handle, NULL,
            G_DBUS_SIGNAL_FLAGS_NONE, on_portal_response, &wait, NULL);
    while(!wait.done)
        g_main_context_iteration(NULL, TRUE);
    g_dbus_connection_signal_unsubscribe(bus, sub_id);
    g_free(handle);
    g_object_unref(bus);

    if(!wait.confirmed || wait.path[0] == '\0')
        return 0;
    snprintf(dlg->result_path, sizeof(dlg->result_path), "%s", wait.path);
    dlg->confirmed = 1;
    update_current_dir_from_path(internal, wait.path);
    return 1;
}
#endif

#if defined(SYSTEM_THEME_GTK)
static GtkFileChooserAction
gtk_action_for_mode(FileDialogMode mode)
{
    if(mode == FILE_DIALOG_SAVE)
        return GTK_FILE_CHOOSER_ACTION_SAVE;
    if(mode == FILE_DIALOG_SELECT_FOLDER)
        return GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
    return GTK_FILE_CHOOSER_ACTION_OPEN;
}

static int
run_gtk_dialog(FileDialog *dlg, FileDialogMode mode, const char *title,
               const char *filter, const char *default_filename)
{
    FileDialogInternal *internal;
    GtkWidget *dialog;
    GtkFileChooser *chooser;
    GtkFileFilter *gtk_filter;
    char patterns[256];
    char *path;
    int response;

    internal = ensure_internal(dlg);
    reset_dialog_result(dlg, mode, title, filter, default_filename);
    if(internal == NULL)
        return 0;
    if(!gtk_init_check(NULL, NULL))
        return 0;

    dialog = gtk_file_chooser_dialog_new(title != NULL && title[0] != '\0'
                                             ? title
                                             : "Select file",
                                         NULL,
                                         gtk_action_for_mode(mode),
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         mode == FILE_DIALOG_SAVE ? "_Save" : "_Open",
                                         GTK_RESPONSE_ACCEPT,
                                         NULL);
    if(dialog == NULL)
        return 0;

    chooser = GTK_FILE_CHOOSER(dialog);
    gtk_file_chooser_set_current_folder(chooser, internal->current_dir);
    if(mode == FILE_DIALOG_SAVE && default_filename != NULL && default_filename[0] != '\0') {
        gtk_file_chooser_set_current_name(chooser, default_filename);
        gtk_file_chooser_set_do_overwrite_confirmation(chooser, TRUE);
    }
    if(mode == FILE_DIALOG_LOAD && filter != NULL && filter[0] != '\0') {
        build_filter_patterns(patterns, sizeof(patterns), filter);
        if(patterns[0] != '\0') {
            char *copy = strdup(patterns);
            char *token = copy;

            gtk_filter = gtk_file_filter_new();
            gtk_file_filter_set_name(gtk_filter, "Files");
            while(copy != NULL && token != NULL && token[0] != '\0') {
                char *next = strchr(token, ' ');
                if(next != NULL)
                    *next++ = '\0';
                if(token[0] != '\0')
                    gtk_file_filter_add_pattern(gtk_filter, token);
                token = next;
            }
            gtk_file_chooser_add_filter(chooser, gtk_filter);
            free(copy);
        }
    }

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response != GTK_RESPONSE_ACCEPT) {
        gtk_widget_destroy(dialog);
        while(gtk_events_pending())
            gtk_main_iteration();
        return 0;
    }

    path = gtk_file_chooser_get_filename(chooser);
    if(path == NULL || path[0] == '\0') {
        if(path != NULL)
            g_free(path);
        gtk_widget_destroy(dialog);
        while(gtk_events_pending())
            gtk_main_iteration();
        return 0;
    }

    if(!copy_dialog_path(dlg->result_path, sizeof(dlg->result_path), path)) {
        g_free(path);
        return 0;
    }
    dlg->confirmed = 1;
    update_current_dir_from_path(internal, path);
    g_free(path);
    gtk_widget_destroy(dialog);
    while(gtk_events_pending())
        gtk_main_iteration();
    return 1;
}
#endif

static int
run_external_dialog(FileDialog *dlg, FileDialogMode mode, const char *title,
                    const char *filter, const char *default_filename)
{
    FileDialogInternal *internal;
    DialogBackend backend;
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

    backend = select_backend();
    if(backend == DIALOG_BACKEND_NONE)
        return 0;

#if defined(FILE_DIALOG_PORTAL)
    if(backend == DIALOG_BACKEND_PORTAL)
        return run_portal_dialog(dlg, mode, title, filter, default_filename);
#endif

#if defined(SYSTEM_THEME_GTK)
    if(backend == DIALOG_BACKEND_GTK)
        return run_gtk_dialog(dlg, mode, title, filter, default_filename);
#endif

    build_start_path(start_path, sizeof(start_path), internal->current_dir, default_filename);
    build_filter_arg(filter_arg, sizeof(filter_arg), filter, backend);

    if(backend == DIALOG_BACKEND_KDIALOG) {
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
        argv[argc++] = backend == DIALOG_BACKEND_YAD ? "yad" : "zenity";
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

const char *
GetFileDialogBackendName(void)
{
    return backend_name(select_backend());
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

#endif

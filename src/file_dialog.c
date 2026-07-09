#include "file_dialog.h"

#include <gtk/gtk.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

typedef struct {
    char current_dir[PATH_MAX];
} FileDialogInternal;

static int gtk_ready = 0;
static int gtk_failed = 0;

static int
init_gtk(void)
{
    int argc = 0;
    char **argv = NULL;

    if(gtk_ready)
        return 1;
    if(gtk_failed)
        return 0;
    if(!gtk_init_check(&argc, &argv)) {
        gtk_failed = 1;
        return 0;
    }
    gtk_ready = 1;
    return 1;
}

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
add_filter_pattern(GtkFileFilter *gtk_filter, const char *token)
{
    char pattern[128];
    size_t len;

    if(gtk_filter == NULL || token == NULL)
        return;
    while(*token == ' ' || *token == '\t')
        token++;
    len = strlen(token);
    while(len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t'))
        len--;
    if(len == 0 || len >= sizeof(pattern) - 2)
        return;
    if(token[0] == '.') {
        snprintf(pattern, sizeof(pattern), "*%.*s", (int)len, token);
        gtk_file_filter_add_pattern(gtk_filter, pattern);
    } else {
        snprintf(pattern, sizeof(pattern), "%.*s", (int)len, token);
        gtk_file_filter_add_pattern(gtk_filter, pattern);
    }
}

static void
apply_filters(GtkWidget *dialog, const char *filter)
{
    GtkFileFilter *gtk_filter;
    char local[sizeof(((FileDialog *)0)->filter)];
    char *token;
    char *saveptr = NULL;

    if(dialog == NULL || filter == NULL || filter[0] == '\0')
        return;
    gtk_filter = gtk_file_filter_new();
    gtk_file_filter_set_name(gtk_filter, filter);
    snprintf(local, sizeof(local), "%s", filter);
    token = strtok_r(local, ",;", &saveptr);
    while(token != NULL) {
        add_filter_pattern(gtk_filter, token);
        token = strtok_r(NULL, ",;", &saveptr);
    }
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), gtk_filter);
}

static int
run_gtk_dialog(FileDialog *dlg, FileDialogMode mode, const char *title,
               const char *filter, const char *default_filename)
{
    GtkFileChooserAction action;
    const char *accept_label;
    GtkWidget *dialog;
    FileDialogInternal *internal;
    gint response;
    char *filename = NULL;

    if(dlg == NULL)
        return 0;
    internal = ensure_internal(dlg);
    reset_dialog_result(dlg, mode, title, filter, default_filename);
    if(internal == NULL || !init_gtk())
        return 0;

    action = GTK_FILE_CHOOSER_ACTION_OPEN;
    accept_label = "_Open";
    if(mode == FILE_DIALOG_SAVE) {
        action = GTK_FILE_CHOOSER_ACTION_SAVE;
        accept_label = "_Save";
    } else if(mode == FILE_DIALOG_SELECT_FOLDER) {
        action = GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER;
        accept_label = "_Select";
    }

    dialog = gtk_file_chooser_dialog_new(title != NULL ? title : "Select File",
                                         NULL,
                                         action,
                                         "_Cancel", GTK_RESPONSE_CANCEL,
                                         accept_label, GTK_RESPONSE_ACCEPT,
                                         NULL);
    if(dialog == NULL)
        return 0;

    if(dir_exists(internal->current_dir))
        gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), internal->current_dir);
    if(mode == FILE_DIALOG_SAVE) {
        gtk_file_chooser_set_do_overwrite_confirmation(GTK_FILE_CHOOSER(dialog), TRUE);
        if(default_filename != NULL && default_filename[0] != '\0')
            gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), default_filename);
    } else if(mode == FILE_DIALOG_LOAD) {
        apply_filters(dialog, filter);
    }

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if(response == GTK_RESPONSE_ACCEPT) {
        filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
        if(filename != NULL && filename[0] != '\0') {
            snprintf(dlg->result_path, sizeof(dlg->result_path), "%s", filename);
            dlg->confirmed = 1;
        }
    }
    if(filename != NULL)
        g_free(filename);
    gtk_widget_destroy(dialog);
    while(gtk_events_pending())
        gtk_main_iteration();
    return dlg->confirmed;
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
    run_gtk_dialog(dlg, FILE_DIALOG_LOAD, title, filter, NULL);
}

void
BeginLoadFileDialog(FileDialog *dlg, const char *title)
{
    BeginLoadFilteredFileDialog(dlg, title, NULL);
}

int
LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter)
{
    return run_gtk_dialog(dlg, FILE_DIALOG_LOAD, title, filter, NULL);
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
    run_gtk_dialog(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
}

int
SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename)
{
    return run_gtk_dialog(dlg, FILE_DIALOG_SAVE, title, NULL, default_filename);
}

void
BeginSelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    if(dlg == NULL)
        return;
    run_gtk_dialog(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
}

int
SelectFileDialogFolder(FileDialog *dlg, const char *title)
{
    return run_gtk_dialog(dlg, FILE_DIALOG_SELECT_FOLDER, title, NULL, NULL);
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

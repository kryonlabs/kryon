#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include "raylib.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FILE_DIALOG_PATH_MAX 512

typedef enum {
    FILE_DIALOG_SAVE,
    FILE_DIALOG_LOAD,
    FILE_DIALOG_SELECT_FOLDER
} FileDialogMode;

typedef struct {
    int active;
    FileDialogMode mode;
    char title[128];
    char result_path[FILE_DIALOG_PATH_MAX];
    char default_name[128];
    char filter[64];
    int confirmed;
    void *_internal;
} FileDialog;

/* Initialize file dialog */
void InitFileDialog(FileDialog *dlg);

/* Set the current folder shown by the dialog. Returns 1 when the folder exists. */
int SetFileDialogCurrentDir(FileDialog *dlg, const char *path);

/* Set an explicit theme scope; pass NULL or empty to follow the current UI theme. */
void SetFileDialogThemeScope(const char *scope);

/* Save file dialog - returns 1 if file selected, 0 if cancelled */
int SaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename);

/* Load file dialog - returns 1 if file selected, 0 if cancelled */
int LoadFileDialog(FileDialog *dlg, const char *title);
int LoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter);

/* Select folder dialog - returns 1 if folder selected, 0 if cancelled */
int SelectFileDialogFolder(FileDialog *dlg, const char *title);

/* Compatibility wrappers. Desktop GTK dialogs complete before Begin* returns. */
void BeginLoadFileDialog(FileDialog *dlg, const char *title);
void BeginLoadFilteredFileDialog(FileDialog *dlg, const char *title, const char *filter);
void BeginSaveFileDialog(FileDialog *dlg, const char *title, const char *default_filename);
void BeginSelectFileDialogFolder(FileDialog *dlg, const char *title);

/* Draw/update active dialog: 1 confirmed, 0 cancelled, -1 still active. */
int UpdateFileDialog(FileDialog *dlg);

/* Get selected file path */
const char *GetFileDialogPath(FileDialog *dlg);

/* Cleanup dialog resources */
void CloseFileDialog(FileDialog *dlg);

#ifdef __cplusplus
}
#endif

#endif /* FILE_DIALOG_H */

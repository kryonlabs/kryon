#include "flint_file_dialog.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if defined(_WIN32)
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/types.h>
#include <pwd.h>
#endif

#define PATH_MAX 512

void flint_file_dialog_init(FlintFileDialog *dlg) {
    if(dlg == NULL)
        return;

    memset(dlg, 0, sizeof(FlintFileDialog));
    dlg->mode = FLINT_FILE_DIALOG_LOAD;
    dlg->confirmed = 0;
}

/* Desktop implementation using system tools */
static int file_dialog_desktop_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    char command[512];
    FILE *pipe;
    char result[PATH_MAX] = "";

    if(!dlg || !title || !default_filename)
        return 0;

    /* Try Zenity first (GTK-based) */
    snprintf(command, sizeof(command),
             "zenity --file-selection --save --title=\"%s\" --filename=\"%s\" 2>/dev/null",
             title, default_filename);

    pipe = popen(command, "r");
    if(pipe) {
        if(fgets(result, sizeof(result), pipe)) {
            /* Remove newline */
            result[strcspn(result, "\n")] = '\0';
            strncpy(dlg->result_path, result, sizeof(dlg->result_path) - 1);
            dlg->result_path[sizeof(dlg->result_path) - 1] = '\0';
            dlg->confirmed = 1;
            pclose(pipe);
            return 1;
        }
        pclose(pipe);
    }

    /* Fallback to kdialog (KDE-based) */
    snprintf(command, sizeof(command),
             "kdialog --getsavefilename --title=\"%s\" --starting-url=\"%s\" 2>/dev/null",
             title, default_filename);

    pipe = popen(command, "r");
    if(pipe) {
        if(fgets(result, sizeof(result), pipe)) {
            result[strcspn(result, "\n")] = '\0';
            strncpy(dlg->result_path, result, sizeof(dlg->result_path) - 1);
            dlg->result_path[sizeof(dlg->result_path) - 1] = '\0';
            dlg->confirmed = 1;
            pclose(pipe);
            return 1;
        }
        pclose(pipe);
    }

    return 0;
}

static int file_dialog_desktop_load(FlintFileDialog *dlg, const char *title) {
    char command[512];
    FILE *pipe;
    char result[PATH_MAX] = "";

    if(!dlg || !title)
        return 0;

    /* Try Zenity first (GTK-based) */
    snprintf(command, sizeof(command),
             "zenity --file-selection --title=\"%s\" --file-filter=\"ZIP files | *.zip\" 2>/dev/null",
             title);

    pipe = popen(command, "r");
    if(pipe) {
        if(fgets(result, sizeof(result), pipe)) {
            /* Remove newline */
            result[strcspn(result, "\n")] = '\0';
            strncpy(dlg->result_path, result, sizeof(dlg->result_path) - 1);
            dlg->result_path[sizeof(dlg->result_path) - 1] = '\0';
            dlg->confirmed = 1;
            pclose(pipe);
            return 1;
        }
        pclose(pipe);
    }

    /* Fallback to kdialog (KDE-based) */
    snprintf(command, sizeof(command),
             "kdialog --getopenfilename --title=\"%s\" 2>/dev/null",
             title);

    pipe = popen(command, "r");
    if(pipe) {
        if(fgets(result, sizeof(result), pipe)) {
            result[strcspn(result, "\n")] = '\0';
            strncpy(dlg->result_path, result, sizeof(dlg->result_path) - 1);
            dlg->result_path[sizeof(dlg->result_path) - 1] = '\0';
            dlg->confirmed = 1;
            pclose(pipe);
            return 1;
        }
        pclose(pipe);
    }

    return 0;
}

/* Platform-specific implementations (stubs for now) */
#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
static int file_dialog_android_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    /* TODO: Implement Android file picker via JNI */
    (void)dlg; (void)title; (void)default_filename;
    return 0;
}

static int file_dialog_android_load(FlintFileDialog *dlg, const char *title) {
    /* TODO: Implement Android file picker via JNI */
    (void)dlg; (void)title;
    return 0;
}
#endif

#if defined(PLATFORM_WEB)
static int file_dialog_web_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    /* TODO: Implement HTML5 file input via Emscripten */
    (void)dlg; (void)title; (void)default_filename;
    return 0;
}

static int file_dialog_web_load(FlintFileDialog *dlg, const char *title) {
    /* TODO: Implement HTML5 file input via Emscripten */
    (void)dlg; (void)title;
    return 0;
}
#endif

/* Public API implementations */
int flint_file_dialog_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    if(!dlg)
        return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_SAVE;

    if(title)
        strncpy(dlg->title, title, sizeof(dlg->title) - 1);
    if(default_filename)
        strncpy(dlg->default_name, default_filename, sizeof(dlg->default_name) - 1);

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return file_dialog_android_save(dlg, title, default_filename);
#elif defined(PLATFORM_WEB)
    return file_dialog_web_save(dlg, title, default_filename);
#else
    return file_dialog_desktop_save(dlg, title, default_filename);
#endif
}

int flint_file_dialog_load(FlintFileDialog *dlg, const char *title) {
    if(!dlg)
        return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_LOAD;

    if(title)
        strncpy(dlg->title, title, sizeof(dlg->title) - 1);

#if defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    return file_dialog_android_load(dlg, title);
#elif defined(PLATFORM_WEB)
    return file_dialog_web_load(dlg, title);
#else
    return file_dialog_desktop_load(dlg, title);
#endif
}

const char *flint_file_dialog_get_path(FlintFileDialog *dlg) {
    if(!dlg)
        return NULL;

    if(dlg->confirmed && dlg->result_path[0] != '\0')
        return dlg->result_path;

    return NULL;
}
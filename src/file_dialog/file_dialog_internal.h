#ifndef FILE_DIALOG_INTERNAL_H
#define FILE_DIALOG_INTERNAL_H

#include "file_dialog.h"
#include <dirent.h>
#include <limits.h>
#include <sys/stat.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#define FILE_DIALOG_ITEM_HEIGHT 28

typedef struct {
    char name[256];
    int is_dir;
    long size;
} DirEntry;

typedef struct {
    char current_dir[PATH_MAX];
    char selected_file[256];
    char filename_input[256];
    char extension_filter[64];
    int file_count;
    DirEntry *entries;
    int scroll_offset;
    int hover_index;
    int last_clicked_index;
    int focus_area;
    Rectangle breadcrumb_rect;
    Rectangle file_list_rect;
    Rectangle filename_rect;
    Rectangle button_ok_rect;
    Rectangle button_cancel_rect;
    Rectangle show_hidden_checkbox_rect;
    Rectangle scrollbar_rect;
    float last_click_time;
    float cursor_blink_time;
    int cursor_visible;
    int dialog_width;  /* Dynamic width */
    int dialog_height;  /* Dynamic height */
    int show_hidden_files;  /* 0 = hide, 1 = show */
} FileDialogInternal;

/* File system operations */
static int scan_directory(FileDialogInternal *internal);
static int compare_entries(const void *a, const void *b);
static void navigate_up(FileDialogInternal *internal);
static void navigate_into(FileDialogInternal *internal, const char *dirname);
static void navigate_to_start_dir(FileDialogInternal *internal);

/* Rendering functions */
static void render_file_dialog(FileDialog *dlg, Vector2 screen_size);
static void render_header(FileDialog *dlg, Rectangle dialog_rect);
static void render_breadcrumb(FileDialog *dlg, Rectangle dialog_rect);
static void render_file_list(FileDialog *dlg, Rectangle dialog_rect);
static void render_filename_input(FileDialog *dlg, Rectangle dialog_rect);
static void render_buttons(FileDialog *dlg, Rectangle dialog_rect);
static void render_scrollbar(FileDialog *dlg, Rectangle dialog_rect);

/* Event handling */
static int handle_mouse_input(FileDialog *dlg);
static int handle_keyboard_input(FileDialog *dlg);
static void confirm_dialog(FileDialog *dlg);
static void cancel_dialog(FileDialog *dlg);

#endif

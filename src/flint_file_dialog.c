#include "flint_file_dialog_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pwd.h>
#include <unistd.h>
#include <time.h>

#include "flint_scaling.h"
#include "flint_color.h"
#include "flint_theme.h"
#include "flint_ui.h"

#ifndef MIN
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#endif

static FlintFileDialogInternal *create_internal(void) {
    FlintFileDialogInternal *internal = calloc(1, sizeof(FlintFileDialogInternal));
    return internal;
}

static int compare_entries(const void *a, const void *b) {
    const DirEntry *ea = (const DirEntry *)a;
    const DirEntry *eb = (const DirEntry *)b;

    if(ea->is_dir && !eb->is_dir) return -1;
    if(!ea->is_dir && eb->is_dir) return 1;

    return strcmp(ea->name, eb->name);
}

static int scan_directory(FlintFileDialogInternal *internal) {
    DIR *dir = opendir(internal->current_dir);
    if(!dir) return 0;

    struct dirent *entry;
    int count = 0;

    while((entry = readdir(dir))) {
        if(strcmp(entry->d_name, ".") == 0) continue;
        if(strcmp(entry->d_name, "..") == 0) continue;
        count++;
    }
    rewinddir(dir);

    if(internal->entries) {
        free(internal->entries);
        internal->entries = NULL;
    }

    if(count > 0) {
        internal->entries = malloc(count * sizeof(DirEntry));
    }
    internal->file_count = 0;

    while((entry = readdir(dir))) {
        if(strcmp(entry->d_name, ".") == 0) continue;
        if(strcmp(entry->d_name, "..") == 0) continue;

        strncpy(internal->entries[internal->file_count].name,
                entry->d_name, 255);
        internal->entries[internal->file_count].name[255] = '\0';

        char full_path[PATH_MAX];
        snprintf(full_path, sizeof(full_path), "%s/%s",
                internal->current_dir, entry->d_name);

        struct stat st;
        if(stat(full_path, &st) == 0) {
            internal->entries[internal->file_count].is_dir = S_ISDIR(st.st_mode);
            internal->entries[internal->file_count].size = st.st_size;
        } else {
            internal->entries[internal->file_count].is_dir = 0;
            internal->entries[internal->file_count].size = 0;
        }

        internal->file_count++;
    }
    closedir(dir);

    if(internal->file_count > 0) {
        qsort(internal->entries, internal->file_count,
              sizeof(DirEntry), compare_entries);
    }

    return 1;
}

static void navigate_home(FlintFileDialogInternal *internal) {
    const char *home = getenv("HOME");
    if(home) {
        strncpy(internal->current_dir, home, PATH_MAX - 1);
        internal->current_dir[PATH_MAX - 1] = '\0';
        scan_directory(internal);
    }
}

static void navigate_up(FlintFileDialogInternal *internal) {
    char *last_slash = strrchr(internal->current_dir, '/');
    if(last_slash && last_slash != internal->current_dir) {
        *last_slash = '\0';
        scan_directory(internal);
    }
}

static void navigate_into(FlintFileDialogInternal *internal, const char *dirname) {
    char new_path[PATH_MAX];
    snprintf(new_path, sizeof(new_path), "%s/%s",
            internal->current_dir, dirname);

    struct stat st;
    if(stat(new_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        strncpy(internal->current_dir, new_path, PATH_MAX - 1);
        internal->current_dir[PATH_MAX - 1] = '\0';
        scan_directory(internal);
    }
}

static void render_header(FlintFileDialog *dlg, Rectangle dialog_rect) {
    const char *title = dlg->title;
    int title_font = flint_ui_font();
    Color title_color = flint_theme_get("global", "text");

    int title_x = dialog_rect.x + flint_px(16);
    int title_y = dialog_rect.y + flint_px(12);

    DrawText(title, title_x, title_y, title_font, title_color);
}

static void render_breadcrumb(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int breadcrumb_y = dialog_rect.y + flint_px(40);
    int breadcrumb_x = dialog_rect.x + flint_px(16);
    int breadcrumb_height = flint_px(24);

    internal->breadcrumb_rect = (Rectangle){
        breadcrumb_x, breadcrumb_y,
        FILE_DIALOG_WIDTH - flint_px(32), breadcrumb_height
    };

    Color bg = flint_theme_get("global", "input_bg");
    DrawRectangleRec(internal->breadcrumb_rect, bg);

    Color border = flint_theme_get("global", "border");
    DrawRectangleLinesEx(internal->breadcrumb_rect, 1, border);

    Color text = flint_theme_get("global", "text");
    int font = flint_ui_font_small();

    char display_path[PATH_MAX];
    strncpy(display_path, internal->current_dir, PATH_MAX - 1);
    display_path[PATH_MAX - 1] = '\0';

    int text_width = MeasureText(display_path, font);
    int max_width = internal->breadcrumb_rect.width - flint_px(8);

    if(text_width > max_width) {
        int ellipsis_width = MeasureText("...", font);
        int available = max_width - ellipsis_width;

        while(text_width > available && strlen(display_path) > 3) {
            memmove(display_path, display_path + 1, strlen(display_path));
            text_width = MeasureText(display_path, font);
        }

        if(strlen(display_path) > 0) {
            memmove(display_path + 3, display_path, strlen(display_path) + 1);
            display_path[0] = '.';
            display_path[1] = '.';
            display_path[2] = '.';
        }
    }

    DrawText(display_path,
             breadcrumb_x + flint_px(4),
             breadcrumb_y + flint_px(4),
             font, text);
}

static void render_file_list(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int list_y = dialog_rect.y + flint_px(70);
    int list_x = dialog_rect.x + flint_px(16);
    int list_width = FILE_DIALOG_WIDTH - flint_px(32);
    int list_height = flint_px(280);

    internal->file_list_rect = (Rectangle){
        list_x, list_y, list_width, list_height
    };

    Color bg = flint_theme_get("global", "list_bg");
    if(bg.a == 0) bg = (Color){245, 245, 245, 255};
    DrawRectangleRec(internal->file_list_rect, bg);

    Color border = flint_theme_get("global", "border");
    DrawRectangleLinesEx(internal->file_list_rect, 1, border);

    BeginScissorMode(list_x, list_y, list_width, list_height);

    int font = flint_ui_font_small();
    Color text = flint_theme_get("global", "text");
    Color hover = flint_theme_get("global", "hover");
    if(hover.a == 0) hover = (Color){200, 200, 200, 255};
    Color selected = flint_theme_get("global", "selected");
    if(selected.a == 0) selected = (Color){100, 150, 255, 255};

    int start_idx = internal->scroll_offset;
    int visible_items = (int)(list_height / FILE_DIALOG_ITEM_HEIGHT);
    int end_idx = MIN(start_idx + visible_items, internal->file_count);

    for(int i = start_idx; i < end_idx; i++) {
        int item_y = list_y + (i - start_idx) * FILE_DIALOG_ITEM_HEIGHT;
        Rectangle item_rect = {list_x, item_y, list_width, FILE_DIALOG_ITEM_HEIGHT};

        if(i == internal->hover_index) {
            DrawRectangleRec(item_rect, hover);
        }

        if(strcmp(internal->entries[i].name, internal->selected_file) == 0) {
            DrawRectangleRec(item_rect, selected);
        }

        const char *icon = internal->entries[i].is_dir ? "📁" : "📄";
        DrawText(icon, list_x + flint_px(4), item_y + flint_px(4),
                font, text);

        DrawText(internal->entries[i].name,
                list_x + flint_px(24), item_y + flint_px(4),
                font, text);
    }

    EndScissorMode();
}

static void render_filename_input(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int input_y = dialog_rect.y + FILE_DIALOG_HEIGHT - flint_px(64);
    int input_x = dialog_rect.x + flint_px(16);
    int input_width = FILE_DIALOG_WIDTH - flint_px(32);
    int input_height = flint_px(24);

    internal->filename_rect = (Rectangle){
        input_x, input_y, input_width, input_height
    };

    Color bg = flint_theme_get("global", "input_bg");
    DrawRectangleRec(internal->filename_rect, bg);

    Color border = flint_theme_get("global", "border");
    Color accent = flint_theme_get("global", "accent");
    if(accent.a == 0) accent = (Color){100, 150, 255, 255};

    if(internal->focus_area == 2) {
        DrawRectangleLinesEx(internal->filename_rect, 2, accent);
    } else {
        DrawRectangleLinesEx(internal->filename_rect, 1, border);
    }

    Color text = flint_theme_get("global", "text");
    Color label_dim = flint_darken(text, 40);
    int font = flint_ui_font_small();
    DrawText("Filename:", input_x, input_y - flint_px(12), font, label_dim);

    DrawText(internal->filename_input,
            input_x + flint_px(4), input_y + flint_px(4),
            font, text);

    if(internal->focus_area == 2) {
        float current_time = GetTime();
        if(current_time - internal->cursor_blink_time > 0.5) {
            internal->cursor_visible = !internal->cursor_visible;
            internal->cursor_blink_time = current_time;
        }

        if(internal->cursor_visible) {
            int cursor_x = input_x + flint_px(4) +
                          MeasureText(internal->filename_input, font);
            DrawLine(cursor_x, input_y + flint_px(4),
                    cursor_x, input_y + input_height - flint_px(4), text);
        }
    } else {
        internal->cursor_visible = 0;
    }
}

static void render_buttons(FlintFileDialog *dlg, Rectangle dialog_rect) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    int button_y = dialog_rect.y + FILE_DIALOG_HEIGHT - flint_px(32);
    int button_width = flint_px(80);
    int button_height = flint_px(24);

    internal->button_cancel_rect = (Rectangle){
        dialog_rect.x + FILE_DIALOG_WIDTH - flint_px(176),
        button_y, button_width, button_height
    };

    internal->button_ok_rect = (Rectangle){
        dialog_rect.x + FILE_DIALOG_WIDTH - flint_px(88),
        button_y, button_width, button_height
    };

    Color cancel_bg = flint_theme_get("global", "button_bg");
    if(cancel_bg.a == 0) cancel_bg = (Color){220, 220, 220, 255};
    Color cancel_text = flint_theme_get("global", "button_text");
    if(cancel_text.a == 0) cancel_text = BLACK;

    DrawRectangleRec(internal->button_cancel_rect, cancel_bg);
    DrawRectangleLinesEx(internal->button_cancel_rect, 1,
                        flint_theme_get("global", "border"));
    DrawText("Cancel",
            internal->button_cancel_rect.x + flint_px(8),
            internal->button_cancel_rect.y + flint_px(4),
            flint_ui_font_small(), cancel_text);

    Color ok_bg = flint_theme_get("global", "button_primary_bg");
    if(ok_bg.a == 0) ok_bg = (Color){100, 150, 255, 255};
    Color ok_text = flint_theme_get("global", "button_primary_text");
    if(ok_text.a == 0) ok_text = WHITE;

    DrawRectangleRec(internal->button_ok_rect, ok_bg);
    DrawRectangleLinesEx(internal->button_ok_rect, 1,
                        flint_theme_get("global", "border"));

    const char *ok_text_str = (dlg->mode == FLINT_FILE_DIALOG_SAVE) ? "Save" : "Open";
    DrawText(ok_text_str,
            internal->button_ok_rect.x + flint_px(8),
            internal->button_ok_rect.y + flint_px(4),
            flint_ui_font_small(), ok_text);
}

static void render_file_dialog(FlintFileDialog *dlg, Vector2 screen_size) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    Rectangle dialog_rect = {
        (screen_size.x - FILE_DIALOG_WIDTH) / 2,
        (screen_size.y - FILE_DIALOG_HEIGHT) / 2,
        FILE_DIALOG_WIDTH,
        FILE_DIALOG_HEIGHT
    };

    DrawRectangle(0, 0, screen_size.x, screen_size.y, (Color){0, 0, 0, 180});

    Color bg_color = flint_theme_get("global", "window_bg");
    if(bg_color.a == 0) bg_color = WHITE;
    DrawRectangleRec(dialog_rect, bg_color);

    Color border_color = flint_theme_get("global", "border");
    DrawRectangleLinesEx(dialog_rect, 2, border_color);

    render_header(dlg, dialog_rect);
    render_breadcrumb(dlg, dialog_rect);
    render_file_list(dlg, dialog_rect);
    render_filename_input(dlg, dialog_rect);
    render_buttons(dlg, dialog_rect);
}

static int handle_mouse_input(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    Vector2 mouse = GetMousePosition();
    float wheel = GetMouseWheelMove();

    if(CheckCollisionPointRec(mouse, internal->file_list_rect)) {
        int hover_idx = internal->scroll_offset +
                       (int)((mouse.y - internal->file_list_rect.y) / FILE_DIALOG_ITEM_HEIGHT);

        if(hover_idx >= 0 && hover_idx < internal->file_count) {
            internal->hover_index = hover_idx;

            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                float current_time = GetTime();
                if(current_time - internal->last_click_time < 0.3) {
                    if(internal->entries[hover_idx].is_dir) {
                        navigate_into(internal, internal->entries[hover_idx].name);
                        internal->hover_index = -1;
                    } else {
                        strncpy(internal->selected_file,
                               internal->entries[hover_idx].name, 255);
                        internal->selected_file[255] = '\0';
                        strncpy(internal->filename_input,
                               internal->entries[hover_idx].name, 255);
                        internal->filename_input[255] = '\0';
                        confirm_dialog(dlg);
                    }
                    internal->last_click_time = 0;
                } else {
                    internal->last_click_time = current_time;
                }
            }
        }
    }

    if(wheel != 0) {
        internal->scroll_offset -= (int)wheel;
        int visible_items = (int)(internal->file_list_rect.height / FILE_DIALOG_ITEM_HEIGHT);
        internal->scroll_offset = MAX(0,
            MIN(internal->scroll_offset,
                MAX(0, internal->file_count - visible_items)));
    }

    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        if(CheckCollisionPointRec(mouse, internal->button_cancel_rect)) {
            cancel_dialog(dlg);
            return 1;
        }

        if(CheckCollisionPointRec(mouse, internal->button_ok_rect)) {
            if(internal->filename_input[0] != '\0') {
                confirm_dialog(dlg);
            }
            return 1;
        }

        if(CheckCollisionPointRec(mouse, internal->breadcrumb_rect)) {
            navigate_up(internal);
            return 1;
        }

        if(CheckCollisionPointRec(mouse, internal->filename_rect)) {
            internal->focus_area = 2;
            return 1;
        }
    }

    return 0;
}

static int handle_keyboard_input(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    int key = GetKeyPressed();

    switch(key) {
        case KEY_ESCAPE:
            cancel_dialog(dlg);
            return 1;

        case KEY_ENTER:
            if(internal->filename_input[0] != '\0') {
                confirm_dialog(dlg);
            }
            return 1;

        case KEY_UP:
            if(internal->file_count > 0) {
                internal->hover_index = MAX(0, internal->hover_index - 1);
                if(internal->hover_index < internal->scroll_offset) {
                    internal->scroll_offset = internal->hover_index;
                }

                if(internal->hover_index >= 0 && internal->hover_index < internal->file_count) {
                    strncpy(internal->selected_file, internal->entries[internal->hover_index].name, 255);
                    internal->selected_file[255] = '\0';
                    strncpy(internal->filename_input, internal->selected_file, 255);
                    internal->filename_input[255] = '\0';
                }
            }
            return 1;

        case KEY_DOWN:
            if(internal->file_count > 0) {
                int visible_items = (int)(internal->file_list_rect.height / FILE_DIALOG_ITEM_HEIGHT);
                internal->hover_index = MIN(internal->file_count - 1,
                                          internal->hover_index + 1);
                if(internal->hover_index >= internal->scroll_offset + visible_items) {
                    internal->scroll_offset = internal->hover_index - visible_items + 1;
                }

                if(internal->hover_index >= 0 && internal->hover_index < internal->file_count) {
                    strncpy(internal->selected_file, internal->entries[internal->hover_index].name, 255);
                    internal->selected_file[255] = '\0';
                    strncpy(internal->filename_input, internal->selected_file, 255);
                    internal->filename_input[255] = '\0';
                }
            }
            return 1;

        case KEY_HOME:
            navigate_home(internal);
            return 1;
    }

    if(internal->focus_area == 2) {
        int char_key = GetCharPressed();
        while(char_key > 0) {
            size_t len = strlen(internal->filename_input);
            if(len < sizeof(internal->filename_input) - 1) {
                internal->filename_input[len] = (char)char_key;
                internal->filename_input[len + 1] = '\0';
            }
            char_key = GetCharPressed();
        }

        if(IsKeyPressed(KEY_BACKSPACE)) {
            size_t len = strlen(internal->filename_input);
            if(len > 0) {
                internal->filename_input[len - 1] = '\0';
            }
        }
    }

    return 0;
}

static void confirm_dialog(FlintFileDialog *dlg) {
    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;

    snprintf(dlg->result_path, FLINT_FILE_DIALOG_PATH_MAX, "%s/%s",
            internal->current_dir, internal->filename_input);

    dlg->confirmed = 1;
    dlg->active = 0;
}

static void cancel_dialog(FlintFileDialog *dlg) {
    dlg->confirmed = 0;
    dlg->active = 0;
    dlg->result_path[0] = '\0';
}

void flint_file_dialog_init(FlintFileDialog *dlg) {
    if(!dlg) return;

    memset(dlg, 0, sizeof(FlintFileDialog));
    dlg->_internal = create_internal();
    dlg->mode = FLINT_FILE_DIALOG_LOAD;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    navigate_home(internal);
}

int flint_file_dialog_load(FlintFileDialog *dlg, const char *title) {
    if(!dlg || !title) return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_LOAD;
    strncpy(dlg->title, title, sizeof(dlg->title) - 1);
    dlg->title[sizeof(dlg->title) - 1] = '\0';
    strncpy(dlg->filter, "*.zip", sizeof(dlg->filter) - 1);
    dlg->filter[sizeof(dlg->filter) - 1] = '\0';
    dlg->active = 1;
    dlg->confirmed = 0;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    internal->focus_area = 0;

    while(dlg->active && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Vector2 screen_size = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        render_file_dialog(dlg, screen_size);

        handle_mouse_input(dlg);
        handle_keyboard_input(dlg);

        EndDrawing();
    }

    return dlg->confirmed;
}

int flint_file_dialog_save(FlintFileDialog *dlg, const char *title, const char *default_filename) {
    if(!dlg || !title || !default_filename) return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_SAVE;
    strncpy(dlg->title, title, sizeof(dlg->title) - 1);
    dlg->title[sizeof(dlg->title) - 1] = '\0';
    strncpy(dlg->default_name, default_filename, sizeof(dlg->default_name) - 1);
    dlg->default_name[sizeof(dlg->default_name) - 1] = '\0';
    strncpy(dlg->filter, "*.zip", sizeof(dlg->filter) - 1);
    dlg->filter[sizeof(dlg->filter) - 1] = '\0';
    dlg->active = 1;
    dlg->confirmed = 0;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    strncpy(internal->filename_input, default_filename, 255);
    internal->filename_input[255] = '\0';
    internal->focus_area = 2;

    while(dlg->active && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Vector2 screen_size = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        render_file_dialog(dlg, screen_size);

        handle_mouse_input(dlg);
        handle_keyboard_input(dlg);

        EndDrawing();
    }

    return dlg->confirmed;
}

int flint_file_dialog_select_folder(FlintFileDialog *dlg, const char *title) {
    if(!dlg || !title) return 0;

    flint_file_dialog_init(dlg);
    dlg->mode = FLINT_FILE_DIALOG_SELECT_FOLDER;
    strncpy(dlg->title, title, sizeof(dlg->title) - 1);
    dlg->title[sizeof(dlg->title) - 1] = '\0';
    dlg->active = 1;
    dlg->confirmed = 0;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    internal->focus_area = 0;

    while(dlg->active && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(RAYWHITE);

        Vector2 screen_size = {(float)GetScreenWidth(), (float)GetScreenHeight()};
        render_file_dialog(dlg, screen_size);

        handle_mouse_input(dlg);
        handle_keyboard_input(dlg);

        EndDrawing();
    }

    if(dlg->confirmed) {
        FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
        strncpy(dlg->result_path, internal->current_dir, FLINT_FILE_DIALOG_PATH_MAX - 1);
        dlg->result_path[FLINT_FILE_DIALOG_PATH_MAX - 1] = '\0';
    }

    return dlg->confirmed;
}

const char *flint_file_dialog_get_path(FlintFileDialog *dlg) {
    if(!dlg) return NULL;

    if(dlg->confirmed && dlg->result_path[0] != '\0')
        return dlg->result_path;

    return NULL;
}

void flint_file_dialog_cleanup(FlintFileDialog *dlg) {
    if(!dlg) return;

    FlintFileDialogInternal *internal = (FlintFileDialogInternal *)dlg->_internal;
    if(internal) {
        if(internal->entries) {
            free(internal->entries);
        }
        free(internal);
        dlg->_internal = NULL;
    }
}
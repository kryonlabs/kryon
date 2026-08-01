#ifndef KRYON_KI_INTERNAL_H
#define KRYON_KI_INTERNAL_H

#include "file_dialog.h"
#include "kryon.h"
#include "ui.h"
#include "ui_internal.h"

enum {
    EDITOR_PATH_CAP = FILE_DIALOG_PATH_MAX,
    EDITOR_MAX_RECENT_PROJECTS = 12,
    EDITOR_MAX_TREE_ENTRIES = 256,
    EDITOR_MAX_TREE_ITEMS = 1024,
    EDITOR_MAX_EXPANDED_DIRS = 256,
    EDITOR_MAX_RUN_TARGETS = 16,
    EDITOR_MAX_PREVIEW_SCENES = 128,
    EDITOR_MAX_OPEN_FILES = 12,
    EDITOR_MAX_SEARCH_RESULTS = 160,
    EDITOR_MAX_DIAGNOSTICS = 128,
    EDITOR_HISTORY_MAX = 32,
    EDITOR_CONSOLE_HISTORY_MAX = 32,
    EDITOR_TREE_DEPTH = 8,
    EDITOR_SOURCE_MAX_BYTES = 512 * 1024,
    EDITOR_OUTPUT_MAX_BYTES = 128 * 1024,
    EDITOR_MAX_EXAMPLES = 64,
    EDITOR_EXAMPLE_TITLE_CAP = 96,
};

typedef enum EditorLayoutMode {
    EDITOR_LAYOUT_SPLIT,
    EDITOR_LAYOUT_SOURCE,
    EDITOR_LAYOUT_PREVIEW
} EditorLayoutMode;

typedef enum EditorBottomPanelMode {
    EDITOR_BOTTOM_PANEL_OUTPUT,
    EDITOR_BOTTOM_PANEL_PROBLEMS,
    EDITOR_BOTTOM_PANEL_CONSOLE
} EditorBottomPanelMode;

typedef enum EditorPreviewScaleMode {
    EDITOR_PREVIEW_SCALE_FIT,
    EDITOR_PREVIEW_SCALE_100,
    EDITOR_PREVIEW_SCALE_75,
    EDITOR_PREVIEW_SCALE_50,
    EDITOR_PREVIEW_SCALE_CUSTOM
} EditorPreviewScaleMode;

typedef struct EditorRunTarget {
    char label[48];
    char name[48];
    char command[EDITOR_PATH_CAP];
} EditorRunTarget;

typedef struct EditorPreviewScene {
    char id[64];
    char group[64];
    char title[128];
    char source_path[EDITOR_PATH_CAP];
} EditorPreviewScene;

typedef struct EditorOpenFile {
    char path[EDITOR_PATH_CAP];
    int cursor;
    int scroll_y;
} EditorOpenFile;

typedef struct EditorSearchResult {
    char path[EDITOR_PATH_CAP];
    int line;
    char excerpt[160];
} EditorSearchResult;

typedef struct EditorDiagnostic {
    char path[EDITOR_PATH_CAP];
    int line;
    int column;
    char severity[16];
    char message[160];
} EditorDiagnostic;

typedef struct EditorProject {
    char path[EDITOR_PATH_CAP];
    char name[96];
    char selected_file[EDITOR_PATH_CAP];
    char source_scroll_file[EDITOR_PATH_CAP];
    int source_scroll_y;
    int source_cursor;
    int source_focused;
    int source_font_size;
    int source_pending_cursor;
    int source_pending_line;
    int source_pending_valid;
    int source_highlight_line;
    int source_dirty;
    int source_loaded;
    double source_last_edit_time;
    char source[EDITOR_SOURCE_MAX_BYTES];
    EditorOpenFile open_files[EDITOR_MAX_OPEN_FILES];
    int open_file_count;
    int active_open_file;
    char find_text[128];
    int find_cursor;
    int find_focused;
    char replace_text[128];
    int replace_cursor;
    int replace_focused;
    int find_replace_visible;
    int source_menu_open;   /* right-click context menu on the source editor */
    int source_menu_x;
    int source_menu_y;
    char *undo_items[EDITOR_HISTORY_MAX];
    char *redo_items[EDITOR_HISTORY_MAX];
    int undo_count;
    int redo_count;
    char image_file[EDITOR_PATH_CAP];
    Texture2D image_texture;
    RenderTexture2D preview_texture;
    int preview_texture_width;
    int preview_texture_height;
    char host_module_path[EDITOR_PATH_CAP];
    char host_module_rel_path[EDITOR_PATH_CAP];
    char app_host_command[EDITOR_PATH_CAP];
    int loaded;
    int selected_screen;
    EditorRunTarget run_targets[EDITOR_MAX_RUN_TARGETS];
    int run_target_count;
    int selected_run_target;
    long source_mtime;
    long host_module_mtime;
    double last_reload_check;
    long selected_file_mtime;
    double last_source_file_check;
    int source_external_change_reported;
    int reload_failed;
    int inspect_active;
    int inspect_menu_open;
    int inspect_menu_x;
    int inspect_menu_y;
    EditorLayoutMode layout_mode;
    int preview_interact;
    int preview_width;
    int preview_height;
    float preview_zoom;
    float preview_pan_x;
    float preview_pan_y;
    char preview_asset_root[EDITOR_PATH_CAP];
    EditorPreviewScene preview_scenes[EDITOR_MAX_PREVIEW_SCENES];
    int preview_scene_count;
    int preview_preset;
    EditorPreviewScaleMode preview_scale_mode;
    char search_text[128];
    int search_cursor;
    int search_focused;
    int search_visible;
    int search_scroll_y;
    EditorSearchResult search_results[EDITOR_MAX_SEARCH_RESULTS];
    int search_result_count;
    int selected_search_result;
    char output[EDITOR_OUTPUT_MAX_BYTES];
    int output_visible;
    EditorBottomPanelMode bottom_panel_mode;
    int output_scroll_y;
    char console_output[EDITOR_OUTPUT_MAX_BYTES];
    char console_input[512];
    int console_cursor;
    int console_focused;
    int console_scroll_y;
    int console_running;
    long console_pid;
    int console_fd;
    int console_exit_status;
    char console_command[512];
    char console_history[EDITOR_CONSOLE_HISTORY_MAX][512];
    int console_history_count;
    int console_history_index;
    /* Non-blocking build task (hot-reload, run targets). Mirrors the console
     * runner: fork + non-blocking pipe read, drained from the main loop. */
    int build_running;
    long build_pid;
    int build_fd;
    int build_exit_status;
    int build_is_host; /* 1: reload the app host via editor_load_host on done */
    char build_status[160];
    EditorDiagnostic diagnostics[EDITOR_MAX_DIAGNOSTICS];
    int diagnostic_count;
    int selected_diagnostic;
    double last_state_save;
    AppHost *host;
    void *host_library;
    DestroyAppHostCallback destroy_host;
} EditorProject;

typedef struct EditorRecentProjects {
    int count;
    char paths[EDITOR_MAX_RECENT_PROJECTS][EDITOR_PATH_CAP];
} EditorRecentProjects;

typedef struct EditorSidebarState {
    int scroll_y;
    int collapsed;
    int selected_id;
    int revealed_id;
    int expanded_ids[EDITOR_MAX_EXPANDED_DIRS];
    int expanded_count;
} EditorSidebarState;

typedef struct EditorTreeEntry {
    char name[128];
    char path[EDITOR_PATH_CAP];
    int is_dir;
} EditorTreeEntry;

typedef struct EditorTreeItem {
    UICascadingTreeItem item;
    char label[128];
    char path[EDITOR_PATH_CAP];
} EditorTreeItem;

const char *path_basename(const char *path);
void editor_select_file(EditorProject *project, const char *path);
int editor_select_source_path(EditorProject *project, const char *source_path);
int editor_remove_recent_project(EditorRecentProjects *recent, int index);
int editor_examples_dir(char *out, size_t out_size);
int editor_collect_examples(const char *examples_dir,
                            char paths[][EDITOR_PATH_CAP],
                            char titles[][EDITOR_EXAMPLE_TITLE_CAP],
                            int max_count);
void editor_open_project(EditorProject *project, const char *path,
                         EditorRecentProjects *recent, char *status,
                         size_t status_size);

void draw_start_page(Rectangle content, EditorProject *project,
                     EditorRecentProjects *recent,
                     EditorSidebarState *sidebar,
                     int *new_project_requested,
                     char *status, size_t status_size);

#endif

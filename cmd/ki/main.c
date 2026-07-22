#include "file_dialog.h"
#include "kryon.h"
#include "theme.h"
#include "ui.h"
#include "ui_internal.h"

#if !defined(_WIN32)
#include <dirent.h>
#include <dlfcn.h>
#include <sys/wait.h>
#include <unistd.h>
#endif
#include <sys/stat.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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
    EDITOR_MAX_KRY_SCREENS = 128,
    EDITOR_HISTORY_MAX = 32,
    EDITOR_TREE_DEPTH = 8,
    EDITOR_SOURCE_MAX_BYTES = 512 * 1024,
    EDITOR_OUTPUT_MAX_BYTES = 128 * 1024,
};

typedef enum EditorLayoutMode {
    EDITOR_LAYOUT_SPLIT,
    EDITOR_LAYOUT_SOURCE,
    EDITOR_LAYOUT_PREVIEW
} EditorLayoutMode;

typedef enum EditorPreviewScaleMode {
    EDITOR_PREVIEW_SCALE_FIT,
    EDITOR_PREVIEW_SCALE_100,
    EDITOR_PREVIEW_SCALE_75,
    EDITOR_PREVIEW_SCALE_50
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

typedef struct EditorKryScreen {
    char source_path[EDITOR_PATH_CAP];
    char header_path[EDITOR_PATH_CAP];
    char name[96];
    char title[128];
    int takes_viewport;
} EditorKryScreen;

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
    char *undo_items[EDITOR_HISTORY_MAX];
    char *redo_items[EDITOR_HISTORY_MAX];
    int undo_count;
    int redo_count;
    char image_file[EDITOR_PATH_CAP];
    Texture2D image_texture;
    char live_module_path[EDITOR_PATH_CAP];
    int loaded;
    int selected_screen;
    char live_module_rel_path[EDITOR_PATH_CAP];
    char build_live_command[EDITOR_PATH_CAP];
    int auto_live_build;
    EditorRunTarget run_targets[EDITOR_MAX_RUN_TARGETS];
    int run_target_count;
    int selected_run_target;
    long live_module_mtime;
    long source_mtime;
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
    int output_scroll_y;
    EditorDiagnostic diagnostics[EDITOR_MAX_DIAGNOSTICS];
    int diagnostic_count;
    int selected_diagnostic;
    AppHost *host;
    PreviewHost *preview_host;
    void *live_library;
    DestroyAppHostCallback destroy_live_host;
    DestroyPreviewHostCallback destroy_preview_host;
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

static int editor_save_source_file(EditorProject *project, char *status,
                                   size_t status_size);
static void editor_select_file(EditorProject *project, const char *path);
static void editor_search_project(EditorProject *project, char *status,
                                  size_t status_size);
static int editor_run_capture(EditorProject *project, const char *command,
                              char *status, size_t status_size);
static int editor_build_capture(EditorProject *project, const char *command,
                                const char *success, const char *failure,
                                char *status, size_t status_size);
static void editor_open_file_at_line(EditorProject *project, const char *path,
                                     int line_no, char *status,
                                     size_t status_size);

static void
load_editor_font(void)
{
    Font font;

    font = LoadUIFontAsset("fonts/noto/NotoSans-Regular.ttf",
                           UI_TEXT_BASE_SIZE);
    if(font.texture.id == 0)
        font = LoadUIFontAsset("../fonts/noto/NotoSans-Regular.ttf",
                               UI_TEXT_BASE_SIZE);
    if(font.texture.id == 0)
        font = LoadUIFontAsset("../kryon/fonts/noto/NotoSans-Regular.ttf",
                               UI_TEXT_BASE_SIZE);
    if(font.texture.id != 0) {
        RegisterUIFont("default", font);
        UseUIFont("default");
    }
}

static int
editor_top_bar_height(void)
{
    return ScaleUIPx(42);
}

static float
editor_ui_scale(void)
{
    return 1.0f;
}

static int
editor_bottom_bar_height(void)
{
    return ScaleUIPx(28);
}

static int
editor_canvas_header_height(void)
{
    return ScaleUIPx(38);
}

static int
editor_source_font_size(EditorProject *project)
{
    if(project == NULL)
        return UI_TEXT_16;
    if(project->source_font_size <= 0)
        project->source_font_size = UI_TEXT_16;
    if(project->source_font_size < 12)
        project->source_font_size = 12;
    if(project->source_font_size > 24)
        project->source_font_size = 24;
    return project->source_font_size;
}

static void
editor_preview_init(EditorProject *project)
{
    if(project == NULL)
        return;
    if(project->preview_width <= 0)
        project->preview_width = 800;
    if(project->preview_height <= 0)
        project->preview_height = 600;
    if(project->preview_width < 240)
        project->preview_width = 240;
    if(project->preview_width > 2560)
        project->preview_width = 2560;
    if(project->preview_height < 240)
        project->preview_height = 240;
    if(project->preview_height > 2560)
        project->preview_height = 2560;
    if(project->preview_scale_mode < EDITOR_PREVIEW_SCALE_FIT ||
       project->preview_scale_mode > EDITOR_PREVIEW_SCALE_50)
        project->preview_scale_mode = EDITOR_PREVIEW_SCALE_FIT;
    if(project->preview_preset < 0 || project->preview_preset > 4)
        project->preview_preset = 0;
}

static Rectangle
editor_preview_stage_rect(Rectangle content)
{
    int toolbar_h = ScaleUIPx(42);
    int pad = ScaleUIPx(18);

    return (Rectangle){
        content.x + (float)pad,
        content.y + (float)toolbar_h + (float)pad,
        content.width - (float)(pad * 2),
        content.height - (float)toolbar_h - (float)(pad * 2)
    };
}

static float
editor_preview_scale(const EditorProject *project, Rectangle stage)
{
    float scale_w;
    float scale_h;
    float scale;

    if(project == NULL || project->preview_width <= 0 ||
       project->preview_height <= 0)
        return 1.0f;
    if(project->preview_scale_mode == EDITOR_PREVIEW_SCALE_100)
        return 1.0f;
    if(project->preview_scale_mode == EDITOR_PREVIEW_SCALE_75)
        return 0.75f;
    if(project->preview_scale_mode == EDITOR_PREVIEW_SCALE_50)
        return 0.5f;
    scale_w = stage.width / (float)project->preview_width;
    scale_h = stage.height / (float)project->preview_height;
    scale = scale_w < scale_h ? scale_w : scale_h;
    if(scale > 1.0f)
        scale = 1.0f;
    if(scale < 0.1f)
        scale = 0.1f;
    return scale;
}

static Rectangle
editor_preview_device_rect(Rectangle content, const EditorProject *project)
{
    Rectangle stage = editor_preview_stage_rect(content);
    float scale = editor_preview_scale(project, stage);
    float w = project != NULL ? (float)project->preview_width * scale : 0.0f;
    float h = project != NULL ? (float)project->preview_height * scale : 0.0f;

    if(w <= 0.0f || h <= 0.0f)
        return (Rectangle){0};
    return (Rectangle){
        stage.x + (stage.width - w) * 0.5f,
        stage.y + (stage.height - h) * 0.5f,
        w,
        h
    };
}

static void
editor_preview_apply_preset(EditorProject *project)
{
    static const int sizes[][2] = {
        {800, 600},
        {1280, 720},
        {390, 844},
        {768, 1024},
        {600, 600}
    };

    if(project == NULL)
        return;
    editor_preview_init(project);
    if(project->preview_preset < 0 || project->preview_preset >= 5)
        return;
    project->preview_width = sizes[project->preview_preset][0];
    project->preview_height = sizes[project->preview_preset][1];
}

static const char *
path_basename(const char *path)
{
    const char *base = path;

    if(path == NULL || path[0] == '\0')
        return "";
    for(const char *p = path; *p != '\0'; p++) {
        if(*p == '/' && p[1] != '\0')
            base = p + 1;
    }
    return base;
}

static void
path_join(char *dst, size_t dst_size, const char *a, const char *b)
{
    if(dst == NULL || dst_size == 0)
        return;
    if(a == NULL || a[0] == '\0') {
        snprintf(dst, dst_size, "%s", b != NULL ? b : "");
        return;
    }
    if(b == NULL || b[0] == '\0') {
        snprintf(dst, dst_size, "%s", a);
        return;
    }
    if(a[strlen(a) - 1] == '/')
        snprintf(dst, dst_size, "%s%s", a, b);
    else
        snprintf(dst, dst_size, "%s/%s", a, b);
}

static void
path_dirname(char *dst, size_t dst_size, const char *path)
{
    char *slash;

    if(dst == NULL || dst_size == 0)
        return;
    if(path == NULL || path[0] == '\0') {
        snprintf(dst, dst_size, ".");
        return;
    }
    snprintf(dst, dst_size, "%s", path);
    slash = strrchr(dst, '/');
    if(slash == NULL) {
        snprintf(dst, dst_size, ".");
    } else if(slash == dst) {
        slash[1] = '\0';
    } else {
        *slash = '\0';
    }
}

static char *
editor_strdup(const char *text)
{
    size_t len;
    char *copy;

    if(text == NULL)
        text = "";
    len = strlen(text);
    copy = malloc(len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static void
editor_free_history(char **items, int *count)
{
    if(items == NULL || count == NULL)
        return;
    for(int i = 0; i < *count; i++) {
        free(items[i]);
        items[i] = NULL;
    }
    *count = 0;
}

static void
editor_push_history(char **items, int *count, const char *text)
{
    char *copy;

    if(items == NULL || count == NULL)
        return;
    if(*count > 0 && strcmp(items[*count - 1], text != NULL ? text : "") == 0)
        return;
    copy = editor_strdup(text);
    if(copy == NULL)
        return;
    if(*count >= EDITOR_HISTORY_MAX) {
        free(items[0]);
        memmove(items, items + 1, sizeof(items[0]) * (EDITOR_HISTORY_MAX - 1));
        *count = EDITOR_HISTORY_MAX - 1;
    }
    items[*count] = copy;
    (*count)++;
}

static int
editor_pop_history(char **items, int *count, char *dst, size_t dst_size)
{
    char *text;

    if(items == NULL || count == NULL || dst == NULL || dst_size == 0 ||
       *count <= 0)
        return 0;
    text = items[*count - 1];
    items[*count - 1] = NULL;
    (*count)--;
    snprintf(dst, dst_size, "%s", text != NULL ? text : "");
    free(text);
    return 1;
}

static const char *
path_extension(const char *path)
{
    const char *slash;
    const char *dot;

    if(path == NULL)
        return "";
    slash = strrchr(path, '/');
    dot = strrchr(path, '.');
    if(dot == NULL || (slash != NULL && dot < slash))
        return "";
    return dot;
}

static int
path_ext_eq(const char *path, const char *ext)
{
    const char *value = path_extension(path);

    if(value == NULL || ext == NULL)
        return 0;
    while(*value != '\0' && *ext != '\0') {
        char a = *value++;
        char b = *ext++;

        if(a >= 'A' && a <= 'Z')
            a = (char)(a - 'A' + 'a');
        if(b >= 'A' && b <= 'Z')
            b = (char)(b - 'A' + 'a');
        if(a != b)
            return 0;
    }
    return *value == '\0' && *ext == '\0';
}

static char *
editor_trim_line(char *s)
{
    char *end;

    if(s == NULL)
        return s;
    while(*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
        s++;
    end = s + strlen(s);
    while(end > s &&
          (end[-1] == ' ' || end[-1] == '\t' ||
           end[-1] == '\r' || end[-1] == '\n'))
        *--end = '\0';
    return s;
}

static int
editor_parse_quoted_value(char *line, const char *key, char *dst,
                          size_t dst_size)
{
    char *p;
    char *end;
    size_t key_len;

    if(line == NULL || key == NULL || dst == NULL || dst_size == 0)
        return 0;
    key_len = strlen(key);
    if(strncmp(line, key, key_len) != 0)
        return 0;
    p = line + key_len;
    if(*p != ' ' && *p != '\t')
        return 0;
    while(*p == ' ' || *p == '\t')
        p++;
    if(*p != '"')
        return 0;
    p++;
    end = strchr(p, '"');
    if(end == NULL)
        return 0;
    *end = '\0';
    snprintf(dst, dst_size, "%s", p);
    return 1;
}

static int
editor_parse_three_quoted_values(char *line, const char *key, char *a,
                                 size_t a_size, char *b, size_t b_size,
                                 char *c, size_t c_size)
{
    char *p;
    char *end;
    char *values[3] = {a, b, c};
    size_t sizes[3] = {a_size, b_size, c_size};
    size_t key_len;

    if(line == NULL || key == NULL || a == NULL || b == NULL || c == NULL)
        return 0;
    key_len = strlen(key);
    if(strncmp(line, key, key_len) != 0)
        return 0;
    p = line + key_len;
    if(*p != ' ' && *p != '\t')
        return 0;
    for(int i = 0; i < 3; i++) {
        while(*p == ' ' || *p == '\t')
            p++;
        if(*p != '"')
            return 0;
        p++;
        end = strchr(p, '"');
        if(end == NULL)
            return 0;
        *end = '\0';
        snprintf(values[i], sizes[i], "%s", p);
        p = end + 1;
    }
    return 1;
}

static int
editor_parse_four_quoted_values(char *line, const char *key, char *a,
                                size_t a_size, char *b, size_t b_size,
                                char *c, size_t c_size, char *d,
                                size_t d_size)
{
    char *p;
    char *end;
    char *values[4] = {a, b, c, d};
    size_t sizes[4] = {a_size, b_size, c_size, d_size};
    size_t key_len;

    if(line == NULL || key == NULL || a == NULL || b == NULL || c == NULL ||
       d == NULL)
        return 0;
    key_len = strlen(key);
    if(strncmp(line, key, key_len) != 0)
        return 0;
    p = line + key_len;
    if(*p != ' ' && *p != '\t')
        return 0;
    for(int i = 0; i < 4; i++) {
        while(*p == ' ' || *p == '\t')
            p++;
        if(*p != '"')
            return 0;
        p++;
        end = strchr(p, '"');
        if(end == NULL)
            return 0;
        *end = '\0';
        snprintf(values[i], sizes[i], "%s", p);
        p = end + 1;
    }
    return 1;
}

static int
editor_parse_two_ints(char *line, const char *key, int *a, int *b)
{
    char *p;
    size_t key_len;

    if(line == NULL || key == NULL || a == NULL || b == NULL)
        return 0;
    key_len = strlen(key);
    if(strncmp(line, key, key_len) != 0)
        return 0;
    p = line + key_len;
    if(*p != ' ' && *p != '\t')
        return 0;
    return sscanf(p, " %d %d", a, b) == 2;
}

static int
editor_project_has_make_target(const EditorProject *project, const char *target)
{
    char path[EDITOR_PATH_CAP];
    char line[1024];
    FILE *file;
    size_t target_len;

    if(project == NULL || target == NULL || target[0] == '\0')
        return 0;
    path_join(path, sizeof(path), project->path, "Makefile");
    file = fopen(path, "r");
    if(file == NULL)
        return 0;
    target_len = strlen(target);
    while(fgets(line, sizeof(line), file) != NULL) {
        char *trimmed = editor_trim_line(line);

        if(strncmp(trimmed, target, target_len) == 0 &&
           trimmed[target_len] == ':') {
            fclose(file);
            return 1;
        }
    }
    fclose(file);
    return 0;
}

static int
editor_project_has_kryon_app(const EditorProject *project)
{
    char path[EDITOR_PATH_CAP];

    if(project == NULL)
        return 0;
    path_join(path, sizeof(path), project->path,
              "vendor/kryon/scripts/kryon-app.sh");
    return FileExists(path);
}

static void
editor_add_run_target(EditorProject *project, const char *label,
                      const char *name, const char *command)
{
    EditorRunTarget *target;

    if(project == NULL || label == NULL || name == NULL || command == NULL ||
       project->run_target_count >= EDITOR_MAX_RUN_TARGETS)
        return;
    target = &project->run_targets[project->run_target_count++];
    snprintf(target->label, sizeof(target->label), "%s", label);
    snprintf(target->name, sizeof(target->name), "%s", name);
    snprintf(target->command, sizeof(target->command), "%s", command);
}

static void
editor_detect_run_targets(EditorProject *project)
{
    static const struct {
        const char *label;
        const char *name;
        const char *make_target;
        const char *kryon_command;
    } candidates[] = {
        {"Native", "native", "run", "run native"},
        {"Web", "web", "web", "build web"},
        {"Android Debug", "android-debug", "android-debug", "build android-debug"},
        {"Android Release", "android-release", "android-release", "build android-release"},
        {"Windows", "windows", "windows", "build windows"},
        {"AppImage", "appimage", "appimage", "package appimage"},
        {"Click", "click", "click", "package click"},
        {"Snap", "snap", "snap", "package snap"},
        {"Flatpak", "flatpak", "flatpak", "package flatpak"}
    };
    int has_kryon_app;

    if(project == NULL)
        return;
    project->selected_run_target = 0;
    if(project->run_target_count > 0)
        return;
    has_kryon_app = editor_project_has_kryon_app(project);
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if(editor_project_has_make_target(project, candidates[i].make_target)) {
            char command[EDITOR_PATH_CAP];

            if(has_kryon_app)
                snprintf(command, sizeof(command),
                         "sh vendor/kryon/scripts/kryon-app.sh %s",
                         candidates[i].kryon_command);
            else
                snprintf(command, sizeof(command), "make %s",
                         candidates[i].make_target);
            editor_add_run_target(project, candidates[i].label,
                                  candidates[i].name, command);
        }
    }
    if(project->run_target_count == 0)
        editor_add_run_target(project, "Native", "native", "make run");
}

static void
editor_load_project_config(EditorProject *project)
{
    char path[EDITOR_PATH_CAP];
    char line[1024];
    FILE *file;

    if(project == NULL)
        return;
    snprintf(project->live_module_rel_path,
             sizeof(project->live_module_rel_path),
             "build/kryon/live_preview.so");
    project->build_live_command[0] = '\0';
    project->auto_live_build = 1;
    project->run_target_count = 0;
    project->preview_scene_count = 0;
    project->preview_asset_root[0] = '\0';

    path_join(path, sizeof(path), project->path, "project.kryon");
    file = fopen(path, "r");
    if(file == NULL)
        return;
    while(fgets(line, sizeof(line), file) != NULL) {
        char *trimmed = editor_trim_line(line);
        char label[48];
        char name[48];
        char command[EDITOR_PATH_CAP];
        char title[128];
        char source_path[EDITOR_PATH_CAP];

        if(trimmed[0] == '\0' || trimmed[0] == '#')
            continue;
        if(editor_parse_quoted_value(trimmed, "name", project->name,
                                     sizeof(project->name)))
            continue;
        if(editor_parse_quoted_value(trimmed, "live",
                                     project->live_module_rel_path,
                                     sizeof(project->live_module_rel_path)))
            continue;
        if(editor_parse_quoted_value(trimmed, "host",
                                     project->live_module_rel_path,
                                     sizeof(project->live_module_rel_path)))
            continue;
        if(editor_parse_quoted_value(trimmed, "build_live",
                                     project->build_live_command,
                                     sizeof(project->build_live_command))) {
            project->auto_live_build = 0;
            continue;
        }
        if(editor_parse_quoted_value(trimmed, "build_host",
                                     project->build_live_command,
                                     sizeof(project->build_live_command))) {
            project->auto_live_build = 0;
            continue;
        }
        if(editor_parse_three_quoted_values(trimmed, "run_target",
                                            label, sizeof(label),
                                            name, sizeof(name),
                                            command, sizeof(command))) {
            editor_add_run_target(project, label, name, command);
            continue;
        }
        if(editor_parse_three_quoted_values(trimmed, "target",
                                            label, sizeof(label),
                                            name, sizeof(name),
                                            command, sizeof(command))) {
            editor_add_run_target(project, label, name, command);
            continue;
        }
        if(editor_parse_two_ints(trimmed, "preview_size",
                                 &project->preview_width,
                                 &project->preview_height))
            continue;
        if(editor_parse_quoted_value(trimmed, "preview_asset_root",
                                     project->preview_asset_root,
                                     sizeof(project->preview_asset_root)))
            continue;
        if(editor_parse_four_quoted_values(trimmed, "preview_scene",
                                           name, sizeof(name),
                                           label, sizeof(label),
                                           title, sizeof(title),
                                           source_path,
                                           sizeof(source_path))) {
            EditorPreviewScene *scene;

            if(project->preview_scene_count >= EDITOR_MAX_PREVIEW_SCENES)
                continue;
            scene = &project->preview_scenes[project->preview_scene_count++];
            snprintf(scene->id, sizeof(scene->id), "%s", name);
            snprintf(scene->group, sizeof(scene->group), "%s", label);
            snprintf(scene->title, sizeof(scene->title), "%s", title);
            snprintf(scene->source_path, sizeof(scene->source_path), "%s",
                     source_path);
            continue;
        }
    }
    fclose(file);
}

static int
editor_file_is_image(const char *path)
{
    return path_ext_eq(path, ".png") ||
           path_ext_eq(path, ".jpg") ||
           path_ext_eq(path, ".jpeg") ||
           path_ext_eq(path, ".bmp") ||
           path_ext_eq(path, ".qoi") ||
           path_ext_eq(path, ".tga");
}

static int
path_is_hidden(const char *name)
{
    return name != NULL && name[0] == '.';
}

static int
path_is_ignored_dir(const char *name)
{
    if(name == NULL)
        return 1;
    return strcmp(name, "build") == 0 ||
           strcmp(name, "vendor") == 0 ||
           strcmp(name, "vendor-builds") == 0 ||
           strcmp(name, "node_modules") == 0 ||
           strcmp(name, "tmp") == 0;
}

static int
editor_file_is_project_source(const char *name)
{
    const char *ext = path_extension(name);

    if(name == NULL)
        return 0;
    return strcmp(name, "Makefile") == 0 ||
           strcmp(name, "project.kryon") == 0 ||
           strcmp(ext, ".kry") == 0 ||
           strcmp(ext, ".c") == 0 ||
           strcmp(ext, ".h") == 0;
}

static long
editor_project_source_mtime_dir(const char *dir_path, int depth)
{
#if defined(_WIN32)
    (void)dir_path;
    (void)depth;
    return 0;
#else
    DIR *dir;
    struct dirent *entry;
    long newest = 0;

    if(dir_path == NULL || depth > EDITOR_TREE_DEPTH)
        return 0;
    dir = opendir(dir_path);
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL) {
        char path[EDITOR_PATH_CAP];
        struct stat st;

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           path_is_hidden(entry->d_name))
            continue;
        if(path_is_ignored_dir(entry->d_name))
            continue;
        path_join(path, sizeof(path), dir_path, entry->d_name);
        if(stat(path, &st) != 0)
            continue;
        if(S_ISDIR(st.st_mode)) {
            long child_mtime = editor_project_source_mtime_dir(path, depth + 1);

            if(child_mtime > newest)
                newest = child_mtime;
            continue;
        }
        if(editor_file_is_project_source(entry->d_name) &&
           (long)st.st_mtime > newest)
            newest = (long)st.st_mtime;
    }
    closedir(dir);
    return newest;
#endif
}

static long
editor_project_source_mtime(const EditorProject *project)
{
    if(project == NULL || !project->loaded)
        return 0;
    return editor_project_source_mtime_dir(project->path, 0);
}

static int
editor_tree_id(const char *path)
{
    unsigned int hash = 2166136261u;

    if(path == NULL || path[0] == '\0')
        return 1;
    for(const unsigned char *p = (const unsigned char *)path; *p != '\0'; p++) {
        hash ^= *p;
        hash *= 16777619u;
    }
    hash &= 0x7fffffffU;
    return hash == 0 ? 1 : (int)hash;
}

static int
tree_entry_compare(const void *a, const void *b)
{
    const EditorTreeEntry *ea = a;
    const EditorTreeEntry *eb = b;

    if(ea->is_dir != eb->is_dir)
        return eb->is_dir - ea->is_dir;
    return strcmp(ea->name, eb->name);
}

static int
editor_select_source_path(EditorProject *project, const char *source_path)
{
    int screen_count;

    if(project == NULL || project->host == NULL || source_path == NULL)
        return 0;
    screen_count = GetAppScreenCount(project->host);
    for(int i = 0; i < screen_count; i++) {
        AppScreenInfo screen = GetAppScreen(project->host, i);
        if(screen.source_path != NULL &&
           strcmp(screen.source_path, source_path) == 0) {
            project->selected_screen = i;
            if(SetAppScreenBySourcePath(project->host, source_path))
                return 1;
            SetAppScreen(project->host, i);
            return 1;
        }
    }
    return 0;
}

static int
editor_source_path_has_screen(const EditorProject *project, const char *source_path)
{
    int screen_count;

    if(project == NULL || project->host == NULL || source_path == NULL)
        return 0;
    screen_count = GetAppScreenCount((AppHost *)project->host);
    for(int i = 0; i < screen_count; i++) {
        AppScreenInfo screen = GetAppScreen((AppHost *)project->host, i);
        if(screen.source_path != NULL &&
           strcmp(screen.source_path, source_path) == 0)
            return 1;
    }
    return 0;
}

static int
editor_selected_file_has_preview(const EditorProject *project)
{
    if(project == NULL || !project->loaded || project->selected_file[0] == '\0')
        return project != NULL && project->host != NULL;
    return editor_source_path_has_screen(project, project->selected_file);
}

static const char *
editor_active_screen_source_path(EditorProject *project)
{
    AppScreenInfo screen;

    if(project == NULL || project->host == NULL)
        return NULL;
    screen = GetAppScreen(project->host, project->selected_screen);
    return screen.source_path;
}

static int
editor_open_active_screen_source(EditorProject *project)
{
    const char *source_path = editor_active_screen_source_path(project);

    if(project == NULL || source_path == NULL || source_path[0] == '\0')
        return 0;
    editor_select_file(project, source_path);
    editor_select_source_path(project, source_path);
    return 1;
}

static void
shell_quote(char *dst, size_t dst_size, const char *src)
{
    size_t n = 0;

    if(dst_size == 0)
        return;
    dst[n++] = '\'';
    while(src != NULL && *src != '\0' && n + 5 < dst_size) {
        if(*src == '\'') {
            dst[n++] = '\'';
            dst[n++] = '\\';
            dst[n++] = '\'';
            dst[n++] = '\'';
        } else {
            dst[n++] = *src;
        }
        src++;
    }
    if(n + 1 < dst_size)
        dst[n++] = '\'';
    dst[n] = '\0';
}

static int
editor_read_tree_entries(const char *dir_path, EditorTreeEntry *entries,
                         int max_entries)
{
#if defined(_WIN32)
    (void)dir_path;
    (void)entries;
    (void)max_entries;
    return 0;
#else
    DIR *dir;
    struct dirent *entry;
    int count = 0;

    dir = opendir(dir_path);
    if(dir == NULL)
        return 0;
    while(count < max_entries && (entry = readdir(dir)) != NULL) {
        struct stat st;
        EditorTreeEntry *item;

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           path_is_hidden(entry->d_name))
            continue;

        item = &entries[count];
        snprintf(item->name, sizeof(item->name), "%s", entry->d_name);
        snprintf(item->path, sizeof(item->path), "%s/%s",
                 dir_path, entry->d_name);
        item->is_dir = stat(item->path, &st) == 0 && S_ISDIR(st.st_mode);
        count++;
    }
    closedir(dir);
    qsort(entries, (size_t)count, sizeof(entries[0]), tree_entry_compare);
    return count;
#endif
}

static void
editor_relative_path(char *dst, size_t dst_size, const EditorProject *project,
                     const char *path)
{
    size_t root_len;

    if(dst_size == 0)
        return;
    dst[0] = '\0';
    if(project == NULL || path == NULL)
        return;
    root_len = strlen(project->path);
    if(strncmp(path, project->path, root_len) == 0 && path[root_len] == '/')
        snprintf(dst, dst_size, "%s", path + root_len + 1);
    else
        snprintf(dst, dst_size, "%s", path);
}

static UITextInputStyle
editor_input_style(void)
{
    return (UITextInputStyle){
        .background = GetThemeSurface(),
        .border = DarkenUIColor(GetThemeSurface(), 40),
        .focus_border = GetThemeButtonHover(),
        .text = GetThemeText(),
        .cursor = GetThemeText(),
        .radius = 0.0f
    };
}

static void
editor_recent_path(char *path, size_t path_size)
{
    const char *home = getenv("HOME");

    if(path_size == 0)
        return;
    if(home == NULL || home[0] == '\0')
        snprintf(path, path_size, ".kryon/recent_projects.txt");
    else
        snprintf(path, path_size, "%s/.kryon/recent_projects.txt", home);
}

static void
editor_load_recent_projects(EditorRecentProjects *recent)
{
    FILE *file;
    char path[EDITOR_PATH_CAP];
    char line[EDITOR_PATH_CAP];

    if(recent == NULL)
        return;
    memset(recent, 0, sizeof(*recent));
    editor_recent_path(path, sizeof(path));
    file = fopen(path, "r");
    if(file == NULL)
        return;

    while(recent->count < EDITOR_MAX_RECENT_PROJECTS &&
          fgets(line, sizeof(line), file) != NULL) {
        size_t len = strlen(line);
        while(len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';
        if(line[0] == '\0')
            continue;
        snprintf(recent->paths[recent->count],
                 sizeof(recent->paths[recent->count]), "%s", line);
        recent->count++;
    }
    fclose(file);
}

static void
editor_save_recent_projects(const EditorRecentProjects *recent)
{
    FILE *file;
    char path[EDITOR_PATH_CAP];
    char dir[EDITOR_PATH_CAP];
    char *slash;

    if(recent == NULL)
        return;
    editor_recent_path(path, sizeof(path));
    snprintf(dir, sizeof(dir), "%s", path);
    slash = strrchr(dir, '/');
    if(slash != NULL) {
        *slash = '\0';
        mkdir(dir, 0755);
    }

    file = fopen(path, "w");
    if(file == NULL)
        return;
    for(int i = 0; i < recent->count; i++)
        fprintf(file, "%s\n", recent->paths[i]);
    fclose(file);
}

static void
editor_add_recent_project(EditorRecentProjects *recent, const char *path)
{
    int existing = -1;

    if(recent == NULL || path == NULL || path[0] == '\0')
        return;
    for(int i = 0; i < recent->count; i++) {
        if(strcmp(recent->paths[i], path) == 0) {
            existing = i;
            break;
        }
    }
    if(existing < 0 && recent->count < EDITOR_MAX_RECENT_PROJECTS)
        existing = recent->count++;
    if(existing < 0)
        existing = EDITOR_MAX_RECENT_PROJECTS - 1;
    for(int i = existing; i > 0; i--)
        snprintf(recent->paths[i], sizeof(recent->paths[i]), "%s",
                 recent->paths[i - 1]);
    snprintf(recent->paths[0], sizeof(recent->paths[0]), "%s", path);
    editor_save_recent_projects(recent);
}

static int
editor_find_open_file(const EditorProject *project, const char *path)
{
    if(project == NULL || path == NULL || path[0] == '\0')
        return -1;
    for(int i = 0; i < project->open_file_count; i++) {
        if(strcmp(project->open_files[i].path, path) == 0)
            return i;
    }
    return -1;
}

static void
editor_store_active_open_file(EditorProject *project)
{
    int index;

    if(project == NULL || project->source_scroll_file[0] == '\0')
        return;
    index = editor_find_open_file(project, project->source_scroll_file);
    if(index < 0)
        return;
    project->open_files[index].cursor = project->source_cursor;
    project->open_files[index].scroll_y = project->source_scroll_y;
}

static int
editor_add_open_file(EditorProject *project, const char *path)
{
    int index;

    if(project == NULL || path == NULL || path[0] == '\0')
        return -1;
    index = editor_find_open_file(project, path);
    if(index >= 0)
        return index;
    if(project->open_file_count >= EDITOR_MAX_OPEN_FILES) {
        memmove(project->open_files, project->open_files + 1,
                sizeof(project->open_files[0]) *
                    (EDITOR_MAX_OPEN_FILES - 1));
        project->open_file_count = EDITOR_MAX_OPEN_FILES - 1;
        if(project->active_open_file > 0)
            project->active_open_file--;
    }
    index = project->open_file_count++;
    memset(&project->open_files[index], 0, sizeof(project->open_files[index]));
    snprintf(project->open_files[index].path,
             sizeof(project->open_files[index].path), "%s", path);
    return index;
}

static void
editor_select_file(EditorProject *project, const char *path)
{
    int index;

    if(project == NULL || path == NULL || path[0] == '\0')
        return;
    editor_store_active_open_file(project);
    index = editor_add_open_file(project, path);
    if(index >= 0)
        project->active_open_file = index;
    snprintf(project->selected_file, sizeof(project->selected_file), "%s",
             path);
    if(path_ext_eq(path, ".kry")) {
        project->layout_mode = EDITOR_LAYOUT_SPLIT;
        editor_select_source_path(project, path);
    }
}

static int
editor_create_new_file(EditorProject *project, char *status,
                       size_t status_size)
{
    char rel[64];
    char path[EDITOR_PATH_CAP];
    FILE *file;

    if(project == NULL || !project->loaded)
        return 0;
    for(int i = 1; i < 1000; i++) {
        snprintf(rel, sizeof(rel), "new%d.kry", i);
        path_join(path, sizeof(path), project->path, rel);
        if(FileExists(path))
            continue;
        file = fopen(path, "wb");
        if(file == NULL)
            break;
        fprintf(file, "screen \"%s\" {\n    text \"New screen\"\n}\n",
                rel);
        fclose(file);
        editor_select_file(project, rel);
        project->source_loaded = 0;
        snprintf(status, status_size, "Created %s", rel);
        return 1;
    }
    snprintf(status, status_size, "Could not create a new file");
    return 0;
}

static void
editor_output_clear(EditorProject *project)
{
    if(project == NULL)
        return;
    project->output[0] = '\0';
    project->output_scroll_y = 0;
    project->diagnostic_count = 0;
    project->selected_diagnostic = -1;
}

static void
editor_set_project(EditorProject *project, const char *path)
{
    if(project == NULL || path == NULL || path[0] == '\0')
        return;

    memset(project, 0, sizeof(*project));
    snprintf(project->path, sizeof(project->path), "%s", path);
    snprintf(project->name, sizeof(project->name), "%s", path_basename(path));
    project->loaded = 1;
    project->layout_mode = EDITOR_LAYOUT_SPLIT;
    project->preview_width = 800;
    project->preview_height = 600;
    project->preview_preset = 0;
    project->preview_scale_mode = EDITOR_PREVIEW_SCALE_FIT;
    project->selected_diagnostic = -1;
    project->selected_search_result = -1;
    editor_load_project_config(project);
    editor_detect_run_targets(project);
}

static void
editor_unload_live_module(EditorProject *project)
{
    if(project == NULL)
        return;
    if(project->destroy_preview_host != NULL && project->preview_host != NULL)
        project->destroy_preview_host(project->preview_host);
    if(project->destroy_live_host != NULL && project->host != NULL)
        project->destroy_live_host(project->host);
#if !defined(_WIN32)
    if(project->live_library != NULL)
        dlclose(project->live_library);
#endif
    project->host = NULL;
    project->preview_host = NULL;
    project->live_library = NULL;
    project->destroy_live_host = NULL;
    project->destroy_preview_host = NULL;
}

static void
editor_close_project(EditorProject *project)
{
    if(project == NULL)
        return;
    if(project->image_texture.id != 0)
        UnloadTexture(project->image_texture);
    editor_free_history(project->undo_items, &project->undo_count);
    editor_free_history(project->redo_items, &project->redo_count);
    editor_unload_live_module(project);
    memset(project, 0, sizeof(*project));
}

static int
editor_starts_word(const char *s, const char *word)
{
    size_t n;

    if(s == NULL || word == NULL)
        return 0;
    n = strlen(word);
    return strncmp(s, word, n) == 0 &&
           (s[n] == '\0' || s[n] == ' ' || s[n] == '\t' || s[n] == '(');
}

static void
editor_strip_kry_ext(char *dst, size_t dst_size, const char *path)
{
    size_t len;

    if(dst == NULL || dst_size == 0)
        return;
    if(path == NULL) {
        dst[0] = '\0';
        return;
    }
    len = strlen(path);
    if(len > 4 && strcmp(path + len - 4, ".kry") == 0)
        len -= 4;
    if(len >= dst_size)
        len = dst_size - 1;
    memcpy(dst, path, len);
    dst[len] = '\0';
}

static void
editor_make_screen_title(char *dst, size_t dst_size, const char *rel_path)
{
    char tmp[128];
    const char *base;
    char *dot;

    if(dst == NULL || dst_size == 0)
        return;
    base = path_basename(rel_path);
    snprintf(tmp, sizeof(tmp), "%s", base);
    dot = strrchr(tmp, '.');
    if(dot != NULL)
        *dot = '\0';
    if(tmp[0] >= '0' && tmp[0] <= '9' &&
       tmp[1] >= '0' && tmp[1] <= '9' && tmp[2] == '_')
        memmove(tmp, tmp + 3, strlen(tmp + 3) + 1);
    for(char *p = tmp; *p != '\0'; p++)
        if(*p == '_')
            *p = ' ';
    snprintf(dst, dst_size, "%s", tmp);
}

static void
editor_generated_header_path(char *dst, size_t dst_size, const char *rel_path)
{
    char base[EDITOR_PATH_CAP];

    editor_strip_kry_ext(base, sizeof(base), rel_path);
    snprintf(dst, dst_size, "%s.h", base);
}

static void
editor_generated_c_path(char *dst, size_t dst_size, const char *codegen_dir,
                        const char *rel_path)
{
    char base[EDITOR_PATH_CAP];
    char crel[EDITOR_PATH_CAP];

    editor_strip_kry_ext(base, sizeof(base), rel_path);
    snprintf(crel, sizeof(crel), "%s.c", base);
    path_join(dst, dst_size, codegen_dir, crel);
}

static int
editor_parse_kry_screen_line(char *line, EditorKryScreen *screen,
                             const char *rel_path)
{
    char *p;
    size_t n = 0;

    if(line == NULL || screen == NULL || rel_path == NULL)
        return 0;
    if(editor_starts_word(line, "screen"))
        p = editor_trim_line(line + strlen("screen"));
    else if(editor_starts_word(line, "preview"))
        p = editor_trim_line(line + strlen("preview"));
    else if(editor_starts_word(line, "page"))
        p = editor_trim_line(line + strlen("page"));
    else
        return 0;
    if(!((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') || *p == '_'))
        return 0;
    while((*p >= 'A' && *p <= 'Z') || (*p >= 'a' && *p <= 'z') ||
          (*p >= '0' && *p <= '9') || *p == '_') {
        if(n + 1 < sizeof(screen->name))
            screen->name[n++] = *p;
        p++;
    }
    screen->name[n] = '\0';
    snprintf(screen->source_path, sizeof(screen->source_path), "%s", rel_path);
    editor_generated_header_path(screen->header_path,
                                 sizeof(screen->header_path), rel_path);
    editor_make_screen_title(screen->title, sizeof(screen->title), rel_path);
    screen->takes_viewport = strstr(p, "Rectangle") != NULL;
    return screen->name[0] != '\0';
}

static int
editor_collect_kry_screens_file(const char *root, const char *rel_path,
                                EditorKryScreen *screens, int *count)
{
    char path[EDITOR_PATH_CAP];
    char line[1024];
    FILE *file;

    if(root == NULL || rel_path == NULL || screens == NULL || count == NULL ||
       *count >= EDITOR_MAX_KRY_SCREENS)
        return 0;
    path_join(path, sizeof(path), root, rel_path);
    file = fopen(path, "r");
    if(file == NULL)
        return 0;
    while(fgets(line, sizeof(line), file) != NULL &&
          *count < EDITOR_MAX_KRY_SCREENS) {
        char *trimmed = editor_trim_line(line);
        EditorKryScreen screen = {0};

        if(editor_parse_kry_screen_line(trimmed, &screen, rel_path))
            screens[(*count)++] = screen;
    }
    fclose(file);
    return 1;
}

static int
editor_collect_kry_screens_dir(const char *root, const char *rel_dir,
                               EditorKryScreen *screens, int *count,
                               int depth)
{
#if defined(_WIN32)
    (void)root;
    (void)rel_dir;
    (void)screens;
    (void)count;
    (void)depth;
    return 0;
#else
    char dir_path[EDITOR_PATH_CAP];
    DIR *dir;
    struct dirent *entry;

    if(root == NULL || screens == NULL || count == NULL ||
       depth > EDITOR_TREE_DEPTH || *count >= EDITOR_MAX_KRY_SCREENS)
        return 0;
    if(rel_dir == NULL || rel_dir[0] == '\0')
        snprintf(dir_path, sizeof(dir_path), "%s", root);
    else
        path_join(dir_path, sizeof(dir_path), root, rel_dir);
    dir = opendir(dir_path);
    if(dir == NULL)
        return 0;
    while((entry = readdir(dir)) != NULL && *count < EDITOR_MAX_KRY_SCREENS) {
        char child_rel[EDITOR_PATH_CAP];
        char child_path[EDITOR_PATH_CAP];
        struct stat st;

        if(strcmp(entry->d_name, ".") == 0 ||
           strcmp(entry->d_name, "..") == 0 ||
           path_is_hidden(entry->d_name) ||
           path_is_ignored_dir(entry->d_name))
            continue;
        if(rel_dir != NULL && rel_dir[0] != '\0')
            path_join(child_rel, sizeof(child_rel), rel_dir, entry->d_name);
        else
            snprintf(child_rel, sizeof(child_rel), "%s", entry->d_name);
        path_join(child_path, sizeof(child_path), root, child_rel);
        if(stat(child_path, &st) != 0)
            continue;
        if(S_ISDIR(st.st_mode)) {
            editor_collect_kry_screens_dir(root, child_rel, screens, count,
                                           depth + 1);
        } else if(path_ext_eq(child_rel, ".kry")) {
            editor_collect_kry_screens_file(root, child_rel, screens, count);
        }
    }
    closedir(dir);
    return *count > 0;
#endif
}

static int
editor_path_has_file(const char *root, const char *rel)
{
    char path[EDITOR_PATH_CAP];

    path_join(path, sizeof(path), root, rel);
    return FileExists(path);
}

static int
editor_find_kryon_root(const EditorProject *project, char *root,
                       size_t root_size)
{
    char cwd[EDITOR_PATH_CAP];
    char parent[EDITOR_PATH_CAP];
    char vendor[EDITOR_PATH_CAP];

    if(root == NULL || root_size == 0)
        return 0;
    if(getenv("KRYON_SDK_ROOT") != NULL &&
       editor_path_has_file(getenv("KRYON_SDK_ROOT"), "include/kryon.h")) {
        snprintf(root, root_size, "%s", getenv("KRYON_SDK_ROOT"));
        return 1;
    }
    if(getcwd(cwd, sizeof(cwd)) != NULL &&
       editor_path_has_file(cwd, "include/kryon.h")) {
        snprintf(root, root_size, "%s", cwd);
        return 1;
    }
    if(project != NULL) {
        snprintf(parent, sizeof(parent), "%s", project->path);
        char *slash = strrchr(parent, '/');
        if(slash != NULL && slash != parent) {
            *slash = '\0';
            if(editor_path_has_file(parent, "include/kryon.h")) {
                snprintf(root, root_size, "%s", parent);
                return 1;
            }
        }
        path_join(vendor, sizeof(vendor), project->path, "vendor/kryon");
        if(editor_path_has_file(vendor, "include/kryon.h")) {
            snprintf(root, root_size, "%s", vendor);
            return 1;
        }
    }
    return 0;
}

static void
editor_command_append(char *command, size_t command_size, const char *text)
{
    size_t len;

    if(command == NULL || text == NULL || command_size == 0)
        return;
    len = strlen(command);
    if(len + 1 >= command_size)
        return;
    snprintf(command + len, command_size - len, "%s", text);
}

static void
editor_command_append_quoted(char *command, size_t command_size,
                             const char *text)
{
    char quoted[EDITOR_PATH_CAP + 16];

    shell_quote(quoted, sizeof(quoted), text);
    editor_command_append(command, command_size, quoted);
}

static int
editor_write_auto_live_host(const char *host_path, const EditorKryScreen *screens,
                            int screen_count)
{
    FILE *file;

    file = fopen(host_path, "wb");
    if(file == NULL)
        return 0;
    fprintf(file, "#include \"kryon.h\"\n");
    fprintf(file, "#include <stdlib.h>\n\n");
    for(int i = 0; i < screen_count; i++)
        fprintf(file, "#include \"%s\"\n", screens[i].header_path);
    fprintf(file, "\ntypedef struct KryonLiveState {\n");
    fprintf(file, "    AppHost host;\n");
    fprintf(file, "    App app;\n");
    fprintf(file, "} KryonLiveState;\n\n");
    for(int i = 0; i < screen_count; i++) {
        fprintf(file, "static void\n");
        fprintf(file, "kryon_live_draw_%d(void *app, Rectangle viewport)\n", i);
        fprintf(file, "{\n");
        fprintf(file, "    (void)app;\n");
        if(screens[i].takes_viewport)
            fprintf(file, "    %s_kry_draw(viewport);\n", screens[i].name);
        else
            fprintf(file, "    %s_kry_draw();\n", screens[i].name);
        fprintf(file, "}\n\n");
    }
    fprintf(file, "static const AppScreen kryon_live_screens[] = {\n");
    for(int i = 0; i < screen_count; i++) {
        fprintf(file,
                "    {\"%s\", \"Examples\", \"%s\", \"%s\", 0, kryon_live_draw_%d}%s\n",
                screens[i].name, screens[i].title, screens[i].source_path, i,
                i + 1 < screen_count ? "," : "");
    }
    fprintf(file, "};\n\n");
    fprintf(file, "AppHost *\n");
    fprintf(file, "CreateKryonLivePreview(int abi_version, const char *project_path)\n");
    fprintf(file, "{\n");
    fprintf(file, "    KryonLiveState *state;\n");
    fprintf(file, "    (void)project_path;\n");
    fprintf(file, "    if(abi_version != APP_HOST_ABI_VERSION)\n");
    fprintf(file, "        return 0;\n");
    fprintf(file, "    state = calloc(1, sizeof(*state));\n");
    fprintf(file, "    if(state == 0)\n");
    fprintf(file, "        return 0;\n");
    fprintf(file, "    state->app.app = state;\n");
    fprintf(file, "    state->app.screens = kryon_live_screens;\n");
    fprintf(file, "    state->app.screen_count = %d;\n", screen_count);
    fprintf(file, "    BindAppHost(&state->app, &state->host);\n");
    fprintf(file, "    return &state->host;\n");
    fprintf(file, "}\n\n");
    fprintf(file, "void\n");
    fprintf(file, "DestroyKryonLivePreview(AppHost *host)\n");
    fprintf(file, "{\n");
    fprintf(file, "    App *app = host != 0 ? (App *)host->userdata : 0;\n");
    fprintf(file, "    KryonLiveState *state = app != 0 ? (KryonLiveState *)app->app : 0;\n");
    fprintf(file, "    free(state);\n");
    fprintf(file, "}\n");
    fclose(file);
    return 1;
}

static int
editor_build_auto_live_module(EditorProject *project, char *status,
                              size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Automatic Kryon preview build is not implemented on Windows yet");
    return 0;
#else
    EditorKryScreen screens[EDITOR_MAX_KRY_SCREENS];
    int screen_count = 0;
    char kryon_root[EDITOR_PATH_CAP];
    char kc_path[EDITOR_PATH_CAP];
    char build_dir[EDITOR_PATH_CAP];
    char codegen_dir[EDITOR_PATH_CAP];
    char host_path[EDITOR_PATH_CAP];
    char next_module_path[EDITOR_PATH_CAP];
    char command[EDITOR_OUTPUT_MAX_BYTES];

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }
    if(!editor_collect_kry_screens_dir(project->path, "", screens,
                                       &screen_count, 0) ||
       screen_count <= 0) {
        snprintf(status, status_size, "No previewable .kry screens found");
        return 0;
    }
    if(!editor_find_kryon_root(project, kryon_root, sizeof(kryon_root))) {
        snprintf(status, status_size, "Could not find Kryon SDK root for automatic preview");
        return 0;
    }
    path_join(build_dir, sizeof(build_dir), project->path, "build/kryon");
    path_join(codegen_dir, sizeof(codegen_dir), build_dir, "codegen");
    path_join(host_path, sizeof(host_path), build_dir, "live_preview_host.c");
    path_join(project->live_module_path, sizeof(project->live_module_path),
              project->path, project->live_module_rel_path);
    snprintf(next_module_path, sizeof(next_module_path), "%s.next",
             project->live_module_path);
    command[0] = '\0';
    editor_command_append(command, sizeof(command), "mkdir -p ");
    editor_command_append_quoted(command, sizeof(command), build_dir);
    if(system(command) != 0) {
        snprintf(status, status_size, "Could not create automatic preview build directory");
        return 0;
    }
    if(!editor_write_auto_live_host(host_path, screens, screen_count)) {
        snprintf(status, status_size, "Could not write automatic preview host");
        return 0;
    }
    path_join(kc_path, sizeof(kc_path), kryon_root, "build/bin/kc");
    if(!FileExists(kc_path))
        snprintf(kc_path, sizeof(kc_path), "kc");

    command[0] = '\0';
    editor_command_append(command, sizeof(command), "mkdir -p ");
    editor_command_append_quoted(command, sizeof(command), codegen_dir);
    editor_command_append(command, sizeof(command), " && ");
    editor_command_append(command, sizeof(command), "cd ");
    editor_command_append_quoted(command, sizeof(command), project->path);
    editor_command_append(command, sizeof(command), " && ");
    editor_command_append_quoted(command, sizeof(command), kc_path);
    editor_command_append(command, sizeof(command), " --no-main --root .");
    editor_command_append(command, sizeof(command), " -o ");
    editor_command_append_quoted(command, sizeof(command), codegen_dir);
    for(int i = 0; i < screen_count; i++) {
        editor_command_append(command, sizeof(command), " ");
        editor_command_append_quoted(command, sizeof(command), screens[i].source_path);
    }
    editor_command_append(command, sizeof(command), " && cc -shared -fPIC -DKRYON_LIVE_PREVIEW -I");
    editor_command_append_quoted(command, sizeof(command), kryon_root);
    editor_command_append(command, sizeof(command), "/include -I");
    editor_command_append_quoted(command, sizeof(command), project->path);
    editor_command_append(command, sizeof(command), " -I");
    editor_command_append_quoted(command, sizeof(command), codegen_dir);
    editor_command_append(command, sizeof(command), " -o ");
    editor_command_append_quoted(command, sizeof(command), next_module_path);
    editor_command_append(command, sizeof(command), " ");
    editor_command_append_quoted(command, sizeof(command), host_path);
    for(int i = 0; i < screen_count; i++) {
        char generated_c[EDITOR_PATH_CAP];

        editor_generated_c_path(generated_c, sizeof(generated_c), codegen_dir,
                                screens[i].source_path);
        editor_command_append(command, sizeof(command), " ");
        editor_command_append_quoted(command, sizeof(command), generated_c);
    }
    editor_command_append(command, sizeof(command), " && mv ");
    editor_command_append_quoted(command, sizeof(command), next_module_path);
    editor_command_append(command, sizeof(command), " ");
    editor_command_append_quoted(command, sizeof(command), project->live_module_path);
    if(!editor_build_capture(project, command,
                             "Built automatic Kryon preview",
                             "Could not build automatic Kryon preview",
                             status, status_size) ||
       !FileExists(project->live_module_path)) {
        if(status[0] == '\0')
            snprintf(status, status_size,
                     "Could not build automatic Kryon preview");
        return 0;
    }
    return 1;
#endif
}

static int
editor_build_live_module(EditorProject *project, char *status,
                         size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Kryon live preview build is not implemented on Windows yet");
    return 0;
#else
    char quoted_path[EDITOR_PATH_CAP + 16];
    char command[EDITOR_PATH_CAP * 2 + 96];

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }

    if(project->auto_live_build || project->build_live_command[0] == '\0')
        return editor_build_auto_live_module(project, status, status_size);

    path_join(project->live_module_path, sizeof(project->live_module_path),
              project->path, project->live_module_rel_path);
    shell_quote(quoted_path, sizeof(quoted_path), project->path);
    snprintf(command, sizeof(command), "cd %s && %s",
             quoted_path, project->build_live_command);
    if(!editor_build_capture(project, command,
                             "Built Kryon live preview",
                             "Could not build Kryon live preview",
                             status, status_size) ||
       !FileExists(project->live_module_path)) {
        if(status[0] == '\0')
            snprintf(status, status_size,
                     "Could not build Kryon live preview");
        return 0;
    }
    return 1;
#endif
}

static int
editor_load_live_module(EditorProject *project, char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Kryon live preview loading is not implemented on Windows yet");
    return 0;
#else
    CreateAppHostCallback create_host;
    void *symbol;
    char *error;
    char selected_file[EDITOR_PATH_CAP];

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }

    path_join(project->live_module_path, sizeof(project->live_module_path),
              project->path, project->live_module_rel_path);
    if(project->auto_live_build) {
        long source_mtime = editor_project_source_mtime(project);
        long live_mtime = GetFileModTime(project->live_module_path);

        if(live_mtime <= 0 || source_mtime > live_mtime) {
            if(!editor_build_live_module(project, status, status_size)) {
                project->reload_failed = 1;
                project->source_mtime = source_mtime;
                return 0;
            }
        }
    } else if(!FileExists(project->live_module_path) &&
              !editor_build_live_module(project, status, status_size)) {
        project->reload_failed = 1;
        return 0;
    }
    snprintf(selected_file, sizeof(selected_file), "%s", project->selected_file);

    project->live_library = dlopen(project->live_module_path,
                                   RTLD_NOW | RTLD_LOCAL);
    if(project->live_library == NULL) {
        snprintf(status, status_size, "Live preview load failed: %s",
                 dlerror());
        return 0;
    }

    dlerror();
    symbol = dlsym(project->live_library, "CreateKryonLivePreview");
    error = dlerror();
    if(error != NULL || symbol == NULL) {
        dlerror();
        symbol = dlsym(project->live_library, "CreateAppHost");
        error = dlerror();
    }
    if(error != NULL || symbol == NULL) {
        snprintf(status, status_size, "Live preview is missing CreateKryonLivePreview");
        dlclose(project->live_library);
        project->live_library = NULL;
        return 0;
    }
    create_host = (CreateAppHostCallback)symbol;

    dlerror();
    symbol = dlsym(project->live_library, "DestroyKryonLivePreview");
    error = dlerror();
    if(error == NULL && symbol != NULL)
        project->destroy_live_host = (DestroyAppHostCallback)symbol;
    else {
        dlerror();
        symbol = dlsym(project->live_library, "DestroyAppHost");
        error = dlerror();
        if(error == NULL && symbol != NULL)
            project->destroy_live_host = (DestroyAppHostCallback)symbol;
    }

    project->host = create_host(APP_HOST_ABI_VERSION, project->path);
    if(project->host == NULL) {
        snprintf(status, status_size, "Live preview rejected app ABI %d",
                 APP_HOST_ABI_VERSION);
        dlclose(project->live_library);
        project->live_library = NULL;
        project->destroy_live_host = NULL;
        return 0;
    }

    dlerror();
    symbol = dlsym(project->live_library, "CreateKryonPreviewHost");
    error = dlerror();
    if(error == NULL && symbol != NULL) {
        CreatePreviewHostCallback create_preview_host;

        create_preview_host = (CreatePreviewHostCallback)symbol;
        project->preview_host = create_preview_host(PREVIEW_HOST_ABI_VERSION,
                                                    project->path);
        dlerror();
        symbol = dlsym(project->live_library, "DestroyKryonPreviewHost");
        error = dlerror();
        if(error == NULL && symbol != NULL)
            project->destroy_preview_host =
                (DestroyPreviewHostCallback)symbol;
    } else {
        dlerror();
    }
    if(selected_file[0] != '\0' &&
       SetAppScreenBySourcePath(project->host, selected_file)) {
        snprintf(project->selected_file, sizeof(project->selected_file),
                 "%s", selected_file);
    } else {
        AppScreenInfo first;

        project->selected_screen = 0;
        SetAppScreen(project->host, 0);
        first = GetAppScreen(project->host, 0);
        if(first.source_path != NULL)
            snprintf(project->selected_file, sizeof(project->selected_file),
                     "%s", first.source_path);
    }
    project->live_module_mtime = GetFileModTime(project->live_module_path);
    project->source_mtime = editor_project_source_mtime(project);
    project->reload_failed = 0;
    snprintf(status, status_size, "Loaded Kryon live preview for %s",
             project->name);
    return 1;
#endif
}

static int
editor_reload_live_module(EditorProject *project, char *status,
                          size_t status_size)
{
    char selected_file[EDITOR_PATH_CAP];
    int selected_run_target;

    if(project == NULL || !project->loaded)
        return 0;
    snprintf(selected_file, sizeof(selected_file), "%s", project->selected_file);
    selected_run_target = project->selected_run_target;
    if(!editor_build_live_module(project, status, status_size)) {
        project->reload_failed = 1;
        return 0;
    }
    editor_unload_live_module(project);
    snprintf(project->selected_file, sizeof(project->selected_file), "%s",
             selected_file);
    project->selected_run_target = selected_run_target;
    return editor_load_live_module(project, status, status_size);
}

static void
editor_maybe_reload_live_module(EditorProject *project, char *status,
                                size_t status_size)
{
    long source_mtime;
    double now;

    if(project == NULL || !project->loaded)
        return;
    now = GetTime();
    if(now - project->last_reload_check < 2.0)
        return;
    project->last_reload_check = now;
    source_mtime = editor_project_source_mtime(project);
    if(source_mtime <= 0)
        return;
    if(source_mtime <= project->source_mtime &&
       (project->host != NULL || project->reload_failed))
        return;
    project->source_mtime = source_mtime;
    if(project->reload_failed && source_mtime <= project->live_module_mtime)
        return;
    snprintf(status, status_size, "Project changed; rebuilding Kryon live preview");
    if(project->host == NULL)
        editor_load_live_module(project, status, status_size);
    else
        editor_reload_live_module(project, status, status_size);
}

static void
editor_open_project(EditorProject *project, const char *path,
                    EditorRecentProjects *recent, char *status,
                    size_t status_size)
{
    if(project != NULL && project->loaded && path != NULL &&
       strcmp(project->path, path) == 0 && project->host != NULL) {
        snprintf(status, status_size, "Project already open: %s",
                 project->name);
        return;
    }
    editor_close_project(project);
    editor_set_project(project, path);
    editor_add_recent_project(recent, path);
    if(!editor_load_live_module(project, status, status_size))
        return;
}

static int
editor_resolve_project_path(char *dst, size_t dst_size, const char *path)
{
    if(dst == NULL || dst_size == 0 || path == NULL || path[0] == '\0')
        return 0;
#if defined(_WIN32)
    snprintf(dst, dst_size, "%s", path);
    return 1;
#else
    struct stat st;
    char *resolved;

    resolved = realpath(path, NULL);
    if(resolved == NULL)
        return 0;
    if(stat(resolved, &st) != 0 ||
       !(S_ISDIR(st.st_mode) ||
         (S_ISREG(st.st_mode) && path_ext_eq(resolved, ".kry")))) {
        free(resolved);
        return 0;
    }
    snprintf(dst, dst_size, "%s", resolved);
    free(resolved);
    return 1;
#endif
}

static int
editor_run_project(EditorProject *project, char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    (void)status_size;
    snprintf(status, status_size, "Run is not implemented on Windows yet");
    return 0;
#else
    const EditorRunTarget *target;
    char base[EDITOR_PATH_CAP];
    char command[EDITOR_PATH_CAP * 2 + 96];

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }
    if(path_ext_eq(project->selected_file, ".kry") &&
       strcmp(path_basename(project->path), "examples") == 0) {
        editor_strip_kry_ext(base, sizeof(base), project->selected_file);
        snprintf(command, sizeof(command),
                 "make ../build/examples/bin/%s && "
                 "(../build/examples/bin/%s >/tmp/kryon-run-example.log 2>&1 &)",
                 base, base);
        return editor_run_capture(project, command, status, status_size);
    }
    if(project->selected_run_target < 0 ||
       project->selected_run_target >= project->run_target_count) {
        snprintf(status, status_size, "No run target selected");
        return 0;
    }
    target = &project->run_targets[project->selected_run_target];
    return editor_run_capture(project, target->command, status, status_size);
#endif
}

static void
draw_panel(Rectangle bounds, const char *title)
{
    DrawRectangleRec(bounds, GetThemeSurface());
    DrawRectangleLinesEx(bounds, 1, DarkenUIColor(GetThemeSurface(), 36));
    if(title != NULL && title[0] != '\0')
        DrawUIText(title, (int)bounds.x + ScaleUIPx(14),
                   (int)bounds.y + ScaleUIPx(12), UI_TEXT_16,
                   GetThemeText());
}

static int
draw_menu_button(int x, int y, const char *label, int active)
{
    return DrawUIGenericButton(x, y, ScaleUIPx(70), ScaleUIPx(24), label,
                               active ? UI_BUTTON_STYLE_PRIMARY
                                      : UI_BUTTON_STYLE_SECONDARY,
                               0, NULL);
}

static void
draw_project_menu(int x, int y, int *open, FileDialog *project_dialog,
                  EditorProject *project, char *status, size_t status_size)
{
    int w = ScaleUIPx(188);
    int item_h = ScaleUIPx(32);
    int item_count = project != NULL && project->loaded ? 4 : 3;

    if(open == NULL || !*open)
        return;
    DrawRectangle(x, y, w, item_h * item_count + ScaleUIPx(10),
                  GetThemeSurface());
    DrawRectangleLines(x, y, w, item_h * item_count + ScaleUIPx(10),
                       DarkenUIColor(GetThemeSurface(), 38));
    y += ScaleUIPx(5);
    if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                           "Open Project", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
        BeginSelectFileDialogFolder(project_dialog, "Open Project");
        *open = 0;
    }
    y += item_h;
    if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                           "New File", UI_BUTTON_STYLE_SECONDARY,
                           project == NULL || !project->loaded, NULL)) {
        editor_create_new_file(project, status, status_size);
        *open = 0;
    }
    y += item_h;
    if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                           "Save", UI_BUTTON_STYLE_SECONDARY,
                           project == NULL || !project->source_loaded, NULL)) {
        editor_save_source_file(project, status, status_size);
        *open = 0;
    }
    if(project != NULL && project->loaded) {
        y += item_h;
        if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12),
                               item_h, "Close Project",
                               UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
            editor_close_project(project);
            snprintf(status, status_size, "Closed project");
            *open = 0;
        }
    }
}

static int
draw_sidebar_row(int x, int y, int w, const char *label, int active)
{
    Rectangle bounds;
    Vector2 mouse;
    int pressed = 0;

    if(w < ScaleUIPx(72))
        w = ScaleUIPx(72);
    bounds = (Rectangle){(float)x, (float)y, (float)w, (float)ScaleUIPx(30)};
    mouse = GetMousePosition();
    if(CheckCollisionPointRec(mouse, bounds) &&
       IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        pressed = 1;
    DrawUIGenericButton(x, y, w, ScaleUIPx(30), label,
                        active ? UI_BUTTON_STYLE_PRIMARY
                               : UI_BUTTON_STYLE_SECONDARY,
                        0, NULL);
    return pressed;
}

static void
draw_sidebar_section(int x, int *y, const char *title)
{
    if(y == NULL)
        return;
    DrawUIText(title, x, *y, UI_TEXT_12, GetThemeIcon());
    *y += ScaleUIPx(22);
}

static void
editor_add_tree_item(EditorTreeItem *items, int *count, const char *label,
                     const char *path, int depth, int is_dir)
{
    EditorTreeItem *entry;

    if(items == NULL || count == NULL || *count >= EDITOR_MAX_TREE_ITEMS)
        return;
    entry = &items[*count];
    memset(entry, 0, sizeof(*entry));
    snprintf(entry->label, sizeof(entry->label), "%s",
             label != NULL ? label : "");
    snprintf(entry->path, sizeof(entry->path), "%s", path != NULL ? path : "");
    entry->item.label = entry->label;
    entry->item.depth = depth;
    entry->item.id = editor_tree_id(path);
    entry->item.is_dir = is_dir;
    entry->item.selectable = !is_dir;
    (*count)++;
}

static void
editor_build_project_tree(EditorProject *project, const char *dir_path,
                          int depth, EditorTreeItem *items, int *count)
{
    EditorTreeEntry entries[EDITOR_MAX_TREE_ENTRIES];
    char rel_path[EDITOR_PATH_CAP];
    int entry_count;

    if(project == NULL || dir_path == NULL || items == NULL ||
       count == NULL || depth > EDITOR_TREE_DEPTH ||
       *count >= EDITOR_MAX_TREE_ITEMS)
        return;

    entry_count = editor_read_tree_entries(dir_path, entries,
                                           EDITOR_MAX_TREE_ENTRIES);
    for(int i = 0; i < entry_count && *count < EDITOR_MAX_TREE_ITEMS; i++) {
        editor_relative_path(rel_path, sizeof(rel_path), project, entries[i].path);
        if(entries[i].is_dir) {
            if(path_is_ignored_dir(entries[i].name))
                continue;
            if(depth >= EDITOR_TREE_DEPTH)
                continue;
            editor_add_tree_item(items, count, entries[i].name, rel_path,
                                 depth, 1);
            editor_build_project_tree(project, entries[i].path, depth + 1,
                                      items, count);
            continue;
        }
        editor_add_tree_item(items, count, entries[i].name, rel_path, depth, 0);
    }
}

static const char *
editor_tree_path_for_id(const EditorTreeItem *items, int count, int id)
{
    for(int i = 0; i < count; i++) {
        if(items[i].item.id == id)
            return items[i].path;
    }
    return NULL;
}

static int
editor_sidebar_has_expanded(const EditorSidebarState *sidebar, int id)
{
    if(sidebar == NULL)
        return 0;
    for(int i = 0; i < sidebar->expanded_count; i++) {
        if(sidebar->expanded_ids[i] == id)
            return 1;
    }
    return 0;
}

static void
editor_sidebar_add_expanded(EditorSidebarState *sidebar, int id)
{
    if(sidebar == NULL || id == 0 || editor_sidebar_has_expanded(sidebar, id))
        return;
    if(sidebar->expanded_count >= EDITOR_MAX_EXPANDED_DIRS)
        return;
    sidebar->expanded_ids[sidebar->expanded_count++] = id;
}

static int
editor_tree_item_path_visible(const char *path,
                              const EditorSidebarState *sidebar)
{
    char parent[EDITOR_PATH_CAP];
    size_t len;

    if(path == NULL || path[0] == '\0')
        return 1;
    snprintf(parent, sizeof(parent), "%s", path);
    len = strlen(parent);
    for(size_t i = 0; i < len; i++) {
        if(parent[i] != '/')
            continue;
        parent[i] = '\0';
        if(!editor_sidebar_has_expanded(sidebar, editor_tree_id(parent)))
            return 0;
        parent[i] = '/';
    }
    return 1;
}

static void
editor_sidebar_reveal_selected(EditorSidebarState *sidebar,
                               const EditorTreeItem *items, int count,
                               const char *selected_path, Rectangle bounds,
                               int row_height)
{
    char parent[EDITOR_PATH_CAP];
    int selected_id;
    int visible_index = 0;

    if(sidebar == NULL || selected_path == NULL || selected_path[0] == '\0')
        return;
    selected_id = editor_tree_id(selected_path);
    snprintf(parent, sizeof(parent), "%s", selected_path);
    for(char *p = parent; *p != '\0'; p++) {
        if(*p != '/')
            continue;
        *p = '\0';
        editor_sidebar_add_expanded(sidebar, editor_tree_id(parent));
        *p = '/';
    }
    sidebar->selected_id = selected_id;
    if(sidebar->revealed_id == selected_id)
        return;
    for(int i = 0; i < count; i++) {
        if(!editor_tree_item_path_visible(items[i].path, sidebar))
            continue;
        if(items[i].item.id == selected_id) {
            int row_h = ScaleUIPx(row_height);
            int target_y = visible_index * row_h -
                           ((int)bounds.height - row_h) / 3;
            sidebar->scroll_y = target_y > 0 ? target_y : 0;
            sidebar->revealed_id = selected_id;
            return;
        }
        visible_index++;
    }
}

static void
draw_left_sidebar(Rectangle bounds, EditorProject *project,
                  EditorRecentProjects *recent, EditorSidebarState *sidebar,
                  char *status, size_t status_size)
{
    int x = (int)bounds.x + ScaleUIPx(14);
    int y;
    int w = (int)bounds.width - ScaleUIPx(28);
    static EditorTreeItem tree_items[EDITOR_MAX_TREE_ITEMS];
    static UICascadingTreeItem ui_items[EDITOR_MAX_TREE_ITEMS];
    int tree_count = 0;
    int activated_id = 0;
    const char *activated_path;
    Rectangle tree_bounds;

    draw_panel(bounds, project->loaded ? project->name : "Project");
    if(sidebar == NULL)
        return;
    if(DrawUIGenericButton((int)(bounds.x + bounds.width - ScaleUIPx(42)),
                           (int)bounds.y + ScaleUIPx(8),
                           ScaleUIPx(28), ScaleUIPx(24), "<",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
        sidebar->collapsed = 1;
        return;
    }
    PushUIInputClip(bounds);

    y = (int)bounds.y + ScaleUIPx(48);

    if(!project->loaded) {
        draw_sidebar_section(x, &y, "Recent Projects");
        if(recent == NULL || recent->count == 0) {
            DrawUIText("No recent projects.", x, y, UI_TEXT_16,
                       GetThemeIcon());
            PopUIInputClip();
            return;
        }
        for(int i = 0; i < recent->count; i++) {
            if(y + ScaleUIPx(34) >
               (int)(bounds.y + bounds.height - ScaleUIPx(14)))
                break;
            if(draw_sidebar_row(x, y, w, path_basename(recent->paths[i]), 0)) {
                editor_open_project(project, recent->paths[i], recent,
                                    status, status_size);
                memset(sidebar, 0, sizeof(*sidebar));
            }
            y += ScaleUIPx(38);
        }
        PopUIInputClip();
        return;
    }

    draw_sidebar_section(x, &y, "Search");
    {
        int commit = 0;
        int field_h = ScaleUIPx(30);
        int btn_w = ScaleUIPx(56);
        int result_h = ScaleUIPx(26);
        int max_results_h = ScaleUIPx(136);

        DrawUITextField((UITextField){
            .bounds = {(float)x, (float)y,
                       (float)(w - btn_w - ScaleUIPx(8)),
                       (float)field_h},
            .text = project->search_text,
            .text_size = sizeof(project->search_text),
            .cursor_position = &project->search_cursor,
            .focused = &project->search_focused,
            .font = UI_TEXT_16,
            .focus_id = 1211,
            .style = editor_input_style(),
            .commit_pressed = &commit
        });
        if(DrawUIGenericButton(x + w - btn_w, y, btn_w, field_h, "Find",
                               UI_BUTTON_STYLE_SECONDARY,
                               project->search_text[0] == '\0', NULL) ||
           commit)
            editor_search_project(project, status, status_size);
        y += field_h + ScaleUIPx(8);

        if(project->search_visible) {
            Rectangle results = {(float)x, (float)y, (float)w,
                                 (float)max_results_h};
            int draw_y = y - project->search_scroll_y;

            DrawRectangleRec(results, DarkenUIColor(GetThemeSurface(), 4));
            DrawRectangleLinesEx(results, 1,
                                 DarkenUIColor(GetThemeSurface(), 38));
            PushUIInputClip(results);
            for(int i = 0; i < project->search_result_count; i++) {
                char label[220];
                int active = i == project->selected_search_result;

                if(draw_y + result_h >= y &&
                   draw_y <= y + max_results_h) {
                    snprintf(label, sizeof(label), "%s:%d %s",
                             project->search_results[i].path,
                             project->search_results[i].line,
                             project->search_results[i].excerpt);
                    if(draw_sidebar_row(x + ScaleUIPx(4), draw_y,
                                        w - ScaleUIPx(8), label, active)) {
                        project->selected_search_result = i;
                        editor_open_file_at_line(project,
                            project->search_results[i].path,
                            project->search_results[i].line,
                            status, status_size);
                    }
                }
                draw_y += result_h;
            }
            PopUIInputClip();
            if(CheckCollisionPointRec(GetMousePosition(), results)) {
                project->search_scroll_y -=
                    (int)(GetMouseWheelMove() * (float)result_h * 3.0f);
                if(project->search_scroll_y < 0)
                    project->search_scroll_y = 0;
            }
            y += max_results_h + ScaleUIPx(12);
        }
    }

    draw_sidebar_section(x, &y, "Files");
    editor_build_project_tree(project, project->path, 0, tree_items,
                              &tree_count);
    for(int i = 0; i < tree_count; i++)
        ui_items[i] = tree_items[i].item;
    tree_bounds = (Rectangle){(float)x, (float)y, (float)w,
                              bounds.y + bounds.height - ScaleUIPx(14) -
                                  (float)y};
    if(project->selected_file[0] != '\0')
        editor_sidebar_reveal_selected(sidebar, tree_items, tree_count,
                                       project->selected_file, tree_bounds,
                                       28);
    DrawUICascadingTreeView((UICascadingTreeView){
        .bounds = tree_bounds,
        .id = 1201,
        .items = ui_items,
        .item_count = tree_count,
        .selected_id = &sidebar->selected_id,
        .activated_id = &activated_id,
        .expanded = {
            .ids = sidebar->expanded_ids,
            .count = &sidebar->expanded_count,
            .capacity = EDITOR_MAX_EXPANDED_DIRS
        },
        .scroll_offset = &sidebar->scroll_y,
        .row_height = 28
    });
    activated_path = editor_tree_path_for_id(tree_items, tree_count,
                                             activated_id);
    if(activated_path != NULL) {
        editor_select_file(project, activated_path);
        sidebar->selected_id = activated_id;
        sidebar->revealed_id = activated_id;
        if(editor_select_source_path(project, activated_path)) {
            snprintf(status, status_size, "Opened %s", activated_path);
        } else {
            snprintf(status, status_size, "Selected %s", activated_path);
        }
    }
    PopUIInputClip();
}

static void editor_restore_ide_ui_frame(int view_w, int view_h);

static void
draw_source_placeholder(Rectangle content, const EditorProject *project,
                        int preview_available)
{
    int x = (int)content.x + ScaleUIPx(28);
    int y = (int)content.y + ScaleUIPx(28);
    const char *file = project != NULL && project->selected_file[0] != '\0' ?
                       project->selected_file : "No file selected";

    DrawRectangleRec(content, GetThemeBackground());
    DrawUIText(file, x, y, UI_TEXT_24, GetThemeText());
    y += ScaleUIPx(42);
    DrawUIText(preview_available ? "Preview available" : "Source view",
               x, y, UI_TEXT_16, GetThemeIcon());
}

static int
editor_read_text_file(const char *path, char *buffer, size_t buffer_size)
{
    FILE *file;
    size_t n;

    if(path == NULL || buffer == NULL || buffer_size == 0)
        return 0;
    buffer[0] = '\0';
    file = fopen(path, "rb");
    if(file == NULL)
        return 0;
    n = fread(buffer, 1, buffer_size - 1, file);
    buffer[n] = '\0';
    fclose(file);
    return 1;
}

static int
editor_write_text_file(const char *path, const char *buffer)
{
    FILE *file;
    size_t len;

    if(path == NULL || buffer == NULL)
        return 0;
    file = fopen(path, "wb");
    if(file == NULL)
        return 0;
    len = strlen(buffer);
    if(fwrite(buffer, 1, len, file) != len) {
        fclose(file);
        return 0;
    }
    fclose(file);
    return 1;
}

static int
editor_file_is_text(const char *path)
{
    const char *base = path_basename(path);

    return path_ext_eq(path, ".kry") ||
           path_ext_eq(path, ".c") ||
           path_ext_eq(path, ".h") ||
           path_ext_eq(path, ".md") ||
           path_ext_eq(path, ".txt") ||
           path_ext_eq(path, ".mk") ||
           strcmp(base, "Makefile") == 0 ||
           strcmp(base, "makefile") == 0 ||
           strcmp(base, "project.kryon") == 0;
}

static void
editor_trim_excerpt(char *text)
{
    char *trimmed;

    if(text == NULL)
        return;
    trimmed = editor_trim_line(text);
    if(trimmed != text)
        memmove(text, trimmed, strlen(trimmed) + 1);
}

static void
editor_add_search_result(EditorProject *project, const char *path,
                         int line_no, const char *line)
{
    EditorSearchResult *result;

    if(project == NULL || path == NULL || line == NULL ||
       project->search_result_count >= EDITOR_MAX_SEARCH_RESULTS)
        return;
    result = &project->search_results[project->search_result_count++];
    snprintf(result->path, sizeof(result->path), "%s", path);
    result->line = line_no;
    snprintf(result->excerpt, sizeof(result->excerpt), "%s", line);
    editor_trim_excerpt(result->excerpt);
}

static void
editor_search_file(EditorProject *project, const char *abs_path,
                   const char *rel_path, const char *query)
{
    char text[EDITOR_SOURCE_MAX_BYTES];
    int line_start = 0;
    int line_no = 1;
    int len;

    if(project == NULL || query == NULL || query[0] == '\0' ||
       abs_path == NULL || rel_path == NULL ||
       project->search_result_count >= EDITOR_MAX_SEARCH_RESULTS)
        return;
    if(strstr(rel_path, query) != NULL)
        editor_add_search_result(project, rel_path, 1, "file match");
    if(!editor_file_is_text(rel_path))
        return;
    if(!editor_read_text_file(abs_path, text, sizeof(text)))
        return;
    len = (int)strlen(text);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            char line[192];
            int line_len = i - line_start;

            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, text + line_start, (size_t)line_len);
            line[line_len] = '\0';
            if(strstr(line, query) != NULL)
                editor_add_search_result(project, rel_path, line_no, line);
            line_start = i + 1;
            line_no++;
        }
        if(project->search_result_count >= EDITOR_MAX_SEARCH_RESULTS)
            return;
    }
}

static void
editor_search_dir(EditorProject *project, const char *dir_path,
                  const char *query, int depth)
{
    EditorTreeEntry entries[EDITOR_MAX_TREE_ENTRIES];
    char rel_path[EDITOR_PATH_CAP];
    int entry_count;

    if(project == NULL || dir_path == NULL || query == NULL ||
       depth > EDITOR_TREE_DEPTH ||
       project->search_result_count >= EDITOR_MAX_SEARCH_RESULTS)
        return;
    entry_count = editor_read_tree_entries(dir_path, entries,
                                           EDITOR_MAX_TREE_ENTRIES);
    for(int i = 0; i < entry_count; i++) {
        editor_relative_path(rel_path, sizeof(rel_path), project,
                             entries[i].path);
        if(entries[i].is_dir) {
            if(!path_is_ignored_dir(entries[i].name))
                editor_search_dir(project, entries[i].path, query, depth + 1);
        } else {
            editor_search_file(project, entries[i].path, rel_path, query);
        }
        if(project->search_result_count >= EDITOR_MAX_SEARCH_RESULTS)
            return;
    }
}

static void
editor_search_project(EditorProject *project, char *status,
                      size_t status_size)
{
    if(project == NULL || !project->loaded)
        return;
    project->search_result_count = 0;
    project->selected_search_result = -1;
    project->search_visible = 1;
    project->search_scroll_y = 0;
    if(project->search_text[0] == '\0') {
        snprintf(status, status_size, "Enter a search query");
        return;
    }
    editor_search_dir(project, project->path, project->search_text, 0);
    snprintf(status, status_size, "Found %d result%s for %s",
             project->search_result_count,
             project->search_result_count == 1 ? "" : "s",
             project->search_text);
}

static int
editor_parse_diagnostic_line(EditorProject *project, char *line)
{
    char *p1;
    char *p2;
    char *p3;
    char *message;
    EditorDiagnostic *diag;
    int line_no;
    int column = 0;

    if(project == NULL || line == NULL ||
       project->diagnostic_count >= EDITOR_MAX_DIAGNOSTICS)
        return 0;
    p1 = strchr(line, ':');
    if(p1 == NULL)
        return 0;
    p2 = strchr(p1 + 1, ':');
    if(p2 == NULL)
        return 0;
    *p1 = '\0';
    *p2 = '\0';
    line_no = atoi(p1 + 1);
    if(line_no <= 0)
        return 0;
    p3 = strchr(p2 + 1, ':');
    if(p3 != NULL) {
        char *after_col = p2 + 1;
        int all_digits = 1;

        for(char *p = after_col; p < p3; p++) {
            if(*p < '0' || *p > '9') {
                all_digits = 0;
                break;
            }
        }
        if(all_digits) {
            *p3 = '\0';
            column = atoi(after_col);
            message = p3 + 1;
        } else {
            message = p2 + 1;
        }
    } else {
        message = p2 + 1;
    }
    message = editor_trim_line(message);
    diag = &project->diagnostics[project->diagnostic_count++];
    snprintf(diag->path, sizeof(diag->path), "%s", line);
    diag->line = line_no;
    diag->column = column;
    if(strstr(message, "error") != NULL)
        snprintf(diag->severity, sizeof(diag->severity), "error");
    else if(strstr(message, "warning") != NULL)
        snprintf(diag->severity, sizeof(diag->severity), "warning");
    else
        snprintf(diag->severity, sizeof(diag->severity), "info");
    snprintf(diag->message, sizeof(diag->message), "%s", message);
    return 1;
}

static void
editor_parse_diagnostics(EditorProject *project)
{
    char line[1024];
    int line_start = 0;
    int len;

    if(project == NULL)
        return;
    project->diagnostic_count = 0;
    project->selected_diagnostic = -1;
    project->source_highlight_line = 0;
    len = (int)strlen(project->output);
    for(int i = 0; i <= len; i++) {
        if(project->output[i] == '\n' || project->output[i] == '\0') {
            int line_len = i - line_start;

            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, project->output + line_start, (size_t)line_len);
            line[line_len] = '\0';
            editor_parse_diagnostic_line(project, line);
            line_start = i + 1;
        }
        if(project->diagnostic_count >= EDITOR_MAX_DIAGNOSTICS)
            break;
    }
    for(int i = 0; i < project->diagnostic_count; i++) {
        char rel[EDITOR_PATH_CAP];

        editor_relative_path(rel, sizeof(rel), project,
                             project->diagnostics[i].path);
        if(strcmp(rel, project->source_scroll_file) == 0 ||
           strcmp(rel, project->selected_file) == 0) {
            project->source_highlight_line = project->diagnostics[i].line;
            project->selected_diagnostic = i;
            break;
        }
    }
}

static int
editor_run_capture(EditorProject *project, const char *command,
                   char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    (void)command;
    snprintf(status, status_size, "Task output is not implemented on Windows yet");
    return 0;
#else
    char quoted_path[EDITOR_PATH_CAP + 16];
    char *shell_command;
    FILE *pipe;
    size_t used = 0;
    size_t command_len;
    int rc;

    if(project == NULL || !project->loaded || command == NULL ||
       command[0] == '\0')
        return 0;
    editor_output_clear(project);
    project->output_visible = 1;
    shell_quote(quoted_path, sizeof(quoted_path), project->path);
    command_len = strlen(quoted_path) + strlen(command) + 16;
    shell_command = malloc(command_len);
    if(shell_command == NULL) {
        snprintf(status, status_size, "Out of memory");
        return 0;
    }
    snprintf(shell_command, command_len, "cd %s && %s 2>&1",
             quoted_path, command);
    pipe = popen(shell_command, "r");
    if(pipe == NULL) {
        free(shell_command);
        snprintf(status, status_size, "Could not start task");
        return 0;
    }
    while(used + 1 < sizeof(project->output)) {
        size_t n = fread(project->output + used, 1,
                         sizeof(project->output) - used - 1, pipe);
        used += n;
        if(n == 0)
            break;
    }
    project->output[used] = '\0';
    rc = pclose(pipe);
    free(shell_command);
    editor_parse_diagnostics(project);
    if(WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
        snprintf(status, status_size, "Task succeeded: %s", command);
        return 1;
    }
    snprintf(status, status_size, "Task failed: %s", command);
    return 0;
#endif
}

static int
editor_build_capture(EditorProject *project, const char *command,
                     const char *success, const char *failure,
                     char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    (void)command;
    (void)success;
    (void)failure;
    snprintf(status, status_size, "Build output is not implemented on Windows yet");
    return 0;
#else
    char *shell_command;
    FILE *pipe;
    size_t used = 0;
    size_t command_len;
    int rc;

    if(project == NULL || !project->loaded || command == NULL ||
       command[0] == '\0')
        return 0;
    editor_output_clear(project);
    command_len = strlen(command) + 8;
    shell_command = malloc(command_len);
    if(shell_command == NULL) {
        snprintf(status, status_size, "Out of memory");
        return 0;
    }
    snprintf(shell_command, command_len, "%s 2>&1", command);
    pipe = popen(shell_command, "r");
    if(pipe == NULL) {
        free(shell_command);
        snprintf(status, status_size, "Could not start build");
        return 0;
    }
    while(used + 1 < sizeof(project->output)) {
        size_t n = fread(project->output + used, 1,
                         sizeof(project->output) - used - 1, pipe);
        used += n;
        if(n == 0)
            break;
    }
    project->output[used] = '\0';
    rc = pclose(pipe);
    free(shell_command);
    editor_parse_diagnostics(project);
    if(WIFEXITED(rc) && WEXITSTATUS(rc) == 0) {
        project->output_visible = 0;
        snprintf(status, status_size, "%s", success);
        return 1;
    }
    project->output_visible = 1;
    snprintf(status, status_size, "%s", failure);
    return 0;
#endif
}

static int
editor_source_cursor_for_line(const char *text, int line_no)
{
    int line = 1;

    if(text == NULL || line_no <= 1)
        return 0;
    for(int i = 0; text[i] != '\0'; i++) {
        if(text[i] == '\n') {
            line++;
            if(line >= line_no)
                return i + 1;
        }
    }
    return (int)strlen(text);
}

static int
editor_source_line_for_cursor(const char *text, int cursor)
{
    int line_no = 1;

    if(text == NULL || cursor <= 0)
        return line_no;
    for(int i = 0; i < cursor && text[i] != '\0'; i++) {
        if(text[i] == '\n')
            line_no++;
    }
    return line_no;
}

static void
editor_open_file_at_line(EditorProject *project, const char *path,
                         int line_no, char *status, size_t status_size)
{
    char abs_path[EDITOR_PATH_CAP];
    char text[EDITOR_SOURCE_MAX_BYTES];
    int cursor = 0;
    int line_h;

    if(project == NULL || path == NULL || path[0] == '\0')
        return;
    editor_select_file(project, path);
    if(line_no > 0) {
        path_join(abs_path, sizeof(abs_path), project->path, path);
        if(editor_read_text_file(abs_path, text, sizeof(text)))
            cursor = editor_source_cursor_for_line(text, line_no);
        project->source_pending_cursor = cursor;
        project->source_pending_line = line_no;
        project->source_pending_valid = 1;
        project->source_highlight_line = line_no;
        line_h = GetUITextLineHeight(editor_source_font_size(project)) +
                 ScaleUIPx(2);
        project->source_scroll_y = (line_no - 2) * line_h;
        if(project->source_scroll_y < 0)
            project->source_scroll_y = 0;
    }
    if(status != NULL && status_size > 0) {
        if(line_no > 0)
            snprintf(status, status_size, "Opened %s:%d", path, line_no);
        else
            snprintf(status, status_size, "Opened %s", path);
    }
}

static int
editor_mod_key_down(void)
{
    return IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
           IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
}

static int
editor_line_starts_with_word(const char *line, const char *word)
{
    size_t len;

    if(line == NULL || word == NULL)
        return 0;
    while(*line == ' ' || *line == '\t')
        line++;
    len = strlen(word);
    if(strncmp(line, word, len) != 0)
        return 0;
    return line[len] == '\0' || line[len] == ' ' || line[len] == '\t' ||
           line[len] == '(' || line[len] == '{';
}

static int
editor_find_widget_line_in_text(const char *text,
                                const UIInspectSelection *selection)
{
    char line[1024];
    int line_start = 0;
    int line_no = 1;
    int kind_seen = 0;
    int action_line = 0;
    int kind_line = 0;
    int len;

    if(text == NULL || selection == NULL || !selection->valid ||
       selection->kind[0] == '\0')
        return 0;
    len = (int)strlen(text);
    for(int i = 0; i <= len; i++) {
        if(text[i] == '\n' || text[i] == '\0') {
            int line_len = i - line_start;

            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, text + line_start, (size_t)line_len);
            line[line_len] = '\0';
            if(editor_line_starts_with_word(line, selection->kind)) {
                kind_seen++;
                if(kind_seen == selection->kind_index)
                    kind_line = line_no;
                if(selection->action[0] != '\0' &&
                   strstr(line, selection->action) != NULL)
                    action_line = line_no;
            }
            if(action_line > 0)
                return action_line;
            if(kind_line > 0)
                return kind_line;
            line_start = i + 1;
            line_no++;
        }
    }
    return 0;
}

static int
editor_jump_to_active_widget_source(EditorProject *project,
                                    const UIInspectSelection *selection,
                                    char *status, size_t status_size)
{
    char path[EDITOR_PATH_CAP];
    char rel_path[EDITOR_PATH_CAP];
    char text[EDITOR_SOURCE_MAX_BYTES];
    const char *source_path;
    int line_no;
    int cursor;
    int line_h;

    if(project == NULL || selection == NULL || !selection->valid)
        return 0;
    source_path = selection->source_path[0] != '\0'
                      ? selection->source_path
                      : editor_active_screen_source_path(project);
    if(source_path == NULL || source_path[0] == '\0' ||
       !path_ext_eq(source_path, ".kry"))
        return 0;
    editor_relative_path(rel_path, sizeof(rel_path), project, source_path);
    if(source_path[0] == '/')
        snprintf(path, sizeof(path), "%s", source_path);
    else
        snprintf(path, sizeof(path), "%s/%s", project->path, source_path);
    if(!editor_read_text_file(path, text, sizeof(text)))
        return 0;
    line_no = selection->source_line > 0
                  ? selection->source_line
                  : editor_find_widget_line_in_text(text, selection);
    if(line_no <= 0)
        return 0;
    cursor = editor_source_cursor_for_line(text, line_no);
    editor_select_file(project, rel_path);
    editor_select_source_path(project, rel_path);
    if(editor_selected_file_has_preview(project))
        project->layout_mode = EDITOR_LAYOUT_SPLIT;
    project->source_pending_cursor = cursor;
    project->source_pending_line = line_no;
    project->source_pending_valid = 1;
    project->source_highlight_line = line_no;
    if(strcmp(project->source_scroll_file, rel_path) == 0) {
        project->source_cursor = cursor;
        line_h = GetUITextLineHeight(editor_source_font_size(project)) +
                 ScaleUIPx(2);
        project->source_scroll_y = (line_no - 2) * line_h;
        if(project->source_scroll_y < 0)
            project->source_scroll_y = 0;
    }
    project->inspect_active = 1;
    snprintf(status, status_size, "Inspecting %s:%d", rel_path, line_no);
    return 1;
}

static UISyntaxMode
editor_source_syntax(const char *path)
{
    const char *base = path_basename(path);

    if(path_ext_eq(path, ".kry") || strcmp(base, "project.kryon") == 0)
        return UI_SYNTAX_KRY;
    if(path_ext_eq(path, ".c") || path_ext_eq(path, ".h"))
        return UI_SYNTAX_C;
    if(strcmp(base, "Makefile") == 0 || strcmp(base, "makefile") == 0 ||
       path_ext_eq(path, ".mk"))
        return UI_SYNTAX_MAKE;
    return UI_SYNTAX_NONE;
}

static int
editor_save_source_file(EditorProject *project, char *status,
                        size_t status_size)
{
    char path[EDITOR_PATH_CAP];

    if(project == NULL || project->source_scroll_file[0] == '\0')
        return 0;
    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->source_scroll_file);
    if(!editor_write_text_file(path, project->source)) {
        snprintf(status, status_size, "Could not save %s",
                 project->source_scroll_file);
        return 0;
    }
    project->source_dirty = 0;
    project->selected_file_mtime = GetFileModTime(path);
    project->source_external_change_reported = 0;
    snprintf(status, status_size, "Saved %s", project->source_scroll_file);
    return 1;
}

static long
editor_selected_source_mtime(EditorProject *project)
{
    char path[EDITOR_PATH_CAP];

    if(project == NULL || project->source_scroll_file[0] == '\0')
        return 0;
    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->source_scroll_file);
    return GetFileModTime(path);
}

static int
editor_source_changed_on_disk(EditorProject *project)
{
    long mtime;

    if(project == NULL || project->selected_file_mtime <= 0)
        return 0;
    mtime = editor_selected_source_mtime(project);
    return mtime > project->selected_file_mtime;
}

static int
editor_reload_source_file(EditorProject *project, char *status,
                          size_t status_size)
{
    char path[EDITOR_PATH_CAP];
    int source_len;

    if(project == NULL || project->source_scroll_file[0] == '\0')
        return 0;
    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->source_scroll_file);
    if(!editor_read_text_file(path, project->source, sizeof(project->source))) {
        snprintf(status, status_size, "Could not reload %s",
                 project->source_scroll_file);
        return 0;
    }
    source_len = (int)strlen(project->source);
    project->source_cursor = ui_clampi(project->source_cursor, 0, source_len);
    SetUITextAreaSelection(1501, project->source_cursor,
                           project->source_cursor);
    project->source_dirty = 0;
    project->source_loaded = 1;
    project->selected_file_mtime = GetFileModTime(path);
    project->source_external_change_reported = 0;
    editor_free_history(project->undo_items, &project->undo_count);
    editor_free_history(project->redo_items, &project->redo_count);
    snprintf(status, status_size, "Reloaded %s", project->source_scroll_file);
    return 1;
}

static void
editor_maybe_reload_source_file(EditorProject *project, char *status,
                                size_t status_size)
{
    double now;
    long mtime;

    if(project == NULL || !project->source_loaded ||
       project->source_scroll_file[0] == '\0')
        return;
    now = GetTime();
    if(now - project->last_source_file_check < 0.6)
        return;
    project->last_source_file_check = now;
    mtime = editor_selected_source_mtime(project);
    if(mtime <= 0)
        return;
    if(project->selected_file_mtime <= 0) {
        project->selected_file_mtime = mtime;
        return;
    }
    if(mtime <= project->selected_file_mtime)
        return;
    if(project->source_dirty) {
        if(!project->source_external_change_reported) {
            snprintf(status, status_size,
                     "%s changed on disk; autosave paused",
                     project->source_scroll_file);
            project->source_external_change_reported = 1;
        }
        return;
    }
    editor_reload_source_file(project, status, status_size);
}

static int
editor_save_source_now(EditorProject *project, char *status,
                       size_t status_size)
{
    char saved_file[EDITOR_PATH_CAP];

    if(project == NULL || !project->source_loaded)
        return 0;
    snprintf(saved_file, sizeof(saved_file), "%s", project->source_scroll_file);
    if(!editor_save_source_file(project, status, status_size))
        return 0;
    if(path_ext_eq(saved_file, ".kry")) {
        snprintf(status, status_size, "Saved %s; rebuilding preview",
                 saved_file);
        editor_reload_live_module(project, status, status_size);
    }
    return 1;
}

static void
editor_maybe_autosave_source(EditorProject *project, char *status,
                             size_t status_size)
{
    if(project == NULL || !project->source_dirty)
        return;
    if(editor_source_changed_on_disk(project)) {
        if(!project->source_external_change_reported) {
            snprintf(status, status_size,
                     "%s changed on disk; autosave paused",
                     project->source_scroll_file);
            project->source_external_change_reported = 1;
        }
        return;
    }
    if(GetTime() - project->source_last_edit_time < 0.65)
        return;
    editor_save_source_now(project, status, status_size);
}

static int
editor_undo_source(EditorProject *project, char *status, size_t status_size)
{
    char current[EDITOR_SOURCE_MAX_BYTES];

    if(project == NULL || project->undo_count <= 0)
        return 0;
    snprintf(current, sizeof(current), "%s", project->source);
    editor_push_history(project->redo_items, &project->redo_count, current);
    if(!editor_pop_history(project->undo_items, &project->undo_count,
                           project->source, sizeof(project->source)))
        return 0;
    project->source_cursor = (int)strlen(project->source);
    project->source_dirty = 1;
    project->source_last_edit_time = GetTime();
    snprintf(status, status_size, "Undo");
    return 1;
}

static int
editor_redo_source(EditorProject *project, char *status, size_t status_size)
{
    char current[EDITOR_SOURCE_MAX_BYTES];

    if(project == NULL || project->redo_count <= 0)
        return 0;
    snprintf(current, sizeof(current), "%s", project->source);
    editor_push_history(project->undo_items, &project->undo_count, current);
    if(!editor_pop_history(project->redo_items, &project->redo_count,
                           project->source, sizeof(project->source)))
        return 0;
    project->source_cursor = (int)strlen(project->source);
    project->source_dirty = 1;
    project->source_last_edit_time = GetTime();
    snprintf(status, status_size, "Redo");
    return 1;
}

static void
editor_adjust_source_zoom(EditorProject *project, int delta, char *status,
                          size_t status_size)
{
    int font_size;

    if(project == NULL)
        return;
    font_size = editor_source_font_size(project) + delta;
    if(font_size < 12)
        font_size = 12;
    if(font_size > 24)
        font_size = 24;
    project->source_font_size = font_size;
    snprintf(status, status_size, "Source font: %dpx", font_size);
}

static int
editor_scroll_to_cursor(EditorProject *project, int line_no)
{
    int line_h;

    if(project == NULL || line_no <= 0)
        return 0;
    line_h = GetUITextLineHeight(editor_source_font_size(project)) +
             ScaleUIPx(2);
    project->source_scroll_y = (line_no - 2) * line_h;
    if(project->source_scroll_y < 0)
        project->source_scroll_y = 0;
    project->source_highlight_line = line_no;
    return 1;
}

static int
editor_find_next_source(EditorProject *project, char *status,
                        size_t status_size)
{
    char *hit;
    int len;

    if(project == NULL || project->find_text[0] == '\0' ||
       !project->source_loaded)
        return 0;
    len = (int)strlen(project->source);
    project->source_cursor = ui_clampi(project->source_cursor, 0, len);
    hit = strstr(project->source + project->source_cursor + 1,
                 project->find_text);
    if(hit == NULL)
        hit = strstr(project->source, project->find_text);
    if(hit == NULL) {
        snprintf(status, status_size, "No match: %s", project->find_text);
        return 0;
    }
    project->source_cursor = (int)(hit - project->source);
    editor_scroll_to_cursor(project,
        editor_source_line_for_cursor(project->source, project->source_cursor));
    snprintf(status, status_size, "Found %s", project->find_text);
    return 1;
}

static int
editor_replace_next_source(EditorProject *project, char *status,
                           size_t status_size)
{
    char *hit;
    int source_len;
    int find_len;
    int replace_len;
    char *before;

    if(project == NULL || project->find_text[0] == '\0' ||
       !project->source_loaded)
        return 0;
    source_len = (int)strlen(project->source);
    find_len = (int)strlen(project->find_text);
    replace_len = (int)strlen(project->replace_text);
    project->source_cursor = ui_clampi(project->source_cursor, 0, source_len);
    hit = strstr(project->source + project->source_cursor,
                 project->find_text);
    if(hit == NULL)
        hit = strstr(project->source, project->find_text);
    if(hit == NULL) {
        snprintf(status, status_size, "No match: %s", project->find_text);
        return 0;
    }
    if(source_len - find_len + replace_len + 1 > EDITOR_SOURCE_MAX_BYTES) {
        snprintf(status, status_size, "Replacement is too large");
        return 0;
    }
    before = editor_strdup(project->source);
    if(before != NULL)
        editor_push_history(project->undo_items, &project->undo_count, before);
    free(before);
    memmove(hit + replace_len, hit + find_len,
            (size_t)(source_len - (int)(hit - project->source) - find_len + 1));
    memcpy(hit, project->replace_text, (size_t)replace_len);
    project->source_cursor = (int)(hit - project->source) + replace_len;
    project->source_dirty = 1;
    project->source_last_edit_time = GetTime();
    editor_scroll_to_cursor(project,
        editor_source_line_for_cursor(project->source, project->source_cursor));
    snprintf(status, status_size, "Replaced %s", project->find_text);
    return 1;
}

static void
editor_fill_find_from_selection(EditorProject *project)
{
    int start;
    int end;
    int len;

    if(project == NULL || !project->source_loaded)
        return;
    if(!GetUITextAreaSelection(1501, &start, &end))
        return;
    len = (int)strlen(project->source);
    start = ui_clampi(start, 0, len);
    end = ui_clampi(end, 0, len);
    if(end <= start)
        return;
    len = end - start;
    if(len >= (int)sizeof(project->find_text))
        len = (int)sizeof(project->find_text) - 1;
    for(int i = 0; i < len; i++) {
        if(project->source[start + i] == '\n' ||
           project->source[start + i] == '\r') {
            len = i;
            break;
        }
    }
    if(len <= 0)
        return;
    memcpy(project->find_text, project->source + start, (size_t)len);
    project->find_text[len] = '\0';
    project->find_cursor = len;
}

static void
editor_source_line_bounds(const char *text, int cursor, int *start, int *end)
{
    int len;
    int s;
    int e;

    if(start != NULL)
        *start = 0;
    if(end != NULL)
        *end = 0;
    if(text == NULL)
        return;
    len = (int)strlen(text);
    cursor = ui_clampi(cursor, 0, len);
    s = cursor;
    while(s > 0 && text[s - 1] != '\n')
        s--;
    e = cursor;
    while(e < len && text[e] != '\n')
        e++;
    if(start != NULL)
        *start = s;
    if(end != NULL)
        *end = e;
}

static int
editor_copy_source_range(const char *text, int start, int end)
{
    char *copy;
    int len;
    int text_len;

    if(text == NULL)
        return 0;
    text_len = (int)strlen(text);
    start = ui_clampi(start, 0, text_len);
    end = ui_clampi(end, 0, text_len);
    if(end <= start)
        return 0;
    len = end - start;
    copy = malloc((size_t)len + 1);
    if(copy == NULL)
        return 0;
    memcpy(copy, text + start, (size_t)len);
    copy[len] = '\0';
    SetClipboardText(copy);
    free(copy);
    return 1;
}

static int
editor_delete_source_range(EditorProject *project, int start, int end)
{
    int len;

    if(project == NULL)
        return 0;
    len = (int)strlen(project->source);
    start = ui_clampi(start, 0, len);
    end = ui_clampi(end, 0, len);
    if(end <= start)
        return 0;
    memmove(project->source + start, project->source + end,
            (size_t)(len - end + 1));
    project->source_cursor = start;
    SetUITextAreaSelection(1501, start, start);
    return 1;
}

static int
editor_insert_source_text(EditorProject *project, const char *text)
{
    int source_len;
    int insert_len;
    int cursor;

    if(project == NULL || text == NULL || text[0] == '\0')
        return 0;
    source_len = (int)strlen(project->source);
    insert_len = (int)strlen(text);
    cursor = ui_clampi(project->source_cursor, 0, source_len);
    if(source_len + insert_len + 1 > EDITOR_SOURCE_MAX_BYTES)
        insert_len = EDITOR_SOURCE_MAX_BYTES - source_len - 1;
    if(insert_len <= 0)
        return 0;
    memmove(project->source + cursor + insert_len,
            project->source + cursor,
            (size_t)(source_len - cursor + 1));
    memcpy(project->source + cursor, text, (size_t)insert_len);
    project->source_cursor = cursor + insert_len;
    SetUITextAreaSelection(1501, project->source_cursor,
                           project->source_cursor);
    return 1;
}

static int
editor_source_line_count(const char *text)
{
    int lines = 1;

    if(text == NULL || text[0] == '\0')
        return 1;
    for(const char *p = text; *p != '\0'; p++)
        if(*p == '\n')
            lines++;
    return lines;
}

static int
editor_handle_source_clipboard(EditorProject *project, int textarea_changed,
                               char *status, size_t status_size)
{
    int start = 0;
    int end = 0;
    int has_selection;

    if(project == NULL || !project->source_focused || !editor_mod_key_down())
        return 0;
    has_selection = GetUITextAreaSelection(1501, &start, &end);
    if(IsKeyPressed(KEY_C)) {
        if(!has_selection)
            editor_source_line_bounds(project->source, project->source_cursor,
                                      &start, &end);
        if(editor_copy_source_range(project->source, start, end)) {
            snprintf(status, status_size, "Copied source");
            return 1;
        }
    }
    if(IsKeyPressed(KEY_X)) {
        if(!has_selection)
            editor_source_line_bounds(project->source, project->source_cursor,
                                      &start, &end);
        if(editor_copy_source_range(project->source, start, end) &&
           editor_delete_source_range(project, start, end)) {
            snprintf(status, status_size, "Cut source");
            return 1;
        }
    }
    if(IsKeyPressed(KEY_V) && !textarea_changed) {
        const char *clip = GetClipboardText();

        if(has_selection)
            editor_delete_source_range(project, start, end);
        if(editor_insert_source_text(project, clip)) {
            snprintf(status, status_size, "Pasted source");
            return 1;
        }
    }
    return 0;
}

static void
draw_source_tabs(Rectangle bounds, EditorProject *project,
                 char *status, size_t status_size)
{
    int x = (int)bounds.x;
    int tab_h = (int)bounds.height;

    if(project == NULL || project->open_file_count <= 0)
        return;
    for(int i = 0; i < project->open_file_count; i++) {
        char label[128];
        int w;

        snprintf(label, sizeof(label), "%s%s",
                 i == project->active_open_file && project->source_dirty
                     ? "* "
                     : "",
                 path_basename(project->open_files[i].path));
        w = MeasureUIText(label, UI_TEXT_12) + ScaleUIPx(22);
        if(w < ScaleUIPx(72))
            w = ScaleUIPx(72);
        if(x + w > (int)(bounds.x + bounds.width))
            break;
        if(DrawUIGenericButton(x, (int)bounds.y, w, tab_h, label,
                               i == project->active_open_file
                                   ? UI_BUTTON_STYLE_PRIMARY
                                   : UI_BUTTON_STYLE_SECONDARY,
                               0, NULL)) {
            editor_open_file_at_line(project, project->open_files[i].path, 0,
                                     status, status_size);
            project->source_cursor = project->open_files[i].cursor;
            project->source_scroll_y = project->open_files[i].scroll_y;
        }
        x += w + ScaleUIPx(4);
    }
}

static void
draw_source_code(Rectangle content, EditorProject *project,
                 char *status, size_t status_size)
{
    char path[EDITOR_PATH_CAP];
    char *before_edit = NULL;
    int source_changed = 0;
    int textarea_changed = 0;
    int pad = ScaleUIPx(16);
    int tabs_h = ScaleUIPx(28);
    int toolbar_h = ScaleUIPx(30);
    int gap = ScaleUIPx(8);
    int find_h = project != NULL && project->find_replace_visible
                     ? toolbar_h + gap
                     : 0;
    int toolbar_y = (int)content.y + pad + tabs_h + gap;
    int find_y = toolbar_y + toolbar_h + gap;
    char zoom_text[16];
    Rectangle bounds = {
        content.x + (float)pad,
        content.y + (float)pad + (float)tabs_h + (float)gap +
            (float)toolbar_h + (float)gap + (float)find_h,
        content.width - (float)(pad * 2),
        content.height -
            (float)(pad * 2 + tabs_h + toolbar_h + gap * 2 + find_h)
    };

    DrawRectangleRec(content, GetThemeBackground());
    if(project == NULL || project->selected_file[0] == '\0') {
        draw_source_placeholder(content, project, 0);
        return;
    }
    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->selected_file);
    if(strcmp(project->source_scroll_file, project->selected_file) != 0) {
        int index;

        editor_store_active_open_file(project);
        if(project->source_dirty)
            editor_save_source_file(project, status, status_size);
        snprintf(project->source_scroll_file, sizeof(project->source_scroll_file),
                 "%s", project->selected_file);
        index = editor_add_open_file(project, project->selected_file);
        if(index >= 0) {
            project->active_open_file = index;
            project->source_scroll_y = project->open_files[index].scroll_y;
            project->source_cursor = project->open_files[index].cursor;
        } else {
            project->source_scroll_y = 0;
            project->source_cursor = 0;
        }
        project->source_focused = 0;
        project->source_dirty = 0;
        project->source_loaded = 0;
        project->selected_file_mtime = 0;
        project->last_source_file_check = 0.0;
        project->source_external_change_reported = 0;
        editor_free_history(project->undo_items, &project->undo_count);
        editor_free_history(project->redo_items, &project->redo_count);
    }
    if(!project->source_loaded &&
       !editor_read_text_file(path, project->source,
                              sizeof(project->source))) {
        DrawUIText("Could not open source file",
                   (int)content.x + pad, (int)content.y + pad,
                   UI_TEXT_16, GetThemeText());
        return;
    }
    project->source_loaded = 1;
    if(project->selected_file_mtime <= 0)
        project->selected_file_mtime = GetFileModTime(path);
    editor_maybe_reload_source_file(project, status, status_size);
    if(project->source_pending_valid) {
        int line_no;
        int line_h;

        project->source_cursor = project->source_pending_cursor;
        line_no = project->source_pending_line > 0
                      ? project->source_pending_line
                      : editor_source_line_for_cursor(project->source,
                                                      project->source_cursor);
        project->source_highlight_line = line_no;
        line_h = GetUITextLineHeight(editor_source_font_size(project)) +
                 ScaleUIPx(2);
        project->source_scroll_y = (line_no - 2) * line_h;
        if(project->source_scroll_y < 0)
            project->source_scroll_y = 0;
        project->source_pending_valid = 0;
    }
    snprintf(zoom_text, sizeof(zoom_text), "%dpx",
             editor_source_font_size(project));
    draw_source_tabs((Rectangle){content.x + (float)pad,
                                 content.y + (float)pad,
                                 content.width - (float)(pad * 2),
                                 (float)tabs_h},
                     project, status, status_size);
    if(DrawUIGenericButton((int)content.x + pad,
                           toolbar_y,
                           ScaleUIPx(30), toolbar_h, "-",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL))
        editor_adjust_source_zoom(project, -4, status, status_size);
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)((int)content.x + pad + ScaleUIPx(36)),
                   (float)toolbar_y,
                   (float)ScaleUIPx(58), (float)toolbar_h},
        .text = zoom_text,
        .style = editor_input_style()
    });
    if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(100),
                           toolbar_y,
                           ScaleUIPx(30), toolbar_h, "+",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL))
        editor_adjust_source_zoom(project, 4, status, status_size);
    if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(140),
                           toolbar_y,
                           ScaleUIPx(56), toolbar_h, "Find",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
        project->find_replace_visible = !project->find_replace_visible;
        if(project->find_replace_visible) {
            editor_fill_find_from_selection(project);
            project->find_focused = 1;
            project->source_focused = 0;
            SetUIFocus(1502);
        }
    }
    if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(204),
                           toolbar_y,
                           ScaleUIPx(58), toolbar_h, "Save",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL))
        editor_save_source_now(project, status, status_size);
    if(project->find_replace_visible) {
        DrawUITextField((UITextField){
            .bounds = {(float)((int)content.x + pad),
                       (float)find_y,
                       (float)ScaleUIPx(190), (float)toolbar_h},
            .text = project->find_text,
            .text_size = sizeof(project->find_text),
            .cursor_position = &project->find_cursor,
            .focused = &project->find_focused,
            .font = UI_TEXT_16,
            .focus_id = 1502,
            .style = editor_input_style()
        });
        if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(198),
                               find_y,
                               ScaleUIPx(54), toolbar_h, "Next",
                               UI_BUTTON_STYLE_SECONDARY,
                               project->find_text[0] == '\0', NULL))
            editor_find_next_source(project, status, status_size);
        DrawUITextField((UITextField){
            .bounds = {(float)((int)content.x + pad + ScaleUIPx(260)),
                       (float)find_y,
                       (float)ScaleUIPx(160), (float)toolbar_h},
            .text = project->replace_text,
            .text_size = sizeof(project->replace_text),
            .cursor_position = &project->replace_cursor,
            .focused = &project->replace_focused,
            .font = UI_TEXT_16,
            .focus_id = 1503,
            .style = editor_input_style()
        });
        if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(428),
                               find_y,
                               ScaleUIPx(76), toolbar_h, "Replace",
                               UI_BUTTON_STYLE_SECONDARY,
                               project->find_text[0] == '\0', NULL))
            editor_replace_next_source(project, status, status_size);
        if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(512),
                               find_y,
                               ScaleUIPx(56), toolbar_h, "Close",
                               UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
            project->find_replace_visible = 0;
            project->find_focused = 0;
            project->replace_focused = 0;
        }
    }
    {
        char state_text[96];
        int line_no = editor_source_line_for_cursor(project->source,
                                                    project->source_cursor);

        snprintf(state_text, sizeof(state_text), "%s  Ln %d",
                 project->source_dirty ? "Modified" : "Saved", line_no);
        DrawUIText(state_text,
                   (int)(content.x + content.width) - pad -
                       MeasureUIText(state_text, UI_TEXT_12),
                   toolbar_y + ScaleUIPx(8),
                   UI_TEXT_12,
                   project->source_dirty ? (Color){150, 74, 0, 255}
                                         : DarkenUIColor(GetThemeText(), 45));
    }
    if(bounds.height < ScaleUIPx(80))
        bounds.height = ScaleUIPx(80);
    before_edit = editor_strdup(project->source);
    if(project->source_focused && editor_mod_key_down()) {
        if(IsKeyPressed(KEY_S)) {
            editor_save_source_now(project, status, status_size);
        } else if(IsKeyPressed(KEY_F)) {
            project->find_replace_visible = 1;
            editor_fill_find_from_selection(project);
            project->source_focused = 0;
            project->find_focused = 1;
            SetUIFocus(1502);
            snprintf(status, status_size, "Find in file");
        } else if(IsKeyPressed(KEY_Z)) {
            if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                editor_redo_source(project, status, status_size);
            else
                editor_undo_source(project, status, status_size);
        } else if(IsKeyPressed(KEY_Y)) {
            editor_redo_source(project, status, status_size);
        }
    }
    textarea_changed = DrawUITextArea((UITextArea){
        .bounds = bounds,
        .text = project->source,
        .text_size = sizeof(project->source),
        .cursor_position = &project->source_cursor,
        .focused = &project->source_focused,
        .scroll_y = &project->source_scroll_y,
        .max_codepoints = EDITOR_SOURCE_MAX_BYTES - 1,
        .font = editor_source_font_size(project),
        .line_gap = ScaleUIPx(2),
        .focus_id = 1501,
        .placeholder = "",
        .syntax = editor_source_syntax(project->selected_file),
        .style = editor_input_style()
    });
    source_changed = textarea_changed;
    if(editor_handle_source_clipboard(project, textarea_changed, status,
                                      status_size))
        source_changed = 1;
    {
        int source_line_h = GetUITextLineHeight(editor_source_font_size(project)) +
                            ScaleUIPx(2);
        int content_h = editor_source_line_count(project->source) * source_line_h +
                        ScaleUIPx(16);
        int max_scroll = content_h - (int)bounds.height;

        if(max_scroll < 0)
            max_scroll = 0;
        DrawUIScrollbar((int)(bounds.x + bounds.width - ScaleUIPx(8)),
                        (int)bounds.y, (int)bounds.height, content_h,
                        &project->source_scroll_y, max_scroll);
    }
    if(source_changed) {
        if(before_edit != NULL)
            editor_push_history(project->undo_items, &project->undo_count,
                                before_edit);
        editor_free_history(project->redo_items, &project->redo_count);
        project->source_dirty = 1;
        project->source_last_edit_time = GetTime();
    }
    if(project->source_highlight_line > 0) {
        int line_h = GetUITextLineHeight(editor_source_font_size(project)) +
                     ScaleUIPx(2);
        int highlight_y = (int)bounds.y + ScaleUIPx(8) -
                          project->source_scroll_y +
                          (project->source_highlight_line - 1) * line_h;
        Rectangle line_rect = {
            bounds.x + ScaleUIPx(4),
            (float)highlight_y,
            bounds.width - ScaleUIPx(8),
            (float)line_h
        };

        if(line_rect.y + line_rect.height >= bounds.y &&
           line_rect.y <= bounds.y + bounds.height)
            DrawRectangleLinesEx(line_rect, ScaleUIPx(2),
                                 (Color){212, 146, 28, 255});
    }
    editor_store_active_open_file(project);
    free(before_edit);
}

static void
draw_image_file(Rectangle content, EditorProject *project)
{
    char path[EDITOR_PATH_CAP];
    int pad = ScaleUIPx(18);
    Rectangle view = {
        content.x + (float)pad,
        content.y + (float)pad,
        content.width - (float)(pad * 2),
        content.height - (float)(pad * 2)
    };
    float scale;
    float draw_w;
    float draw_h;
    Rectangle src;
    Rectangle dst;

    DrawRectangleRec(content, GetThemeBackground());
    if(project == NULL || project->selected_file[0] == '\0')
        return;

    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->selected_file);
    if(strcmp(project->image_file, project->selected_file) != 0) {
        if(project->image_texture.id != 0)
            UnloadTexture(project->image_texture);
        memset(&project->image_texture, 0, sizeof(project->image_texture));
        snprintf(project->image_file, sizeof(project->image_file), "%s",
                 project->selected_file);
        if(FileExists(path))
            project->image_texture = LoadTexture(path);
    }

    if(project->image_texture.id == 0) {
        DrawUIText("Could not open image",
                   (int)view.x, (int)view.y, UI_TEXT_16, GetThemeText());
        return;
    }

    DrawRectangleRec(view, GetThemeSurface());
    DrawRectangleLinesEx(view, 1, DarkenUIColor(GetThemeSurface(), 34));
    src = (Rectangle){0.0f, 0.0f,
                      (float)project->image_texture.width,
                      (float)project->image_texture.height};
    scale = view.width / (float)project->image_texture.width;
    if((float)project->image_texture.height * scale > view.height)
        scale = view.height / (float)project->image_texture.height;
    if(scale > 1.0f)
        scale = 1.0f;
    draw_w = (float)project->image_texture.width * scale;
    draw_h = (float)project->image_texture.height * scale;
    dst = (Rectangle){
        view.x + (view.width - draw_w) * 0.5f,
        view.y + (view.height - draw_h) * 0.5f,
        draw_w,
        draw_h
    };

    BeginScissorMode((int)view.x, (int)view.y,
                     (int)view.width, (int)view.height);
    DrawTexturePro(project->image_texture, src, dst, (Vector2){0}, 0.0f,
                   WHITE);
    EndScissorMode();
}

static void
draw_preview_context_menu(EditorProject *project, Rectangle preview,
                          char *status, size_t status_size)
{
    UIMenuItem items[] = {
        {UI_MENU_COMMAND, "Inspect", NULL, 1, 0, 0, NULL, 0},
        {UI_MENU_COMMAND, "Open Source", NULL, 2, 0, 0, NULL, 0}
    };
    int choice;

    if(project == NULL)
        return;
    choice = DrawUIContextMenu((UIContextMenu){
        .id = 2101,
        .trigger = preview,
        .items = items,
        .item_count = 2,
        .open = &project->inspect_menu_open,
        .x = &project->inspect_menu_x,
        .y = &project->inspect_menu_y
    });
    if(choice == 0)
        return;
    if(editor_open_active_screen_source(project)) {
        project->inspect_active = choice == 1;
        snprintf(status, status_size, choice == 1
                     ? "Inspecting %s"
                     : "Opened %s",
                 project->selected_file);
    } else {
        snprintf(status, status_size, "No source is available for this preview");
    }
}

static void
draw_preview_error(Rectangle content, EditorProject *project,
                   char *status, size_t status_size)
{
    int pad = ScaleUIPx(18);
    int y = (int)content.y + pad;
    int line_h = GetUITextLineHeight(UI_TEXT_12) + ScaleUIPx(3);
    int line_start = 0;
    int len;

    DrawRectangleRec(content, (Color){55, 32, 32, 255});
    DrawRectangleLinesEx(content, 1, (Color){166, 73, 73, 255});
    DrawUIText("Preview build failed", (int)content.x + pad, y,
               UI_TEXT_24, (Color){255, 226, 226, 255});
    y += ScaleUIPx(40);
    if(project == NULL || project->output[0] == '\0') {
        DrawUIText("Fix the Kry source and save to rebuild.",
                   (int)content.x + pad, y, UI_TEXT_16,
                   (Color){255, 226, 226, 255});
        return;
    }
    if(project->diagnostic_count > 0) {
        const EditorDiagnostic *d = &project->diagnostics[0];
        char diag[260];
        char rel[EDITOR_PATH_CAP];

        snprintf(diag, sizeof(diag), "%s:%d: %s",
                 d->path, d->line, d->message);
        DrawUIText(diag, (int)content.x + pad, y, UI_TEXT_16,
                   (Color){255, 226, 226, 255});
        editor_relative_path(rel, sizeof(rel), project, d->path);
        if(DrawUIGenericButton((int)content.x + pad, y + ScaleUIPx(24),
                               ScaleUIPx(86), ScaleUIPx(28), "Open",
                               UI_BUTTON_STYLE_SECONDARY, 0, NULL))
            editor_open_file_at_line(project, rel, d->line, status,
                                     status_size);
        y += ScaleUIPx(60);
    }
    len = (int)strlen(project->output);
    PushUIInputClip(content);
    for(int i = 0; i <= len && y < (int)(content.y + content.height - pad);
        i++) {
        if(project->output[i] == '\n' || project->output[i] == '\0') {
            char line[420];
            int line_len = i - line_start;

            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, project->output + line_start, (size_t)line_len);
            line[line_len] = '\0';
            DrawUIText(line, (int)content.x + pad, y, UI_TEXT_12,
                       (Color){255, 226, 226, 255});
            y += line_h;
            line_start = i + 1;
        }
    }
    PopUIInputClip();
}

static void
draw_preview_toolbar(Rectangle content, EditorProject *project,
                     char *status, size_t status_size)
{
    static const char *preset_labels[] = {
        "800x600", "Desktop", "Phone", "Tablet", "Square"
    };
    static const char *scale_labels[] = {"Fit", "100%", "75%", "50%"};
    int x = (int)content.x + ScaleUIPx(12);
    int y = (int)content.y + ScaleUIPx(7);
    int h = ScaleUIPx(28);
    int old_preset;
    int old_scale;

    if(project == NULL)
        return;
    editor_preview_init(project);
    old_preset = project->preview_preset;
    old_scale = (int)project->preview_scale_mode;
    DrawUIDropdownButton(1305, x, y, ScaleUIPx(108), h,
                         preset_labels, 5, &project->preview_preset);
    x += ScaleUIPx(116);
    DrawUIDropdownButton(1306, x, y, ScaleUIPx(82), h,
                         scale_labels, 4,
                         (int *)&project->preview_scale_mode);
    if(DrawUIDropdownMenu(1305) && old_preset != project->preview_preset) {
        editor_preview_apply_preset(project);
        snprintf(status, status_size, "Preview viewport: %s",
                 preset_labels[project->preview_preset]);
    }
    if(DrawUIDropdownMenu(1306) &&
       old_scale != (int)project->preview_scale_mode) {
        snprintf(status, status_size, "Preview scale: %s",
                 scale_labels[(int)project->preview_scale_mode]);
    }
    editor_preview_init(project);
}

static void
draw_preview_pane(Rectangle content, EditorProject *project, char *status,
                  size_t status_size)
{
    int x = (int)content.x + ScaleUIPx(28);
    int y = (int)content.y + ScaleUIPx(28);
    Rectangle stage;
    Rectangle device;
    Camera2D preview_camera = {0};
    Camera2D editor_camera;
    int inspect_transform = 0;
    int inspect_chrome = 0;
    int old_keyboard_enabled;
    int preview_keyboard_enabled;
    int old_view_w;
    int old_view_h;

    DrawRectangleRec(content, GetThemeBackground());
    if(project != NULL)
        editor_preview_init(project);
    if(project != NULL && project->loaded) {
        draw_preview_toolbar(content, project, status, status_size);
        stage = editor_preview_stage_rect(content);
        if(project->reload_failed) {
            draw_preview_error(stage, project, status, status_size);
            return;
        }
    }
    if(project == NULL || project->host == NULL) {
        DrawUIText("Kryon live preview unavailable", x, y, UI_TEXT_24,
                   GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Open a folder with .kry screens to preview them here.",
                   x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }
    if(!editor_selected_file_has_preview(project)) {
        DrawUIText("No preview for this file", x, y, UI_TEXT_24,
                   GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Select a .kry screen source to preview it here.",
                   x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }

    stage = editor_preview_stage_rect(content);
    device = editor_preview_device_rect(content, project);
    if(device.width <= 0.0f || device.height <= 0.0f)
        return;

    DrawRectangleRec(stage, DarkenUIColor(GetThemeBackground(), 7));
    DrawRectangleRec(device, GetThemeBackground());
    DrawRectangleLinesEx(device, 1, DarkenUIColor(GetThemeBackground(), 48));
    SetUIInspectCanvasBounds(device);
    BeginScissorMode((int)device.x, (int)device.y,
                     (int)device.width, (int)device.height);
    PushUIInputClip((Rectangle){0.0f, 0.0f,
                                (float)project->preview_width,
                                (float)project->preview_height});
    preview_camera.offset = (Vector2){device.x, device.y};
    preview_camera.target = (Vector2){0.0f, 0.0f};
    preview_camera.rotation = 0.0f;
    preview_camera.zoom = device.width / (float)project->preview_width;
    inspect_transform = PushUIInspectTransform(preview_camera);
    editor_camera = g_ui_camera;
    SetUIFrame(preview_camera);
    old_view_w = ui_view_width;
    old_view_h = ui_view_height;
    SetUIViewSize(project->preview_width, project->preview_height);
    preview_keyboard_enabled = project->preview_interact &&
                               !project->source_focused &&
                               !project->find_focused &&
                               !project->replace_focused &&
                               !project->search_focused;
    old_keyboard_enabled = SetUIKeyboardInputEnabled(preview_keyboard_enabled);
    if(project->preview_interact)
        inspect_chrome = PushUIInspectChrome(1);
    BeginMode2D(preview_camera);
    DrawAppScreen((AppHost *)project->host,
                  (Rectangle){0.0f, 0.0f,
                              (float)project->preview_width,
                              (float)project->preview_height});
    EndMode2D();
    if(project->preview_interact)
        PopUIInspectChrome(inspect_chrome);
    SetUIKeyboardInputEnabled(old_keyboard_enabled);
    SetUIViewSize(old_view_w, old_view_h);
    SetUIFrame(editor_camera);
    PopUIInputClip();
    PopUIInspectTransform(inspect_transform);
    EndScissorMode();
    draw_preview_context_menu(project, device, status, status_size);
}

static void
draw_source_pane(Rectangle content, EditorProject *project,
                 char *status, size_t status_size)
{
    int chrome = PushUIInspectChrome(1);

    if(project != NULL && editor_file_is_image(project->selected_file))
        draw_image_file(content, project);
    else
        draw_source_code(content, project, status, status_size);
    PopUIInspectChrome(chrome);
}

static void
draw_output_text(Rectangle bounds, EditorProject *project)
{
    int line_h = GetUITextLineHeight(UI_TEXT_12) + ScaleUIPx(2);
    int y = (int)bounds.y + ScaleUIPx(8) -
            (project != NULL ? project->output_scroll_y : 0);
    int line_start = 0;
    int len;

    if(project == NULL)
        return;
    DrawRectangleRec(bounds, DarkenUIColor(GetThemeSurface(), 3));
    DrawRectangleLinesEx(bounds, 1, DarkenUIColor(GetThemeSurface(), 36));
    PushUIInputClip(bounds);
    len = (int)strlen(project->output);
    for(int i = 0; i <= len; i++) {
        if(project->output[i] == '\n' || project->output[i] == '\0') {
            char line[1024];
            int line_len = i - line_start;

            if(line_len >= (int)sizeof(line))
                line_len = (int)sizeof(line) - 1;
            memcpy(line, project->output + line_start, (size_t)line_len);
            line[line_len] = '\0';
            if(y + line_h >= bounds.y && y <= bounds.y + bounds.height)
                DrawUIText(line, (int)bounds.x + ScaleUIPx(8), y,
                           UI_TEXT_12, GetThemeText());
            y += line_h;
            line_start = i + 1;
        }
    }
    PopUIInputClip();
    if(CheckCollisionPointRec(GetMousePosition(), bounds)) {
        project->output_scroll_y -=
            (int)(GetMouseWheelMove() * (float)line_h * 3.0f);
        if(project->output_scroll_y < 0)
            project->output_scroll_y = 0;
    }
}

static void
draw_output_panel(Rectangle bounds, EditorProject *project,
                  char *status, size_t status_size)
{
    int pad = ScaleUIPx(10);
    int header_h = ScaleUIPx(28);
    int diag_w = ScaleUIPx(360);
    Rectangle header = {bounds.x, bounds.y, bounds.width, (float)header_h};
    Rectangle diag;
    Rectangle output;

    if(project == NULL || !project->output_visible)
        return;
    DrawRectangleRec(bounds, GetThemeSurface());
    DrawRectangleLinesEx(bounds, 1, DarkenUIColor(GetThemeSurface(), 42));
    DrawRectangleRec(header, DarkenUIColor(GetThemeSurface(), 8));
    DrawUIText("Output", (int)bounds.x + pad,
               (int)bounds.y + ScaleUIPx(7), UI_TEXT_12, GetThemeText());
    if(project->diagnostic_count > 0) {
        char label[64];

        snprintf(label, sizeof(label), "%d diagnostics",
                 project->diagnostic_count);
        DrawUIText(label, (int)bounds.x + ScaleUIPx(92),
                   (int)bounds.y + ScaleUIPx(7), UI_TEXT_12,
                   (Color){150, 74, 0, 255});
    }
    if(DrawUIGenericButton((int)(bounds.x + bounds.width) - ScaleUIPx(66),
                           (int)bounds.y + ScaleUIPx(4),
                           ScaleUIPx(56), ScaleUIPx(21), "Close",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL)) {
        project->output_visible = 0;
        snprintf(status, status_size, "Output closed");
        return;
    }

    diag = (Rectangle){bounds.x + (float)pad,
                       bounds.y + (float)header_h + (float)pad,
                       (float)diag_w,
                       bounds.height - (float)header_h - (float)(pad * 2)};
    output = (Rectangle){diag.x + diag.width + (float)pad,
                         diag.y,
                         bounds.width - diag.width - (float)(pad * 3),
                         diag.height};
    if(project->diagnostic_count <= 0) {
        output.x = bounds.x + (float)pad;
        output.width = bounds.width - (float)(pad * 2);
    } else {
        int y = (int)diag.y + ScaleUIPx(6);
        int row_h = ScaleUIPx(26);

        DrawRectangleRec(diag, DarkenUIColor(GetThemeSurface(), 3));
        DrawRectangleLinesEx(diag, 1, DarkenUIColor(GetThemeSurface(), 36));
        PushUIInputClip(diag);
        for(int i = 0; i < project->diagnostic_count; i++) {
            char label[256];
            EditorDiagnostic *d = &project->diagnostics[i];

            snprintf(label, sizeof(label), "%s:%d %s",
                     path_basename(d->path), d->line, d->message);
            if(draw_sidebar_row((int)diag.x + ScaleUIPx(4), y,
                                (int)diag.width - ScaleUIPx(8), label,
                                i == project->selected_diagnostic)) {
                char rel[EDITOR_PATH_CAP];

                project->selected_diagnostic = i;
                editor_relative_path(rel, sizeof(rel), project, d->path);
                editor_open_file_at_line(project, rel, d->line,
                                         status, status_size);
            }
            y += row_h;
        }
        PopUIInputClip();
    }
    draw_output_text(output, project);
}

static void
draw_recent_projects_grid(Rectangle content, EditorProject *project,
                          EditorRecentProjects *recent,
                          EditorSidebarState *sidebar,
                          char *status, size_t status_size)
{
    int pad = ScaleUIPx(34);
    int x0 = (int)content.x + pad;
    int y = (int)content.y + pad;
    int gap = ScaleUIPx(14);
    int min_card_w = ScaleUIPx(220);
    int card_h = ScaleUIPx(78);
    int available_w = (int)content.width - pad * 2;
    int cols;
    int card_w;

    DrawRectangleRec(content, GetThemeBackground());
    DrawUIText("Recent Projects", x0, y, UI_TEXT_24, GetThemeText());
    y += ScaleUIPx(44);
    if(recent == NULL || recent->count == 0) {
        DrawUIText("Use Project / Open Project to choose an app folder.",
                   x0, y, UI_TEXT_16, GetThemeIcon());
        return;
    }
    cols = available_w / (min_card_w + gap);
    if(cols < 1)
        cols = 1;
    if(cols > 3)
        cols = 3;
    card_w = (available_w - gap * (cols - 1)) / cols;
    for(int i = 0; i < recent->count; i++) {
        int col = i % cols;
        int row = i / cols;
        int x = x0 + col * (card_w + gap);
        int card_y = y + row * (card_h + gap);
        Rectangle card = {(float)x, (float)card_y,
                          (float)card_w, (float)card_h};
        Vector2 mouse = GetMousePosition();
        int hovered = CheckCollisionPointRec(mouse, card);

        if(card_y + card_h > (int)(content.y + content.height - pad))
            break;
        DrawRectangleRec(card, hovered ? GetThemeButtonHover()
                                       : GetThemeSurface());
        DrawRectangleLinesEx(card, 1, DarkenUIColor(GetThemeSurface(), 42));
        DrawUIText(path_basename(recent->paths[i]),
                   x + ScaleUIPx(12), card_y + ScaleUIPx(12),
                   UI_TEXT_16, GetThemeText());
        DrawUIText(recent->paths[i],
                   x + ScaleUIPx(12), card_y + ScaleUIPx(42),
                   UI_TEXT_12, GetThemeIcon());
        if(hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            editor_open_project(project, recent->paths[i], recent,
                                status, status_size);
            if(sidebar != NULL)
                memset(sidebar, 0, sizeof(*sidebar));
        }
    }
}

static void
draw_canvas(Rectangle frame, Rectangle content, EditorProject *project,
            EditorRecentProjects *recent, EditorSidebarState *sidebar,
            char *status, size_t status_size)
{
    int header_h = editor_canvas_header_height();
    int title_x = (int)frame.x + ScaleUIPx(14);
    int divider = ScaleUIPx(6);
    int output_h = project != NULL && project->output_visible
                       ? ScaleUIPx(190)
                       : 0;
    int has_preview = project != NULL && editor_selected_file_has_preview(project);
    AppScreenInfo active_screen = {0};
    Rectangle work = content;
    Rectangle preview = content;
    Rectangle source = content;
    Rectangle output = {0};
    char title[EDITOR_PATH_CAP + 4];
    const char *title_text;

    DrawRectangleRec(frame, DarkenUIColor(GetThemeBackground(), 4));
    DrawRectangleLinesEx(frame, 1, DarkenUIColor(GetThemeBackground(), 38));
    DrawRectangle((int)frame.x, (int)frame.y, (int)frame.width, header_h,
                  GetThemeSurface());
    DrawLine((int)frame.x, (int)frame.y + header_h,
             (int)(frame.x + frame.width), (int)frame.y + header_h,
             DarkenUIColor(GetThemeSurface(), 38));

    if(project != NULL && project->host != NULL)
        active_screen = GetAppScreen(project->host, project->selected_screen);
    title_text = project != NULL && project->selected_file[0] != '\0'
                     ? project->selected_file
                     : (active_screen.title != NULL ? active_screen.title
                                                    : "Project");
    snprintf(title, sizeof(title), "%s%s",
             project != NULL && project->source_dirty ? "* " : "",
             title_text);
    DrawUIText(title, title_x, (int)frame.y + ScaleUIPx(14), UI_TEXT_16,
               GetThemeText());

    if(project == NULL || !project->loaded) {
        draw_recent_projects_grid(content, project, recent, sidebar,
                                  status, status_size);
        return;
    }

    if(output_h > 0 && output_h < (int)content.height - ScaleUIPx(120)) {
        work.height -= (float)output_h;
        output = (Rectangle){content.x,
                             content.y + work.height,
                             content.width,
                             (float)output_h};
    }

    if(project->layout_mode == EDITOR_LAYOUT_SOURCE ||
       (project->layout_mode == EDITOR_LAYOUT_SPLIT && !has_preview)) {
        source = work;
        draw_source_pane(source, project, status, status_size);
    } else if(project->layout_mode == EDITOR_LAYOUT_PREVIEW) {
        preview = work;
        draw_preview_pane(preview, project, status, status_size);
    } else {
        source = work;
        preview = work;
        source.width = (work.width - divider) * 0.5f;
        preview.x = source.x + source.width + divider;
        preview.width = work.width - source.width - divider;
        DrawRectangleRec((Rectangle){source.x + source.width, source.y,
                                     (float)divider, source.height},
                         DarkenUIColor(GetThemeBackground(), 32));
        draw_preview_pane(preview, project, status, status_size);
        editor_restore_ide_ui_frame(GetScreenWidth(), GetScreenHeight());
        draw_source_pane(source, project, status, status_size);
    }
    if(output_h > 0) {
        int chrome;

        editor_restore_ide_ui_frame(GetScreenWidth(), GetScreenHeight());
        chrome = PushUIInspectChrome(1);
        draw_output_panel(output, project, status, status_size);
        PopUIInspectChrome(chrome);
    }
}

static int
editor_preview_bounds(Rectangle content, const EditorProject *project,
                      Rectangle *preview_out)
{
    int divider = ScaleUIPx(6);
    int output_h = project != NULL && project->output_visible
                       ? ScaleUIPx(190)
                       : 0;
    Rectangle work = content;
    Rectangle source = content;
    Rectangle preview = content;

    if(preview_out != NULL)
        *preview_out = (Rectangle){0};
    if(project == NULL || !project->loaded ||
       !editor_selected_file_has_preview(project))
        return 0;
    if(project->layout_mode == EDITOR_LAYOUT_SOURCE)
        return 0;
    if(output_h > 0 && output_h < (int)content.height - ScaleUIPx(120))
        work.height -= (float)output_h;
    if(project->layout_mode == EDITOR_LAYOUT_PREVIEW) {
        preview = work;
    } else {
        source = work;
        preview = work;
        source.width = (work.width - divider) * 0.5f;
        preview.x = source.x + source.width + divider;
        preview.width = work.width - source.width - divider;
    }
    if(preview.width <= 0.0f || preview.height <= 0.0f)
        return 0;
    if(preview_out != NULL)
        *preview_out = editor_preview_device_rect(preview, project);
    if(preview_out != NULL &&
       (preview_out->width <= 0.0f || preview_out->height <= 0.0f))
        return 0;
    return 1;
}

static void
editor_restore_ide_ui_frame(int view_w, int view_h)
{
    SetUIViewSize(view_w, view_h);
    g_ui_camera = GetUIDefaultCamera();
    UseUIFont("default");
    SetUIScale(editor_ui_scale());
    SetCurrentTheme(THEME_MONO, 0);
    ClearUIInputCaptures();
    ResetUIClip();
}

static void
draw_top_bar(int view_w, int *project_menu_open, FileDialog *project_dialog,
             EditorProject *project, Texture2D play_icon, char *status_text,
             size_t status_size)
{
    int chrome = PushUIInspectChrome(1);
    int top_h = editor_top_bar_height();
    int menu_x = ScaleUIPx(10);
    int run_y = ScaleUIPx(7);
    int run_h = ScaleUIPx(28);
    int redo_x = view_w - ScaleUIPx(70);
    int undo_x = view_w - ScaleUIPx(132);
    int run_x = view_w - ScaleUIPx(186);
    int target_x = view_w - ScaleUIPx(332);
    int target_w = ScaleUIPx(138);
    int layout_x = view_w - ScaleUIPx(596);
    int layout_w = ScaleUIPx(132);
    int preview_mode_x = view_w - ScaleUIPx(720);
    int preview_mode_w = ScaleUIPx(116);
    const char *layout_labels[] = {"Split", "Source", "Preview"};
    const char *preview_mode_labels[] = {"Inspect", "Interact"};
    const char *target_labels[EDITOR_MAX_RUN_TARGETS];
    int target_count = 0;
    int old_target = project != NULL ? project->selected_run_target : 0;

    DrawRectangle(0, 0, view_w, top_h, DarkenUIColor(GetThemeBackground(), 12));
    DrawLine(0, top_h - 1, view_w, top_h - 1,
             DarkenUIColor(GetThemeBackground(), 42));
    if(draw_menu_button(menu_x, ScaleUIPx(9), "Project", *project_menu_open)) {
        *project_menu_open = !*project_menu_open;
    }
    if(project != NULL && project->loaded) {
        int selected_layout = (int)project->layout_mode;

        DrawUIDropdownButton(1303, layout_x, run_y, layout_w, run_h,
                             layout_labels, 3, &selected_layout);
        if(selected_layout >= 0 && selected_layout < 3)
            project->layout_mode = (EditorLayoutMode)selected_layout;
        if(editor_selected_file_has_preview(project)) {
            int selected_preview_mode = project->preview_interact ? 1 : 0;

            DrawUIDropdownButton(1304, preview_mode_x, run_y,
                                 preview_mode_w, run_h,
                                 preview_mode_labels, 2,
                                 &selected_preview_mode);
            project->preview_interact = selected_preview_mode == 1;
        }
    }
    if(DrawUIGenericButton(view_w - ScaleUIPx(454), run_y,
                           ScaleUIPx(112), run_h, "Open Project",
                           UI_BUTTON_STYLE_PRIMARY, 0, NULL))
        BeginSelectFileDialogFolder(project_dialog, "Open Project");
    SetUIDropdownClipTop(top_h);
    SetUIDropdownClipBottom(GetScreenHeight() - editor_bottom_bar_height());
    if(project != NULL && project->loaded) {
        target_count = project->run_target_count;
        if(target_count > EDITOR_MAX_RUN_TARGETS)
            target_count = EDITOR_MAX_RUN_TARGETS;
        for(int i = 0; i < target_count; i++)
            target_labels[i] = project->run_targets[i].label;
        DrawUIDropdownButton(1301, target_x, run_y, target_w, run_h,
                             target_labels, target_count,
                             &project->selected_run_target);
    } else {
        const char *targets[] = {"Native"};
        int selected = 0;
        DrawUIDropdownButton(1301, target_x, run_y, target_w, run_h,
                             targets, 1, &selected);
    }
    if(DrawUIIconButton((UIIconButton){
        .bounds = {(float)run_x, (float)run_y, (float)ScaleUIPx(46),
                   (float)run_h},
        .icon = play_icon,
        .icon_type = UI_ICON_TYPE_PLAY,
        .icon_size = ScaleUIPx(16),
        .focus_id = 1302,
        .disabled = project == NULL || !project->loaded,
        .background = project != NULL && project->loaded
                          ? GetThemeButton()
                          : DarkenUIColor(GetThemeButton(), 20),
        .hover_background = GetThemeButtonHover(),
        .icon_color = GetThemeText()
    }))
        editor_run_project(project, status_text, status_size);
    if(DrawUIGenericButton(undo_x, run_y, ScaleUIPx(56), run_h, "Undo",
                           UI_BUTTON_STYLE_SECONDARY,
                           project == NULL || project->undo_count <= 0,
                           NULL))
        editor_undo_source(project, status_text, status_size);
    if(DrawUIGenericButton(redo_x, run_y, ScaleUIPx(56), run_h, "Redo",
                           UI_BUTTON_STYLE_SECONDARY,
                           project == NULL || project->redo_count <= 0,
                           NULL))
        editor_redo_source(project, status_text, status_size);
    if(DrawUIDropdownMenu(1301) && project != NULL && project->loaded &&
       project->selected_run_target >= 0 &&
       project->selected_run_target < project->run_target_count &&
       project->selected_run_target != old_target) {
        snprintf(status_text, status_size, "Run target: %s",
                 project->run_targets[project->selected_run_target].label);
    }
    if(DrawUIDropdownMenu(1303) && project != NULL && project->loaded) {
        snprintf(status_text, status_size, "Layout: %s",
                 layout_labels[(int)project->layout_mode]);
    }
    if(DrawUIDropdownMenu(1304) && project != NULL && project->loaded &&
       editor_selected_file_has_preview(project)) {
        snprintf(status_text, status_size, "Preview mode: %s",
                 preview_mode_labels[project->preview_interact ? 1 : 0]);
    }
    if(project != NULL && project->loaded && !project->output_visible &&
       project->output[0] != '\0') {
        if(DrawUIGenericButton(view_w - ScaleUIPx(780), run_y,
                               ScaleUIPx(56), run_h, "Output",
                               UI_BUTTON_STYLE_SECONDARY, 0, NULL))
            project->output_visible = 1;
    }
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);
    PopUIInspectChrome(chrome);
}

static void
draw_chrome(int view_w, int view_h, EditorProject *project,
            EditorRecentProjects *recent, EditorSidebarState *sidebar,
            int *project_menu_open, FileDialog *project_dialog,
            Texture2D play_icon,
            char *status_text, size_t status_size)
{
    int chrome = PushUIInspectChrome(1);
    int top_h = editor_top_bar_height();
    int collapsed_w = ScaleUIPx(42);
    int side_w = sidebar != NULL && sidebar->collapsed
                     ? collapsed_w
                     : ScaleUIPx(320);
    int bottom_h = editor_bottom_bar_height();
    int menu_x = ScaleUIPx(10);
    int menu_y = top_h - ScaleUIPx(2);

    draw_top_bar(view_w, project_menu_open, project_dialog, project, play_icon,
                 status_text, status_size);
    if(project_menu_open != NULL && *project_menu_open)
        PushUIInputCapture((Rectangle){0.0f, 0.0f, (float)view_w,
                                       (float)view_h}, 0);
    if(sidebar != NULL && sidebar->collapsed) {
        Rectangle strip = {0, (float)top_h, (float)side_w,
                           (float)(view_h - top_h - bottom_h)};

        DrawRectangleRec(strip, GetThemeSurface());
        DrawRectangleLinesEx(strip, 1, DarkenUIColor(GetThemeSurface(), 36));
        if(DrawUIGenericButton(ScaleUIPx(7), top_h + ScaleUIPx(8),
                               ScaleUIPx(28), ScaleUIPx(24), ">",
                               UI_BUTTON_STYLE_SECONDARY, 0, NULL))
            sidebar->collapsed = 0;
    } else {
        draw_left_sidebar((Rectangle){0, (float)top_h, (float)side_w,
                                      (float)(view_h - top_h - bottom_h)},
                          project, recent, sidebar, status_text, status_size);
    }
    if(project_menu_open != NULL && *project_menu_open)
        ClearUIInputCaptures();

    DrawRectangle(0, view_h - bottom_h, view_w, bottom_h,
                  DarkenUIColor(GetThemeBackground(), 14));
    DrawLine(0, view_h - bottom_h, view_w, view_h - bottom_h,
             DarkenUIColor(GetThemeBackground(), 42));
    DrawUIText(status_text, ScaleUIPx(12), view_h - bottom_h + ScaleUIPx(7),
               UI_TEXT_12, GetThemeText());
    draw_project_menu(menu_x, menu_y, project_menu_open, project_dialog,
                      project, status_text, status_size);
    PopUIInspectChrome(chrome);
}

static int
run_screen_smoke(EditorProject *project)
{
    Rectangle canvas = {260.0f, 100.0f, 1140.0f, 800.0f};
    int count;

    if(project == NULL || project->host == NULL)
        return 1;
    count = GetAppScreenCount(project->host);
    TraceLog(LOG_INFO, "KRYON_SMOKE: %d screens", count);
    for(int i = 0; i < count; i++) {
        AppScreenInfo screen = GetAppScreen(project->host, i);
        const char *title = screen.title != NULL ? screen.title : screen.id;

        project->selected_screen = i;
        SetAppScreen(project->host, i);
        TraceLog(LOG_INFO, "KRYON_SMOKE: screen %d %s", i,
                 title != NULL ? title : "screen");
        for(int frame = 0; frame < 2 && !WindowShouldClose(); frame++) {
            BeginDrawing();
            ClearBackground(GetThemeBackground());
            BeginUIFrame(GetScreenWidth(), GetScreenHeight(), editor_ui_scale());
            BeginUIInspectFrame(project->path);
            SetUIInspectCanvasBounds(canvas);
            PushUIInputClip(canvas);
            DrawAppScreen(project->host, canvas);
            PopUIInputClip();
            DrawUIInspectOverlay();
            EndUIFocus();
            EndDrawing();
        }
    }
    return 0;
}

static int
color_delta(Color a, Color b)
{
    int dr = (int)a.r - (int)b.r;
    int dg = (int)a.g - (int)b.g;
    int db = (int)a.b - (int)b.b;

    if(dr < 0)
        dr = -dr;
    if(dg < 0)
        dg = -dg;
    if(db < 0)
        db = -db;
    return dr + dg + db;
}

static int
count_region_detail_pixels(Image image, Rectangle region)
{
    Color *pixels;
    Color base;
    int x0;
    int y0;
    int x1;
    int y1;
    int count = 0;

    if(image.data == NULL || image.width <= 0 || image.height <= 0)
        return 0;
    x0 = ui_clampi((int)region.x, 0, image.width - 1);
    y0 = ui_clampi((int)region.y, 0, image.height - 1);
    x1 = ui_clampi((int)(region.x + region.width), x0 + 1, image.width);
    y1 = ui_clampi((int)(region.y + region.height), y0 + 1, image.height);
    pixels = LoadImageColors(image);
    if(pixels == NULL)
        return 0;
    base = pixels[y0 * image.width + x0];
    for(int y = y0; y < y1; y++) {
        for(int x = x0; x < x1; x++) {
            Color pixel = pixels[y * image.width + x];

            if(color_delta(pixel, base) > 48)
                count++;
        }
    }
    UnloadImageColors(pixels);
    return count;
}

static int
run_ide_smoke(EditorProject *project, EditorRecentProjects *recent,
              EditorSidebarState *sidebar, FileDialog *project_dialog,
              Texture2D play_icon, const char *screenshot_path)
{
    int view_w = GetScreenWidth();
    int view_h = GetScreenHeight();
    int top_h = editor_top_bar_height();
    int left_w = sidebar != NULL && sidebar->collapsed ? ScaleUIPx(42) : ScaleUIPx(320);
    int bottom_h = editor_bottom_bar_height();
    int canvas_header_h = editor_canvas_header_height();
    int divider = ScaleUIPx(6);
    int detail_pixels;
    int inspect_widgets = 0;
    int source_pick_ok = 0;
    char status_text[160] = "IDE smoke";
    Rectangle canvas_frame = {
        (float)left_w,
        (float)top_h,
        (float)(view_w - left_w),
        (float)(view_h - top_h - bottom_h)
    };
    Rectangle canvas_content = {
        canvas_frame.x,
        canvas_frame.y + canvas_header_h,
        canvas_frame.width,
        canvas_frame.height - canvas_header_h
    };
    Rectangle source = canvas_content;
    Rectangle preview = canvas_content;
    Rectangle sample;
    Image image;

    if(project == NULL || !project->loaded || !editor_selected_file_has_preview(project)) {
        TraceLog(LOG_ERROR, "KRYON_SMOKE_IDE: selected file has no preview");
        return 1;
    }
    source.width = (canvas_content.width - (float)divider) * 0.5f;
    preview.x = source.x + source.width + (float)divider;
    preview.width = canvas_content.width - source.width - (float)divider;
    sample = (Rectangle){
        preview.x + ScaleUIPx(24),
        preview.y + ScaleUIPx(24),
        preview.width - ScaleUIPx(48),
        preview.height - ScaleUIPx(48)
    };

    for(int frame = 0; frame < 3 && !WindowShouldClose(); frame++) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(view_w, view_h, editor_ui_scale());
        BeginUIInspectFrame(project->path);
        SetUIInspectCanvasBounds(canvas_content);
        draw_canvas(canvas_frame, canvas_content, project, recent, sidebar,
                    status_text, sizeof(status_text));
        if(editor_selected_file_has_preview(project))
            DrawUIInspectOverlay();
        editor_restore_ide_ui_frame(view_w, view_h);
        draw_chrome(view_w, view_h, project, recent, sidebar, &(int){0},
                    project_dialog, play_icon, status_text,
                    sizeof(status_text));
        EndUIFocus();
        EndDrawing();
    }
    inspect_widgets = UIInspectWidgetCount();
    for(int py = (int)sample.y; py < (int)(sample.y + sample.height) && !source_pick_ok;
        py += ScaleUIPx(24)) {
        for(int px = (int)sample.x;
            px < (int)(sample.x + sample.width) && !source_pick_ok;
            px += ScaleUIPx(24)) {
            UIInspectSelection selection;

            if(!UIInspectSelectAt((Vector2){(float)px, (float)py}))
                continue;
            selection = UIInspectGetSelection();
            if(selection.valid && selection.source_line > 0 &&
               selection.source_path[0] != '\0' &&
               editor_jump_to_active_widget_source(project, &selection,
                                                   status_text,
                                                   sizeof(status_text)))
                source_pick_ok = 1;
        }
    }
    if(source_pick_ok && !WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(view_w, view_h, editor_ui_scale());
        BeginUIInspectFrame(project->path);
        SetUIInspectCanvasBounds(canvas_content);
        draw_canvas(canvas_frame, canvas_content, project, recent, sidebar,
                    status_text, sizeof(status_text));
        if(editor_selected_file_has_preview(project))
            DrawUIInspectOverlay();
        editor_restore_ide_ui_frame(view_w, view_h);
        draw_chrome(view_w, view_h, project, recent, sidebar, &(int){0},
                    project_dialog, play_icon, status_text,
                    sizeof(status_text));
        EndUIFocus();
        EndDrawing();
    }

    image = LoadImageFromScreen();
    if(screenshot_path != NULL && screenshot_path[0] != '\0')
        ExportImage(image, screenshot_path);
    detail_pixels = count_region_detail_pixels(image, sample);
    TraceLog(LOG_INFO, "KRYON_SMOKE_IDE: screenshot %s", screenshot_path);
    TraceLog(LOG_INFO, "KRYON_SMOKE_IDE: selected %s preview detail pixels %d",
             project->selected_file, detail_pixels);
    TraceLog(LOG_INFO, "KRYON_SMOKE_IDE: inspect widgets %d source pick %s",
             inspect_widgets, source_pick_ok ? "ok" : "failed");
    UnloadImage(image);
    if(detail_pixels < 2500) {
        TraceLog(LOG_ERROR, "KRYON_SMOKE_IDE: preview pane looks blank");
        return 1;
    }
    if(inspect_widgets <= 0 || !source_pick_ok) {
        TraceLog(LOG_ERROR,
                 "KRYON_SMOKE_IDE: preview widget source inspection failed");
        return 1;
    }
    return 0;
}

int
main(int argc, char **argv)
{
    const int screen_w = 1400;
    const int screen_h = 900;
    const UIIconAsset *window_icon_asset;
    Image window_icon = {0};
    int project_menu_open = 0;
    FileDialog project_dialog;
    EditorProject project = {0};
    EditorRecentProjects recent;
    EditorSidebarState sidebar = {0};
    Texture2D play_icon = {0};
    char status_text[160] = "Ready";
    int smoke_screens = 0;
    int smoke_ide = 0;
    const char *project_arg = NULL;
    const char *smoke_screenshot_path = "/tmp/kryon-ide-smoke.png";
    char project_path[EDITOR_PATH_CAP];

    if(argc > 1 && strcmp(argv[1], "--smoke-screens") == 0) {
        smoke_screens = 1;
        if(argc > 2)
            project_arg = argv[2];
    } else if(argc > 1 && strcmp(argv[1], "--smoke-ide") == 0) {
        smoke_ide = 1;
        if(argc > 2)
            project_arg = argv[2];
        if(argc > 3)
            smoke_screenshot_path = argv[3];
    } else if(argc > 1) {
        project_arg = argv[1];
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_w, screen_h, "Kryon IDE");
    window_icon_asset = GetUIIconAsset(UI_ICON_TYPE_KRYON);
    if(window_icon_asset != NULL) {
        window_icon = LoadImageFromMemory(".png", window_icon_asset->data,
                                          (int)window_icon_asset->size);
        if(window_icon.data != NULL) {
            SetWindowIcon(window_icon);
            UnloadImage(window_icon);
        }
    }
    SetTargetFPS(60);
    load_editor_font();
    play_icon = LoadUIIconTexture(UI_ICON_TYPE_PLAY);
    InitUI(screen_w, screen_h, editor_ui_scale());
    SetCurrentTheme(THEME_MONO, 0);
    SetUIInspectEnabled(1);
    InitFileDialog(&project_dialog);
    editor_load_recent_projects(&recent);

    if(project_arg != NULL && project_arg[0] != '\0') {
        if(editor_resolve_project_path(project_path, sizeof(project_path),
                                       project_arg)) {
            if(path_ext_eq(project_path, ".kry")) {
                char project_dir[EDITOR_PATH_CAP];
                const char *file_name = path_basename(project_path);

                path_dirname(project_dir, sizeof(project_dir), project_path);
                editor_open_project(&project, project_dir, &recent,
                                    status_text, sizeof(status_text));
                if(project.loaded) {
                    editor_select_file(&project, file_name);
                    editor_select_source_path(&project, file_name);
                    snprintf(status_text, sizeof(status_text), "Opened %s",
                             file_name);
                }
            } else {
                editor_open_project(&project, project_path, &recent,
                                    status_text, sizeof(status_text));
            }
        } else {
            snprintf(status_text, sizeof(status_text),
                     "Could not open project path: %s", project_arg);
        }
        if(smoke_screens && project.host == NULL)
            TraceLog(LOG_ERROR, "KRYON_SMOKE: %s", status_text);
    }
    SetFileDialogCurrentDir(&project_dialog,
                            project.loaded ? project.path : GetWorkingDirectory());

    if(smoke_screens) {
        int result = run_screen_smoke(&project);
        editor_close_project(&project);
        if(play_icon.id != 0)
            UnloadTexture(play_icon);
        ClearUIFonts();
        CloseFileDialog(&project_dialog);
        CloseWindow();
        return result;
    }
    if(smoke_ide) {
        int result = run_ide_smoke(&project, &recent, &sidebar,
                                   &project_dialog, play_icon,
                                   smoke_screenshot_path);
        editor_close_project(&project);
        if(play_icon.id != 0)
            UnloadTexture(play_icon);
        ClearUIFonts();
        CloseFileDialog(&project_dialog);
        CloseWindow();
        return result;
    }

    while(!WindowShouldClose()) {
        int view_w = GetScreenWidth();
        int view_h = GetScreenHeight();
        int top_h = editor_top_bar_height();
        int left_w = sidebar.collapsed ? ScaleUIPx(42) : ScaleUIPx(320);
        int bottom_h = editor_bottom_bar_height();
        int canvas_header_h = editor_canvas_header_height();
        int dialog_result;
        Rectangle canvas_frame = {
            (float)left_w,
            (float)top_h,
            (float)(view_w - left_w),
            (float)(view_h - top_h - bottom_h)
        };
        Rectangle canvas_content = {
            canvas_frame.x,
            canvas_frame.y + canvas_header_h,
            canvas_frame.width,
            canvas_frame.height - canvas_header_h
        };
        Rectangle preview_bounds = {0};
        int preview_bounds_valid;

        BeginDrawing();
        ClearBackground(GetThemeBackground());
        BeginUIFrame(view_w, view_h, editor_ui_scale());
        BeginUIInspectFrame(project.loaded ? project.path : ".");
        dialog_result = UpdateFileDialog(&project_dialog);
        if(dialog_result == 1) {
            editor_open_project(&project, GetFileDialogPath(&project_dialog),
                                &recent, status_text, sizeof(status_text));
            SetFileDialogCurrentDir(&project_dialog, project.path);
            project_dialog.confirmed = 0;
            project_dialog.result_path[0] = '\0';
        }
        if(project.loaded && editor_mod_key_down() &&
           (IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)) &&
           IsKeyPressed(KEY_F)) {
            project.search_focused = 1;
            project.source_focused = 0;
            SetUIFocus(1211);
            if(project.search_text[0] != '\0')
                editor_search_project(&project, status_text,
                                      sizeof(status_text));
            else
                snprintf(status_text, sizeof(status_text), "Search project");
        }
        if(project.loaded && editor_mod_key_down() && IsKeyPressed(KEY_P)) {
            project.search_focused = 1;
            project.source_focused = 0;
            SetUIFocus(1211);
            snprintf(status_text, sizeof(status_text), "Quick open/search");
        }
        editor_maybe_autosave_source(&project, status_text,
                                     sizeof(status_text));
        editor_maybe_reload_live_module(&project, status_text,
                                        sizeof(status_text));

        SetUIInspectCanvasBounds(canvas_content);
        preview_bounds_valid =
            editor_preview_bounds(canvas_content, &project, &preview_bounds);
        draw_canvas(canvas_frame, canvas_content, &project, &recent, &sidebar,
                    status_text, sizeof(status_text));
        if(editor_selected_file_has_preview(&project) &&
           !project.preview_interact && preview_bounds_valid) {
            Vector2 mouse = GetMousePosition();

            SetUIInspectCanvasBounds(preview_bounds);
            DrawUIInspectOverlay();
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT) &&
               !IsKeyDown(KEY_LEFT_ALT) && !IsKeyDown(KEY_RIGHT_ALT) &&
               CheckCollisionPointRec(mouse, preview_bounds)) {
                UIInspectSelection selection = UIInspectGetSelection();

                if(UIInspectSelectAt(mouse))
                    selection = UIInspectGetSelection();
                else
                    selection.valid = 0;
                if(selection.valid)
                    editor_jump_to_active_widget_source(&project, &selection,
                                                        status_text,
                                                        sizeof(status_text));
            }
        }
        editor_restore_ide_ui_frame(view_w, view_h);
        if(CheckCollisionPointRec(GetMousePosition(), canvas_content))
            PushUIInputCapture(canvas_content, 1);
        draw_chrome(view_w, view_h, &project, &recent, &sidebar,
                    &project_menu_open, &project_dialog, play_icon,
                    status_text, sizeof(status_text));

        EndUIFocus();
        EndDrawing();
    }

    editor_close_project(&project);
    if(play_icon.id != 0)
        UnloadTexture(play_icon);
    ClearUIFonts();
    CloseFileDialog(&project_dialog);
    CloseWindow();
    return 0;
}

#include "file_dialog.h"
#include "kryon.h"
#include "theme.h"
#include "ui.h"
#include "ui_internal.h"

#if !defined(_WIN32)
#include <dirent.h>
#include <dlfcn.h>
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
    EDITOR_HISTORY_MAX = 32,
    EDITOR_TREE_DEPTH = 8,
    EDITOR_SOURCE_MAX_BYTES = 512 * 1024,
};

typedef struct EditorRunTarget {
    char label[48];
    char name[48];
    char command[EDITOR_PATH_CAP];
} EditorRunTarget;

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
    char *undo_items[EDITOR_HISTORY_MAX];
    char *redo_items[EDITOR_HISTORY_MAX];
    int undo_count;
    int redo_count;
    char image_file[EDITOR_PATH_CAP];
    Texture2D image_texture;
    char host_path[EDITOR_PATH_CAP];
    int loaded;
    int selected_screen;
    char host_rel_path[EDITOR_PATH_CAP];
    char build_host_command[EDITOR_PATH_CAP];
    EditorRunTarget run_targets[EDITOR_MAX_RUN_TARGETS];
    int run_target_count;
    int selected_run_target;
    long host_mtime;
    long source_mtime;
    double last_reload_check;
    int reload_failed;
    int inspect_active;
    int inspect_menu_open;
    int inspect_menu_x;
    int inspect_menu_y;
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
    } candidates[] = {
        {"Native", "native", "run"},
        {"Web", "web", "web"},
        {"Android Debug", "android-debug", "android-debug"},
        {"Android Release", "android-release", "android-release"},
        {"Windows", "windows", "windows"},
        {"AppImage", "appimage", "appimage"},
        {"Click", "click", "click"},
        {"Snap", "snap", "snap"},
        {"Flatpak", "flatpak", "flatpak"}
    };

    if(project == NULL)
        return;
    project->selected_run_target = 0;
    if(project->run_target_count > 0)
        return;
    for(size_t i = 0; i < sizeof(candidates) / sizeof(candidates[0]); i++) {
        if(editor_project_has_make_target(project, candidates[i].make_target)) {
            char command[96];

            snprintf(command, sizeof(command), "make %s",
                     strcmp(candidates[i].make_target, "run") == 0
                         ? "run"
                         : candidates[i].make_target);
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
    snprintf(project->host_rel_path, sizeof(project->host_rel_path),
             "build/kryon/editor_host.so");
    snprintf(project->build_host_command, sizeof(project->build_host_command),
             "make kryon-host");
    project->run_target_count = 0;

    path_join(path, sizeof(path), project->path, "project.kryon");
    file = fopen(path, "r");
    if(file == NULL)
        return;
    while(fgets(line, sizeof(line), file) != NULL) {
        char *trimmed = editor_trim_line(line);
        char label[48];
        char name[48];
        char command[EDITOR_PATH_CAP];

        if(trimmed[0] == '\0' || trimmed[0] == '#')
            continue;
        if(editor_parse_quoted_value(trimmed, "name", project->name,
                                     sizeof(project->name)))
            continue;
        if(editor_parse_quoted_value(trimmed, "host", project->host_rel_path,
                                     sizeof(project->host_rel_path)))
            continue;
        if(editor_parse_quoted_value(trimmed, "build_host",
                                     project->build_host_command,
                                     sizeof(project->build_host_command)))
            continue;
        if(editor_parse_three_quoted_values(trimmed, "run_target",
                                            label, sizeof(label),
                                            name, sizeof(name),
                                            command, sizeof(command))) {
            editor_add_run_target(project, label, name, command);
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
    return path_ext_eq(project->selected_file, ".kry") &&
           editor_source_path_has_screen(project, project->selected_file);
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
    snprintf(project->selected_file, sizeof(project->selected_file), "%s",
             source_path);
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

static void
editor_set_project(EditorProject *project, const char *path)
{
    if(project == NULL || path == NULL || path[0] == '\0')
        return;

    memset(project, 0, sizeof(*project));
    snprintf(project->path, sizeof(project->path), "%s", path);
    snprintf(project->name, sizeof(project->name), "%s", path_basename(path));
    project->loaded = 1;
    editor_load_project_config(project);
    editor_detect_run_targets(project);
}

static void
editor_unload_project_host(EditorProject *project)
{
    if(project == NULL)
        return;
    if(project->destroy_host != NULL && project->host != NULL)
        project->destroy_host(project->host);
#if !defined(_WIN32)
    if(project->host_library != NULL)
        dlclose(project->host_library);
#endif
    project->host = NULL;
    project->host_library = NULL;
    project->destroy_host = NULL;
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
    editor_unload_project_host(project);
    memset(project, 0, sizeof(*project));
}

static int
editor_build_project_host(EditorProject *project, char *status,
                          size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Editor host build is not implemented on Windows yet");
    return 0;
#else
    char quoted_path[EDITOR_PATH_CAP + 16];
    char command[EDITOR_PATH_CAP * 2 + 96];

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }

    path_join(project->host_path, sizeof(project->host_path), project->path,
              project->host_rel_path);
    shell_quote(quoted_path, sizeof(quoted_path), project->path);
    snprintf(command, sizeof(command), "cd %s && %s",
             quoted_path, project->build_host_command);
    if(system(command) != 0 || !FileExists(project->host_path)) {
        snprintf(status, status_size, "Could not build editor host");
        return 0;
    }
    snprintf(status, status_size, "Built editor host");
    return 1;
#endif
}

static int
editor_load_project_host(EditorProject *project, char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Editor host loading is not implemented on Windows yet");
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

    path_join(project->host_path, sizeof(project->host_path), project->path,
              project->host_rel_path);
    if(!FileExists(project->host_path) &&
       !editor_build_project_host(project, status, status_size))
        return 0;
    snprintf(selected_file, sizeof(selected_file), "%s", project->selected_file);

    project->host_library = dlopen(project->host_path, RTLD_NOW | RTLD_LOCAL);
    if(project->host_library == NULL) {
        snprintf(status, status_size, "Host load failed: %s", dlerror());
        return 0;
    }

    dlerror();
    symbol = dlsym(project->host_library, "CreateAppHost");
    error = dlerror();
    if(error != NULL || symbol == NULL) {
        snprintf(status, status_size, "Host is missing CreateAppHost");
        dlclose(project->host_library);
        project->host_library = NULL;
        return 0;
    }
    create_host = (CreateAppHostCallback)symbol;

    dlerror();
    symbol = dlsym(project->host_library, "DestroyAppHost");
    error = dlerror();
    if(error == NULL && symbol != NULL)
        project->destroy_host = (DestroyAppHostCallback)symbol;

    project->host = create_host(APP_HOST_ABI_VERSION, project->path);
    if(project->host == NULL) {
        snprintf(status, status_size, "Host rejected app host ABI %d",
                 APP_HOST_ABI_VERSION);
        dlclose(project->host_library);
        project->host_library = NULL;
        project->destroy_host = NULL;
        return 0;
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
    project->host_mtime = GetFileModTime(project->host_path);
    project->source_mtime = editor_project_source_mtime(project);
    project->reload_failed = 0;
    snprintf(status, status_size, "Loaded editor host for %s", project->name);
    return 1;
#endif
}

static int
editor_reload_project_host(EditorProject *project, char *status,
                           size_t status_size)
{
    char selected_file[EDITOR_PATH_CAP];
    int selected_run_target;

    if(project == NULL || !project->loaded)
        return 0;
    snprintf(selected_file, sizeof(selected_file), "%s", project->selected_file);
    selected_run_target = project->selected_run_target;
    editor_unload_project_host(project);
    if(!editor_build_project_host(project, status, status_size)) {
        project->reload_failed = 1;
        return 0;
    }
    snprintf(project->selected_file, sizeof(project->selected_file), "%s",
             selected_file);
    project->selected_run_target = selected_run_target;
    return editor_load_project_host(project, status, status_size);
}

static void
editor_maybe_reload_project_host(EditorProject *project, char *status,
                                 size_t status_size)
{
    long source_mtime;
    double now;

    if(project == NULL || !project->loaded || project->host == NULL)
        return;
    now = GetTime();
    if(now - project->last_reload_check < 2.0)
        return;
    project->last_reload_check = now;
    source_mtime = editor_project_source_mtime(project);
    if(source_mtime <= 0 || source_mtime <= project->source_mtime)
        return;
    project->source_mtime = source_mtime;
    if(project->reload_failed && source_mtime <= project->host_mtime)
        return;
    snprintf(status, status_size, "Project changed; rebuilding editor host");
    editor_reload_project_host(project, status, status_size);
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
    if(!editor_load_project_host(project, status, status_size))
        return;
}

static int
editor_run_project(EditorProject *project, char *status, size_t status_size)
{
#if defined(_WIN32)
    (void)project;
    snprintf(status, status_size, "Run is not implemented on Windows yet");
    return 0;
#else
    char quoted_path[EDITOR_PATH_CAP + 16];
    char command[EDITOR_PATH_CAP * 2 + 128];
    pid_t pid;
    const EditorRunTarget *target;

    if(project == NULL || !project->loaded) {
        snprintf(status, status_size, "Open a project first");
        return 0;
    }
    if(project->selected_run_target < 0 ||
       project->selected_run_target >= project->run_target_count) {
        snprintf(status, status_size, "No run target selected");
        return 0;
    }
    target = &project->run_targets[project->selected_run_target];

    shell_quote(quoted_path, sizeof(quoted_path), project->path);
    snprintf(command, sizeof(command), "cd %s && %s", quoted_path,
             target->command);
    pid = fork();
    if(pid < 0) {
        snprintf(status, status_size, "Could not launch project");
        return 0;
    }
    if(pid == 0) {
        execl("/bin/sh", "sh", "-lc", command, (char *)NULL);
        _exit(127);
    }
    snprintf(status, status_size, "Running %s (%s)", project->name,
             target->name);
    return 1;
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
draw_project_menu(int x, int y, int *open, FileDialog *project_dialog)
{
    int w = ScaleUIPx(188);
    int item_h = ScaleUIPx(32);

    if(open == NULL || !*open)
        return;
    DrawRectangle(x, y, w, item_h * 3 + ScaleUIPx(10), GetThemeSurface());
    DrawRectangleLines(x, y, w, item_h * 3 + ScaleUIPx(10),
                       DarkenUIColor(GetThemeSurface(), 38));
    y += ScaleUIPx(5);
    if(DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                           "Open Project", UI_BUTTON_STYLE_PRIMARY, 0, NULL)) {
        BeginSelectFileDialogFolder(project_dialog, "Open Project");
        *open = 0;
    }
    y += item_h;
    DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                        "New Project", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
    y += item_h;
    DrawUIGenericButton(x + ScaleUIPx(6), y, w - ScaleUIPx(12), item_h,
                        "Save", UI_BUTTON_STYLE_SECONDARY, 0, NULL);
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
        snprintf(project->selected_file, sizeof(project->selected_file),
                 "%s", activated_path);
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
    snprintf(project->selected_file, sizeof(project->selected_file), "%s",
             rel_path);
    editor_select_source_path(project, rel_path);
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
    snprintf(status, status_size, "Saved %s", project->source_scroll_file);
    return 1;
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
        editor_reload_project_host(project, status, status_size);
    }
    return 1;
}

static void
editor_maybe_autosave_source(EditorProject *project, char *status,
                             size_t status_size)
{
    if(project == NULL || !project->source_dirty)
        return;
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

static void
draw_source_code(Rectangle content, EditorProject *project,
                 char *status, size_t status_size)
{
    char path[EDITOR_PATH_CAP];
    char *before_edit = NULL;
    int pad = ScaleUIPx(16);
    int toolbar_h = ScaleUIPx(30);
    int gap = ScaleUIPx(8);
    char zoom_text[16];
    const char *dirty_label = project != NULL && project->source_dirty
                                  ? "Modified"
                                  : "Saved";
    Rectangle bounds = {
        content.x + (float)pad,
        content.y + (float)pad + (float)toolbar_h + (float)gap,
        content.width - (float)(pad * 2),
        content.height - (float)(pad * 2 + toolbar_h + gap)
    };

    DrawRectangleRec(content, GetThemeBackground());
    if(project == NULL || project->selected_file[0] == '\0') {
        draw_source_placeholder(content, project, 0);
        return;
    }
    snprintf(path, sizeof(path), "%s/%s", project->path,
             project->selected_file);
    if(strcmp(project->source_scroll_file, project->selected_file) != 0) {
        if(project->source_dirty)
            editor_save_source_file(project, status, status_size);
        snprintf(project->source_scroll_file, sizeof(project->source_scroll_file),
                 "%s", project->selected_file);
        project->source_scroll_y = 0;
        project->source_cursor = 0;
        project->source_focused = 0;
        project->source_dirty = 0;
        project->source_loaded = 0;
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
    if(DrawUIGenericButton((int)content.x + pad, (int)content.y + pad,
                           ScaleUIPx(30), toolbar_h, "-",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL))
        editor_adjust_source_zoom(project, -4, status, status_size);
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)((int)content.x + pad + ScaleUIPx(36)),
                   (float)((int)content.y + pad),
                   (float)ScaleUIPx(58), (float)toolbar_h},
        .text = zoom_text,
        .style = editor_input_style()
    });
    if(DrawUIGenericButton((int)content.x + pad + ScaleUIPx(100),
                           (int)content.y + pad,
                           ScaleUIPx(30), toolbar_h, "+",
                           UI_BUTTON_STYLE_SECONDARY, 0, NULL))
        editor_adjust_source_zoom(project, 4, status, status_size);
    DrawUIText(dirty_label,
               (int)(content.x + content.width) - pad -
                   MeasureUIText(dirty_label, UI_TEXT_12),
               (int)content.y + pad + ScaleUIPx(8),
               UI_TEXT_12,
               project->source_dirty ? (Color){150, 74, 0, 255}
                                     : DarkenUIColor(GetThemeText(), 45));
    if(bounds.height < ScaleUIPx(80))
        bounds.height = ScaleUIPx(80);
    before_edit = editor_strdup(project->source);
    if(project->source_focused && editor_mod_key_down()) {
        if(IsKeyPressed(KEY_S)) {
            editor_save_source_now(project, status, status_size);
        } else if(IsKeyPressed(KEY_Z)) {
            if(IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT))
                editor_redo_source(project, status, status_size);
            else
                editor_undo_source(project, status, status_size);
        } else if(IsKeyPressed(KEY_Y)) {
            editor_redo_source(project, status, status_size);
        }
    }
    if(DrawUITextArea((UITextArea){
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
    })) {
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
draw_preview_pane(Rectangle content, EditorProject *project, char *status,
                  size_t status_size)
{
    int x = (int)content.x + ScaleUIPx(28);
    int y = (int)content.y + ScaleUIPx(28);

    DrawRectangleRec(content, GetThemeBackground());
    if(project == NULL || project->host == NULL) {
        DrawUIText("Editor host required", x, y, UI_TEXT_24, GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Kryon renders the real app here after the project exposes its editor host.",
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

    SetUIInspectCanvasBounds(content);
    BeginScissorMode((int)content.x, (int)content.y,
                     (int)content.width, (int)content.height);
    PushUIInputClip(content);
    DrawAppScreen((AppHost *)project->host, content);
    PopUIInputClip();
    EndScissorMode();
    draw_preview_context_menu(project, content, status, status_size);
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
draw_canvas(Rectangle frame, Rectangle content, EditorProject *project,
            char *status, size_t status_size)
{
    int header_h = editor_canvas_header_height();
    int title_x = (int)frame.x + ScaleUIPx(14);
    int x = (int)content.x + ScaleUIPx(34);
    int y = (int)content.y + ScaleUIPx(34);
    int divider = ScaleUIPx(6);
    AppScreenInfo active_screen = {0};
    Rectangle preview = content;
    Rectangle source = content;
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
        DrawUIText("Open a project", x, y, UI_TEXT_24, GetThemeText());
        y += ScaleUIPx(40);
        DrawUIText("Use Project / Open Project to choose an app folder.",
                   x, y, UI_TEXT_16, GetThemeIcon());
        return;
    }

    source.width = (content.width - divider) * 0.5f;
    preview.x = source.x + source.width + divider;
    preview.width = content.width - source.width - divider;
    DrawRectangleRec((Rectangle){source.x + source.width, source.y,
                                 (float)divider, source.height},
                     DarkenUIColor(GetThemeBackground(), 32));

    draw_preview_pane(preview, project, status, status_size);
    editor_restore_ide_ui_frame(GetScreenWidth(), GetScreenHeight());
    draw_source_pane(source, project, status, status_size);
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
    int path_x = ScaleUIPx(90);
    int path_w = view_w - ScaleUIPx(660);
    int run_y = ScaleUIPx(7);
    int run_h = ScaleUIPx(28);
    int redo_x = view_w - ScaleUIPx(70);
    int undo_x = view_w - ScaleUIPx(132);
    int run_x = view_w - ScaleUIPx(186);
    int target_x = view_w - ScaleUIPx(332);
    int target_w = ScaleUIPx(138);
    const char *target_labels[EDITOR_MAX_RUN_TARGETS];
    int target_count = 0;
    int old_target = project != NULL ? project->selected_run_target : 0;

    if(path_w < ScaleUIPx(150))
        path_w = ScaleUIPx(150);

    DrawRectangle(0, 0, view_w, top_h, DarkenUIColor(GetThemeBackground(), 12));
    DrawLine(0, top_h - 1, view_w, top_h - 1,
             DarkenUIColor(GetThemeBackground(), 42));
    if(draw_menu_button(menu_x, ScaleUIPx(9), "Project", *project_menu_open)) {
        *project_menu_open = !*project_menu_open;
    }
    DrawUIReadonlyTextBox((UIReadonlyTextBox){
        .bounds = {(float)path_x, (float)ScaleUIPx(6),
                   (float)path_w, (float)ScaleUIPx(30)},
        .text = project != NULL && project->loaded ? project->path : "",
        .style = editor_input_style()
    });
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
    SetUIDropdownClipTop(0);
    SetUIDropdownClipBottom(0);
    draw_project_menu(menu_x, top_h - ScaleUIPx(2),
                      project_menu_open, project_dialog);
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

    draw_top_bar(view_w, project_menu_open, project_dialog, project, play_icon,
                 status_text, status_size);
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

    DrawRectangle(0, view_h - bottom_h, view_w, bottom_h,
                  DarkenUIColor(GetThemeBackground(), 14));
    DrawLine(0, view_h - bottom_h, view_w, view_h - bottom_h,
             DarkenUIColor(GetThemeBackground(), 42));
    DrawUIText(status_text, ScaleUIPx(12), view_h - bottom_h + ScaleUIPx(7),
               UI_TEXT_12, GetThemeText());
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

int
main(int argc, char **argv)
{
    const int screen_w = 1400;
    const int screen_h = 900;
    int project_menu_open = 0;
    FileDialog project_dialog;
    EditorProject project = {0};
    EditorRecentProjects recent;
    EditorSidebarState sidebar = {0};
    Texture2D play_icon = {0};
    char status_text[160] = "Ready";
    int smoke_screens = 0;
    const char *project_arg = NULL;

    if(argc > 1 && strcmp(argv[1], "--smoke-screens") == 0) {
        smoke_screens = 1;
        if(argc > 2)
            project_arg = argv[2];
    } else if(argc > 1) {
        project_arg = argv[1];
    }

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(screen_w, screen_h, "Kryon IDE");
    SetTargetFPS(60);
    load_editor_font();
    play_icon = LoadUIIconTexture(UI_ICON_TYPE_PLAY);
    InitUI(screen_w, screen_h, editor_ui_scale());
    SetCurrentTheme(THEME_MONO, 0);
    SetUIInspectEnabled(1);
    InitFileDialog(&project_dialog);
    editor_load_recent_projects(&recent);

    if(project_arg != NULL && project_arg[0] != '\0') {
        editor_open_project(&project, project_arg, &recent, status_text,
                            sizeof(status_text));
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
        editor_maybe_autosave_source(&project, status_text,
                                     sizeof(status_text));
        editor_maybe_reload_project_host(&project, status_text,
                                         sizeof(status_text));

        SetUIInspectCanvasBounds(canvas_content);
        draw_canvas(canvas_frame, canvas_content, &project, status_text,
                    sizeof(status_text));
        if(editor_selected_file_has_preview(&project)) {
            DrawUIInspectOverlay();
            if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                UIInspectSelection selection = UIInspectGetSelection();

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

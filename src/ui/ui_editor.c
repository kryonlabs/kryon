#include "ui_internal.h"
#include "ui_editor.h"
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>

#define UI_EDITOR_MAX_WIDGETS 512
#define UI_EDITOR_MAX_OVERRIDES 512
#define UI_EDITOR_ID_MAX 96
#define UI_EDITOR_KIND_MAX 32
#define UI_EDITOR_PATH_MAX 512

typedef struct UIEditorWidget {
    char id[UI_EDITOR_ID_MAX];
    char kind[UI_EDITOR_KIND_MAX];
    Rectangle bounds;
    int flags;
    int order;
} UIEditorWidget;

typedef struct UIEditorOverride {
    char id[UI_EDITOR_ID_MAX];
    Rectangle bounds;
    int enabled;
} UIEditorOverride;

typedef struct UIEditorState {
    int initialized;
    int enabled;
    int visible;
    int loaded;
    int dirty;
    int dragging;
    int resizing;
    int selected;
    Vector2 drag_start;
    Rectangle edit_start;
    char project_root[UI_EDITOR_PATH_MAX];
    UIEditorWidget widgets[UI_EDITOR_MAX_WIDGETS];
    int widget_count;
    UIEditorOverride overrides[UI_EDITOR_MAX_OVERRIDES];
    int override_count;
} UIEditorState;

static UIEditorState g_ui_editor;

static void
ui_editor_strncpy(char *dst, size_t dst_size, const char *src)
{
    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

static int
ui_editor_streq(const char *a, const char *b)
{
    if(a == NULL || b == NULL)
        return 0;
    return strcmp(a, b) == 0;
}

static int
ui_editor_persistent_id(const char *id)
{
    return id != NULL && id[0] != '\0' && strncmp(id, "tmp:", 4) != 0;
}

static void
ui_editor_default_project_root(void)
{
    const char *root;

    if(g_ui_editor.project_root[0] != '\0')
        return;
    root = getenv("FLINT_PROJECT_ROOT");
    if(root != NULL && root[0] != '\0') {
        ui_editor_strncpy(g_ui_editor.project_root,
                          sizeof(g_ui_editor.project_root), root);
        return;
    }
    ui_editor_strncpy(g_ui_editor.project_root,
                      sizeof(g_ui_editor.project_root), ".");
}

static void
ui_editor_init_from_env(void)
{
    const char *env;

    if(g_ui_editor.initialized)
        return;
    g_ui_editor.initialized = 1;
    env = getenv("FLINT_EDITOR");
    g_ui_editor.enabled = env != NULL && env[0] != '\0' && env[0] != '0';
    g_ui_editor.visible = g_ui_editor.enabled;
    ui_editor_default_project_root();
}

static void
ui_editor_layout_path(char *path, size_t path_size)
{
    ui_editor_default_project_root();
    snprintf(path, path_size, "%s/.flint/editor_layout.ini",
             g_ui_editor.project_root);
}

static UIEditorOverride *
ui_editor_find_override(const char *id)
{
    for(int i = 0; i < g_ui_editor.override_count; i++) {
        if(ui_editor_streq(g_ui_editor.overrides[i].id, id))
            return &g_ui_editor.overrides[i];
    }
    return NULL;
}

static UIEditorOverride *
ui_editor_get_override(const char *id)
{
    UIEditorOverride *override;

    override = ui_editor_find_override(id);
    if(override != NULL)
        return override;
    if(g_ui_editor.override_count >= UI_EDITOR_MAX_OVERRIDES)
        return NULL;
    override = &g_ui_editor.overrides[g_ui_editor.override_count++];
    memset(override, 0, sizeof(*override));
    ui_editor_strncpy(override->id, sizeof(override->id), id);
    override->enabled = 1;
    return override;
}

static char *
ui_editor_trim(char *text)
{
    char *end;

    if(text == NULL)
        return NULL;
    while(isspace((unsigned char)*text))
        text++;
    end = text + strlen(text);
    while(end > text && isspace((unsigned char)end[-1]))
        *--end = '\0';
    return text;
}

static void
ui_editor_load_layout(void)
{
    char path[UI_EDITOR_PATH_MAX];
    char line[256];
    FILE *file;
    UIEditorOverride *current = NULL;

    if(g_ui_editor.loaded)
        return;
    g_ui_editor.loaded = 1;
    ui_editor_layout_path(path, sizeof(path));
    file = fopen(path, "rb");
    if(file == NULL)
        return;
    while(fgets(line, sizeof(line), file) != NULL) {
        char *text = ui_editor_trim(line);
        char *eq;

        if(text == NULL || text[0] == '\0' || text[0] == '#')
            continue;
        if(text[0] == '[') {
            char *end = strchr(text, ']');
            if(end == NULL)
                continue;
            *end = '\0';
            current = ui_editor_get_override(text + 1);
            continue;
        }
        if(current == NULL)
            continue;
        eq = strchr(text, '=');
        if(eq == NULL)
            continue;
        *eq++ = '\0';
        text = ui_editor_trim(text);
        eq = ui_editor_trim(eq);
        if(ui_editor_streq(text, "x"))
            current->bounds.x = (float)atof(eq);
        else if(ui_editor_streq(text, "y"))
            current->bounds.y = (float)atof(eq);
        else if(ui_editor_streq(text, "w"))
            current->bounds.width = (float)atof(eq);
        else if(ui_editor_streq(text, "h"))
            current->bounds.height = (float)atof(eq);
        else if(ui_editor_streq(text, "enabled"))
            current->enabled = atoi(eq) != 0;
    }
    fclose(file);
}

static void
ui_editor_save_layout(void)
{
    char dir[UI_EDITOR_PATH_MAX];
    char path[UI_EDITOR_PATH_MAX];
    FILE *file;

    ui_editor_default_project_root();
    snprintf(dir, sizeof(dir), "%s/.flint", g_ui_editor.project_root);
    if(mkdir(dir, 0775) != 0 && errno != EEXIST)
        return;
    ui_editor_layout_path(path, sizeof(path));
    file = fopen(path, "wb");
    if(file == NULL)
        return;
    fprintf(file, "# Flint editor layout\n\n");
    for(int i = 0; i < g_ui_editor.override_count; i++) {
        UIEditorOverride *override = &g_ui_editor.overrides[i];
        if(!ui_editor_persistent_id(override->id))
            continue;
        fprintf(file, "[%s]\n", override->id);
        fprintf(file, "enabled=%d\n", override->enabled ? 1 : 0);
        fprintf(file, "x=%.0f\n", override->bounds.x);
        fprintf(file, "y=%.0f\n", override->bounds.y);
        fprintf(file, "w=%.0f\n", override->bounds.width);
        fprintf(file, "h=%.0f\n\n", override->bounds.height);
    }
    fclose(file);
    g_ui_editor.dirty = 0;
}

void
BeginUIEditorFrame(const char *project_root)
{
    ui_editor_init_from_env();
    if(project_root != NULL && project_root[0] != '\0' &&
       !ui_editor_streq(project_root, g_ui_editor.project_root)) {
        ui_editor_strncpy(g_ui_editor.project_root,
                          sizeof(g_ui_editor.project_root), project_root);
        g_ui_editor.loaded = 0;
        g_ui_editor.override_count = 0;
    }
    if(!g_ui_editor.enabled)
        return;
    ui_editor_load_layout();
    g_ui_editor.widget_count = 0;
    if(IsKeyPressed(KEY_F12))
        g_ui_editor.visible = !g_ui_editor.visible;
}

void
EndUIEditorFrame(void)
{
}

void
SetUIEditorEnabled(int enabled)
{
    ui_editor_init_from_env();
    g_ui_editor.enabled = enabled ? 1 : 0;
    g_ui_editor.visible = g_ui_editor.enabled;
}

int
UIEditorEnabled(void)
{
    ui_editor_init_from_env();
    return g_ui_editor.enabled;
}

int
UIEditorInputCapturesClick(Vector2 point)
{
    (void)point;
    ui_editor_init_from_env();
    return g_ui_editor.enabled && g_ui_editor.visible;
}

void
UIEditorApplyBounds(const char *id, Rectangle *bounds)
{
    UIEditorOverride *override;

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled || id == NULL || bounds == NULL)
        return;
    ui_editor_load_layout();
    override = ui_editor_find_override(id);
    if(override == NULL || !override->enabled)
        return;
    if(override->bounds.width > 0.0f && override->bounds.height > 0.0f)
        *bounds = override->bounds;
}

void
UIEditorRegisterWidget(const char *id, const char *kind,
                       Rectangle *bounds, int flags)
{
    UIEditorWidget *widget;
    UIEditorOverride *override;

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled || id == NULL || id[0] == '\0' ||
       bounds == NULL || g_ui_editor.widget_count >= UI_EDITOR_MAX_WIDGETS)
        return;
    widget = &g_ui_editor.widgets[g_ui_editor.widget_count++];
    memset(widget, 0, sizeof(*widget));
    ui_editor_strncpy(widget->id, sizeof(widget->id), id);
    ui_editor_strncpy(widget->kind, sizeof(widget->kind),
                      kind != NULL ? kind : "widget");
    widget->bounds = *bounds;
    widget->flags = flags;
    if(!ui_editor_persistent_id(id))
        widget->flags |= UI_EDITOR_WIDGET_TEMPORARY_ID;
    widget->order = g_ui_editor.widget_count;

    override = ui_editor_find_override(id);
    if(override == NULL && ui_editor_persistent_id(id) &&
       (flags & (UI_EDITOR_WIDGET_MOVABLE | UI_EDITOR_WIDGET_RESIZABLE)) != 0) {
        override = ui_editor_get_override(id);
        if(override != NULL)
            override->bounds = *bounds;
    }
}

static int
ui_editor_point_in_resize_handle(Rectangle bounds, Vector2 point)
{
    int handle = ScaleUIPx(12);
    Rectangle rect = {
        bounds.x + bounds.width - (float)handle,
        bounds.y + bounds.height - (float)handle,
        (float)handle,
        (float)handle
    };
    return CheckCollisionPointRec(point, rect);
}

static void
ui_editor_select_at(Vector2 mouse)
{
    g_ui_editor.selected = -1;
    for(int i = g_ui_editor.widget_count - 1; i >= 0; i--) {
        if(CheckCollisionPointRec(mouse, g_ui_editor.widgets[i].bounds)) {
            g_ui_editor.selected = i;
            return;
        }
    }
}

static void
ui_editor_commit_selected(Rectangle bounds)
{
    UIEditorWidget *widget;
    UIEditorOverride *override;

    if(g_ui_editor.selected < 0 ||
       g_ui_editor.selected >= g_ui_editor.widget_count)
        return;
    widget = &g_ui_editor.widgets[g_ui_editor.selected];
    if(!ui_editor_persistent_id(widget->id))
        return;
    override = ui_editor_get_override(widget->id);
    if(override == NULL)
        return;
    if(bounds.width < 4.0f)
        bounds.width = 4.0f;
    if(bounds.height < 4.0f)
        bounds.height = 4.0f;
    override->bounds = bounds;
    override->enabled = 1;
    widget->bounds = bounds;
    g_ui_editor.dirty = 1;
}

static void
ui_editor_update_interaction(void)
{
    Vector2 mouse = ui_mouse_world();
    UIEditorWidget *selected;
    Rectangle bounds;
    int can_move;
    int can_resize;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui_editor_select_at(mouse);
        if(g_ui_editor.selected >= 0) {
            selected = &g_ui_editor.widgets[g_ui_editor.selected];
            can_move = (selected->flags & UI_EDITOR_WIDGET_MOVABLE) != 0;
            can_resize = (selected->flags & UI_EDITOR_WIDGET_RESIZABLE) != 0;
            g_ui_editor.drag_start = mouse;
            g_ui_editor.edit_start = selected->bounds;
            g_ui_editor.resizing = can_resize &&
                                   ui_editor_point_in_resize_handle(selected->bounds, mouse);
            g_ui_editor.dragging = can_move && !g_ui_editor.resizing;
        }
    }
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (g_ui_editor.dragging || g_ui_editor.resizing) &&
       g_ui_editor.selected >= 0) {
        float dx = mouse.x - g_ui_editor.drag_start.x;
        float dy = mouse.y - g_ui_editor.drag_start.y;
        bounds = g_ui_editor.edit_start;
        if(g_ui_editor.dragging) {
            bounds.x += dx;
            bounds.y += dy;
        } else if(g_ui_editor.resizing) {
            bounds.width += dx;
            bounds.height += dy;
        }
        ui_editor_commit_selected(bounds);
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_editor.dragging = 0;
        g_ui_editor.resizing = 0;
        UIConsumeRelease();
    }
    if(g_ui_editor.selected >= 0 &&
       g_ui_editor.selected < g_ui_editor.widget_count) {
        selected = &g_ui_editor.widgets[g_ui_editor.selected];
        bounds = selected->bounds;
        if(IsKeyPressed(KEY_LEFT)) {
            bounds.x -= IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.0f : 1.0f;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_RIGHT)) {
            bounds.x += IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.0f : 1.0f;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_UP)) {
            bounds.y -= IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.0f : 1.0f;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_DOWN)) {
            bounds.y += IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT) ? 8.0f : 1.0f;
            ui_editor_commit_selected(bounds);
        }
    }
    if((IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) &&
       IsKeyPressed(KEY_S))
        ui_editor_save_layout();
}

static void
ui_editor_draw_inspector(void)
{
    Rectangle panel = {8, 8, 300, 124};
    Color panel_bg = c_surface;
    Color border = LightenUIColor(c_surface, 35);
    int y = (int)panel.y + ScaleUIPx(10);
    char line[160];

    panel_bg.a = panel_bg.a > 220 ? panel_bg.a : 220;
    DrawRectangleRec(panel, panel_bg);
    DrawRectangleLinesEx(panel, 1, border);
    DrawUIText("Flint Editor", (int)panel.x + ScaleUIPx(10), y,
               GetUIFontSize(), c_text);
    y += ScaleUIPx(24);
    snprintf(line, sizeof(line), "Widgets: %d%s", g_ui_editor.widget_count,
             g_ui_editor.dirty ? "  unsaved" : "");
    DrawUIText(line, (int)panel.x + ScaleUIPx(10), y,
               GetUISmallFontSize(), c_text);
    y += ScaleUIPx(20);
    if(g_ui_editor.selected >= 0 &&
       g_ui_editor.selected < g_ui_editor.widget_count) {
        UIEditorWidget *widget = &g_ui_editor.widgets[g_ui_editor.selected];
        snprintf(line, sizeof(line), "%s  %s", widget->kind, widget->id);
        DrawUIText(line, (int)panel.x + ScaleUIPx(10), y,
                   GetUISmallFontSize(), c_text);
        y += ScaleUIPx(20);
        snprintf(line, sizeof(line), "x %.0f  y %.0f  w %.0f  h %.0f",
                 widget->bounds.x, widget->bounds.y,
                 widget->bounds.width, widget->bounds.height);
        DrawUIText(line, (int)panel.x + ScaleUIPx(10), y,
                   GetUISmallFontSize(), c_text);
    } else {
        DrawUIText("Select a widget", (int)panel.x + ScaleUIPx(10), y,
                   GetUISmallFontSize(), c_text);
    }
}

void
DrawUIEditorOverlay(void)
{
    Color outline = {40, 180, 255, 170};
    Color selected = {255, 204, 64, 230};
    Color handle = {255, 204, 64, 255};

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled)
        return;
    if(!g_ui_editor.visible)
        return;

    ui_editor_update_interaction();
    for(int i = 0; i < g_ui_editor.widget_count; i++) {
        UIEditorWidget *widget = &g_ui_editor.widgets[i];
        Color color = i == g_ui_editor.selected ? selected : outline;
        DrawRectangleLinesEx(widget->bounds, i == g_ui_editor.selected ? 2 : 1,
                             color);
        if(i == g_ui_editor.selected &&
           (widget->flags & UI_EDITOR_WIDGET_RESIZABLE) != 0) {
            int s = ScaleUIPx(10);
            DrawRectangle((int)(widget->bounds.x + widget->bounds.width - s),
                          (int)(widget->bounds.y + widget->bounds.height - s),
                          s, s, handle);
        }
    }
    ui_editor_draw_inspector();
}

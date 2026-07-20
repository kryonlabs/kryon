#include "ui_internal.h"
#include "ui_editor.h"

#define UI_EDITOR_MAX_WIDGETS 512
#define UI_EDITOR_MAX_OVERRIDES 512
#define UI_EDITOR_ID_MAX 96
#define UI_EDITOR_KIND_MAX 32
#define UI_EDITOR_ACTION_MAX 64
#define UI_EDITOR_PATH_MAX 512

typedef struct UIEditorWidget {
    char id[UI_EDITOR_ID_MAX];
    char kind[UI_EDITOR_KIND_MAX];
    char action[UI_EDITOR_ACTION_MAX];
    Rectangle bounds;
    Rectangle screen_bounds;
    float screen_zoom;
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
    int dirty;
    int dragging;
    int resizing;
    int selected;
    int chrome_depth;
    int canvas_active;
    Rectangle canvas_bounds;
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

void
BeginUIEditorFrame(const char *project_root)
{
    ui_editor_init_from_env();
    if(project_root != NULL && project_root[0] != '\0' &&
       !ui_editor_streq(project_root, g_ui_editor.project_root)) {
        ui_editor_strncpy(g_ui_editor.project_root,
                          sizeof(g_ui_editor.project_root), project_root);
        g_ui_editor.override_count = 0;
        g_ui_editor.dirty = 0;
    }
    if(!g_ui_editor.enabled)
        return;
    g_ui_editor.widget_count = 0;
    g_ui_editor.canvas_active = 0;
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
UIEditorWidgetCount(void)
{
    ui_editor_init_from_env();
    return g_ui_editor.widget_count;
}

UIEditorSelection
UIEditorGetSelection(void)
{
    UIEditorSelection selection = {0};
    UIEditorWidget *widget;

    ui_editor_init_from_env();
    if(g_ui_editor.selected < 0 ||
       g_ui_editor.selected >= g_ui_editor.widget_count)
        return selection;
    widget = &g_ui_editor.widgets[g_ui_editor.selected];
    ui_editor_strncpy(selection.id, sizeof(selection.id), widget->id);
    ui_editor_strncpy(selection.kind, sizeof(selection.kind), widget->kind);
    ui_editor_strncpy(selection.action, sizeof(selection.action),
                      widget->action);
    selection.bounds = widget->bounds;
    selection.flags = widget->flags;
    selection.valid = 1;
    return selection;
}

void
SetUIEditorCanvasBounds(Rectangle bounds)
{
    ui_editor_init_from_env();
    g_ui_editor.canvas_bounds = bounds;
    g_ui_editor.canvas_active = bounds.width > 0.0f && bounds.height > 0.0f;
}

static Vector2
ui_editor_mouse_screen(void)
{
    return GetMousePosition();
}

static Rectangle
ui_editor_rect_to_screen(Rectangle bounds)
{
    return (Rectangle){
        g_ui_camera.offset.x + bounds.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + bounds.y * g_ui_camera.zoom,
        bounds.width * g_ui_camera.zoom,
        bounds.height * g_ui_camera.zoom
    };
}

int
PushUIEditorChrome(int enabled)
{
    int token;

    ui_editor_init_from_env();
    token = g_ui_editor.chrome_depth;
    if(enabled)
        g_ui_editor.chrome_depth++;
    return token;
}

void
PopUIEditorChrome(int token)
{
    ui_editor_init_from_env();
    if(token < 0)
        token = 0;
    g_ui_editor.chrome_depth = token;
}

int
UIEditorInputCapturesClick(Vector2 point)
{
    ui_editor_init_from_env();
    (void)point;
    if(g_ui_editor.chrome_depth > 0)
        return 0;
    if(g_ui_editor.dragging || g_ui_editor.resizing)
        return 1;
    if(g_ui_editor.canvas_active &&
       !CheckCollisionPointRec(ui_editor_mouse_screen(),
                               g_ui_editor.canvas_bounds))
        return 0;
    return g_ui_editor.enabled && g_ui_editor.visible;
}

void
UIEditorApplyBounds(const char *id, Rectangle *bounds)
{
    UIEditorOverride *override;

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled || id == NULL || bounds == NULL)
        return;
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
    Rectangle screen_bounds;

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled || id == NULL || id[0] == '\0' ||
       bounds == NULL || g_ui_editor.widget_count >= UI_EDITOR_MAX_WIDGETS)
        return;
    if(g_ui_editor.chrome_depth > 0)
        return;
    screen_bounds = ui_editor_rect_to_screen(*bounds);
    if(g_ui_editor.canvas_active &&
       !CheckCollisionRecs(screen_bounds, g_ui_editor.canvas_bounds))
        return;
    widget = &g_ui_editor.widgets[g_ui_editor.widget_count++];
    memset(widget, 0, sizeof(*widget));
    ui_editor_strncpy(widget->id, sizeof(widget->id), id);
    ui_editor_strncpy(widget->kind, sizeof(widget->kind),
                      kind != NULL ? kind : "widget");
    widget->bounds = *bounds;
    widget->screen_bounds = screen_bounds;
    widget->screen_zoom = g_ui_camera.zoom > 0.0f ? g_ui_camera.zoom : 1.0f;
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

void
UIEditorSetWidgetAction(const char *id, const char *action)
{
    UIEditorWidget *widget;

    ui_editor_init_from_env();
    if(!g_ui_editor.enabled || id == NULL || action == NULL)
        return;
    for(int i = g_ui_editor.widget_count - 1; i >= 0; i--) {
        widget = &g_ui_editor.widgets[i];
        if(ui_editor_streq(widget->id, id)) {
            ui_editor_strncpy(widget->action, sizeof(widget->action), action);
            return;
        }
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
        if(CheckCollisionPointRec(mouse, g_ui_editor.widgets[i].screen_bounds)) {
            g_ui_editor.selected = i;
            return;
        }
    }
}

static Rectangle
ui_editor_clamp_bounds(Rectangle bounds)
{
    if(bounds.width < 4.0f)
        bounds.width = 4.0f;
    if(bounds.height < 4.0f)
        bounds.height = 4.0f;
    return bounds;
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
    bounds = ui_editor_clamp_bounds(bounds);
    override->bounds = bounds;
    override->enabled = 1;
    widget->bounds = bounds;
    g_ui_editor.dirty = 1;
}

static void
ui_editor_update_interaction(void)
{
    Vector2 screen_mouse = ui_editor_mouse_screen();
    UIEditorWidget *selected;
    Rectangle bounds;
    Rectangle screen_bounds;
    int can_move;
    int can_resize;

    if(g_ui_editor.canvas_active &&
       !CheckCollisionPointRec(screen_mouse, g_ui_editor.canvas_bounds) &&
       !g_ui_editor.dragging && !g_ui_editor.resizing)
        return;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui_editor_select_at(screen_mouse);
        if(g_ui_editor.selected >= 0) {
            selected = &g_ui_editor.widgets[g_ui_editor.selected];
            can_move = (selected->flags & UI_EDITOR_WIDGET_MOVABLE) != 0;
            can_resize = (selected->flags & UI_EDITOR_WIDGET_RESIZABLE) != 0;
            g_ui_editor.drag_start = screen_mouse;
            g_ui_editor.edit_start = selected->bounds;
            g_ui_editor.resizing = can_resize &&
                                   ui_editor_point_in_resize_handle(
                                       selected->screen_bounds, screen_mouse);
            g_ui_editor.dragging = can_move && !g_ui_editor.resizing;
        }
    }
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (g_ui_editor.dragging || g_ui_editor.resizing) &&
       g_ui_editor.selected >= 0) {
        float dx;
        float dy;
        float zoom;

        selected = &g_ui_editor.widgets[g_ui_editor.selected];
        dx = screen_mouse.x - g_ui_editor.drag_start.x;
        dy = screen_mouse.y - g_ui_editor.drag_start.y;
        zoom = selected->screen_zoom > 0.0f ? selected->screen_zoom : 1.0f;
        bounds = g_ui_editor.edit_start;
        screen_bounds = selected->screen_bounds;
        if(g_ui_editor.dragging) {
            bounds.x += dx / zoom;
            bounds.y += dy / zoom;
            screen_bounds.x += dx;
            screen_bounds.y += dy;
        } else if(g_ui_editor.resizing) {
            bounds.width += dx / zoom;
            bounds.height += dy / zoom;
            screen_bounds.width += dx;
            screen_bounds.height += dy;
        }
        bounds = ui_editor_clamp_bounds(bounds);
        ui_editor_commit_selected(bounds);
        selected->screen_bounds = screen_bounds;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_editor.dragging = 0;
        g_ui_editor.resizing = 0;
        UIConsumeRelease();
    }
    if(g_ui_editor.selected >= 0 &&
       g_ui_editor.selected < g_ui_editor.widget_count) {
        float step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)
                         ? 8.0f
                         : 1.0f;

        selected = &g_ui_editor.widgets[g_ui_editor.selected];
        bounds = selected->bounds;
        if(IsKeyPressed(KEY_LEFT)) {
            bounds.x -= step;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_RIGHT)) {
            bounds.x += step;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_UP)) {
            bounds.y -= step;
            ui_editor_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_DOWN)) {
            bounds.y += step;
            ui_editor_commit_selected(bounds);
        }
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
        Rectangle screen_bounds = widget->screen_bounds;
        Color color = i == g_ui_editor.selected ? selected : outline;

        DrawRectangleLinesEx(screen_bounds, i == g_ui_editor.selected ? 2 : 1,
                             color);
        if(i == g_ui_editor.selected &&
           (widget->flags & UI_EDITOR_WIDGET_RESIZABLE) != 0) {
            int s = ScaleUIPx(10);

            DrawRectangle((int)(screen_bounds.x + screen_bounds.width - s),
                          (int)(screen_bounds.y + screen_bounds.height - s),
                          s, s, handle);
        }
    }
}

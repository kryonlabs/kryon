#include "ui_internal.h"
#include "ui_inspect.h"

#define UI_INSPECT_MAX_WIDGETS 512
#define UI_INSPECT_MAX_OVERRIDES 512
#define UI_INSPECT_ID_MAX 96
#define UI_INSPECT_KIND_MAX 32
#define UI_INSPECT_ACTION_MAX 64
#define UI_INSPECT_PATH_MAX 512
#define UI_INSPECT_SOURCE_STACK_MAX 32

typedef struct UIInspectWidget {
    char id[UI_INSPECT_ID_MAX];
    char kind[UI_INSPECT_KIND_MAX];
    char action[UI_INSPECT_ACTION_MAX];
    char source_path[UI_INSPECT_PATH_MAX];
    Rectangle bounds;
    Rectangle screen_bounds;
    float screen_zoom;
    int flags;
    int order;
    int parent;
    int depth;
    int source_line;
} UIInspectWidget;

typedef struct UIInspectOverride {
    char id[UI_INSPECT_ID_MAX];
    Rectangle bounds;
    int enabled;
} UIInspectOverride;

typedef struct UIInspectState {
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
    char project_root[UI_INSPECT_PATH_MAX];
    char source_path_stack[UI_INSPECT_SOURCE_STACK_MAX][UI_INSPECT_PATH_MAX];
    int source_line_stack[UI_INSPECT_SOURCE_STACK_MAX];
    int source_depth;
    int widget_stack[UI_INSPECT_MAX_WIDGETS];
    int widget_depth;
    UIInspectWidget widgets[UI_INSPECT_MAX_WIDGETS];
    int widget_count;
    UIInspectOverride overrides[UI_INSPECT_MAX_OVERRIDES];
    int override_count;
} UIInspectState;

static UIInspectState g_ui_inspect;

static void
ui_inspect_strncpy(char *dst, size_t dst_size, const char *src)
{
    if(dst == NULL || dst_size == 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, dst_size, "%s", src);
}

static int
ui_inspect_streq(const char *a, const char *b)
{
    if(a == NULL || b == NULL)
        return 0;
    return strcmp(a, b) == 0;
}

static int
ui_inspect_persistent_id(const char *id)
{
    return id != NULL && id[0] != '\0' && strncmp(id, "tmp:", 4) != 0;
}

static void
ui_inspect_default_project_root(void)
{
    const char *root;

    if(g_ui_inspect.project_root[0] != '\0')
        return;
    root = getenv("FLINT_PROJECT_ROOT");
    if(root != NULL && root[0] != '\0') {
        ui_inspect_strncpy(g_ui_inspect.project_root,
                          sizeof(g_ui_inspect.project_root), root);
        return;
    }
    ui_inspect_strncpy(g_ui_inspect.project_root,
                      sizeof(g_ui_inspect.project_root), ".");
}

static void
ui_inspect_init_from_env(void)
{
    const char *env;

    if(g_ui_inspect.initialized)
        return;
    g_ui_inspect.initialized = 1;
    env = getenv("FLINT_INSPECT");
    g_ui_inspect.enabled = env != NULL && env[0] != '\0' && env[0] != '0';
    g_ui_inspect.visible = g_ui_inspect.enabled;
    ui_inspect_default_project_root();
}

static UIInspectOverride *
ui_inspect_find_override(const char *id)
{
    for(int i = 0; i < g_ui_inspect.override_count; i++) {
        if(ui_inspect_streq(g_ui_inspect.overrides[i].id, id))
            return &g_ui_inspect.overrides[i];
    }
    return NULL;
}

static UIInspectOverride *
ui_inspect_get_override(const char *id)
{
    UIInspectOverride *override;

    override = ui_inspect_find_override(id);
    if(override != NULL)
        return override;
    if(g_ui_inspect.override_count >= UI_INSPECT_MAX_OVERRIDES)
        return NULL;
    override = &g_ui_inspect.overrides[g_ui_inspect.override_count++];
    memset(override, 0, sizeof(*override));
    ui_inspect_strncpy(override->id, sizeof(override->id), id);
    override->enabled = 1;
    return override;
}

void
BeginUIInspectFrame(const char *project_root)
{
    ui_inspect_init_from_env();
    if(project_root != NULL && project_root[0] != '\0' &&
       !ui_inspect_streq(project_root, g_ui_inspect.project_root)) {
        ui_inspect_strncpy(g_ui_inspect.project_root,
                          sizeof(g_ui_inspect.project_root), project_root);
        g_ui_inspect.override_count = 0;
        g_ui_inspect.dirty = 0;
    }
    if(!g_ui_inspect.enabled)
        return;
    g_ui_inspect.widget_count = 0;
    g_ui_inspect.canvas_active = 0;
    g_ui_inspect.source_depth = 0;
    g_ui_inspect.widget_depth = 0;
    if(IsKeyPressed(KEY_F12))
        g_ui_inspect.visible = !g_ui_inspect.visible;
}

void
EndUIInspectFrame(void)
{
}

void
SetUIInspectEnabled(int enabled)
{
    ui_inspect_init_from_env();
    g_ui_inspect.enabled = enabled ? 1 : 0;
    g_ui_inspect.visible = g_ui_inspect.enabled;
}

int
UIInspectEnabled(void)
{
    ui_inspect_init_from_env();
    return g_ui_inspect.enabled;
}

int
UIInspectWidgetCount(void)
{
    ui_inspect_init_from_env();
    return g_ui_inspect.widget_count;
}

UIInspectSelection
UIInspectGetSelection(void)
{
    UIInspectSelection selection = {0};
    UIInspectWidget *widget;

    ui_inspect_init_from_env();
    if(g_ui_inspect.selected < 0 ||
       g_ui_inspect.selected >= g_ui_inspect.widget_count)
        return selection;
    widget = &g_ui_inspect.widgets[g_ui_inspect.selected];
    ui_inspect_strncpy(selection.id, sizeof(selection.id), widget->id);
    ui_inspect_strncpy(selection.kind, sizeof(selection.kind), widget->kind);
    ui_inspect_strncpy(selection.action, sizeof(selection.action),
                      widget->action);
    ui_inspect_strncpy(selection.source_path, sizeof(selection.source_path),
                      widget->source_path);
    selection.bounds = widget->bounds;
    selection.flags = widget->flags;
    selection.source_line = widget->source_line;
    selection.kind_index = 1;
    for(int i = 0; i < g_ui_inspect.selected; i++) {
        if(ui_inspect_streq(g_ui_inspect.widgets[i].kind, widget->kind))
            selection.kind_index++;
    }
    selection.valid = 1;
    return selection;
}

void
SetUIInspectCanvasBounds(Rectangle bounds)
{
    ui_inspect_init_from_env();
    g_ui_inspect.canvas_bounds = bounds;
    g_ui_inspect.canvas_active = bounds.width > 0.0f && bounds.height > 0.0f;
}

static Vector2
ui_inspect_mouse_screen(void)
{
    return GetMousePosition();
}

static Rectangle
ui_inspect_rect_to_screen(Rectangle bounds)
{
    return (Rectangle){
        g_ui_camera.offset.x + bounds.x * g_ui_camera.zoom,
        g_ui_camera.offset.y + bounds.y * g_ui_camera.zoom,
        bounds.width * g_ui_camera.zoom,
        bounds.height * g_ui_camera.zoom
    };
}

int
PushUIInspectChrome(int enabled)
{
    int token;

    ui_inspect_init_from_env();
    token = g_ui_inspect.chrome_depth;
    if(enabled)
        g_ui_inspect.chrome_depth++;
    return token;
}

void
PopUIInspectChrome(int token)
{
    ui_inspect_init_from_env();
    if(token < 0)
        token = 0;
    g_ui_inspect.chrome_depth = token;
}

int
UIInspectInputCapturesClick(Vector2 point)
{
    ui_inspect_init_from_env();
    (void)point;
    if(g_ui_inspect.chrome_depth > 0)
        return 0;
    if(g_ui_inspect.dragging || g_ui_inspect.resizing)
        return 1;
    if(g_ui_inspect.canvas_active &&
       !CheckCollisionPointRec(ui_inspect_mouse_screen(),
                               g_ui_inspect.canvas_bounds))
        return 0;
    return g_ui_inspect.enabled && g_ui_inspect.visible;
}

static void
ui_widget_apply_bounds(const char *id, Rectangle *bounds)
{
    UIInspectOverride *override;

    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled || id == NULL || bounds == NULL)
        return;
    override = ui_inspect_find_override(id);
    if(override == NULL || !override->enabled)
        return;
    if(override->bounds.width > 0.0f && override->bounds.height > 0.0f)
        *bounds = override->bounds;
}

void
PushUIInspectSource(const char *path, int line)
{
    int index;

    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled)
        return;
    if(g_ui_inspect.source_depth >= UI_INSPECT_SOURCE_STACK_MAX)
        return;
    index = g_ui_inspect.source_depth++;
    ui_inspect_strncpy(g_ui_inspect.source_path_stack[index],
                      sizeof(g_ui_inspect.source_path_stack[index]), path);
    g_ui_inspect.source_line_stack[index] = line;
}

void
PopUIInspectSource(void)
{
    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled)
        return;
    if(g_ui_inspect.source_depth > 0)
        g_ui_inspect.source_depth--;
}

static int
ui_inspect_register_widget(const char *id, const char *kind,
                           Rectangle *bounds, int flags)
{
    UIInspectWidget *widget;
    UIInspectOverride *override;
    Rectangle screen_bounds;
    int index;

    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled || id == NULL || id[0] == '\0' ||
       bounds == NULL || g_ui_inspect.widget_count >= UI_INSPECT_MAX_WIDGETS)
        return -1;
    if(g_ui_inspect.chrome_depth > 0)
        return -1;
    screen_bounds = ui_inspect_rect_to_screen(*bounds);
    if(g_ui_inspect.canvas_active &&
       !CheckCollisionRecs(screen_bounds, g_ui_inspect.canvas_bounds))
        return -1;
    index = g_ui_inspect.widget_count++;
    widget = &g_ui_inspect.widgets[index];
    memset(widget, 0, sizeof(*widget));
    ui_inspect_strncpy(widget->id, sizeof(widget->id), id);
    ui_inspect_strncpy(widget->kind, sizeof(widget->kind),
                      kind != NULL ? kind : "widget");
    widget->bounds = *bounds;
    widget->screen_bounds = screen_bounds;
    widget->screen_zoom = g_ui_camera.zoom > 0.0f ? g_ui_camera.zoom : 1.0f;
    widget->flags = flags;
    widget->parent = g_ui_inspect.widget_depth > 0
                         ? g_ui_inspect.widget_stack[g_ui_inspect.widget_depth - 1]
                         : -1;
    widget->depth = g_ui_inspect.widget_depth;
    if(g_ui_inspect.source_depth > 0) {
        int source = g_ui_inspect.source_depth - 1;

        ui_inspect_strncpy(widget->source_path, sizeof(widget->source_path),
                          g_ui_inspect.source_path_stack[source]);
        widget->source_line = g_ui_inspect.source_line_stack[source];
    }
    if(!ui_inspect_persistent_id(id))
        widget->flags |= UI_WIDGET_TEMPORARY_ID;
    widget->order = g_ui_inspect.widget_count;

    override = ui_inspect_find_override(id);
    if(override == NULL && ui_inspect_persistent_id(id) &&
       (flags & (UI_WIDGET_MOVABLE | UI_WIDGET_RESIZABLE)) != 0) {
        override = ui_inspect_get_override(id);
        if(override != NULL)
            override->bounds = *bounds;
    }
    return index;
}

static void
ui_inspect_set_widget_action(int index, const char *action)
{
    UIInspectWidget *widget;

    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled || action == NULL)
        return;
    if(index < 0 || index >= g_ui_inspect.widget_count)
        return;
    widget = &g_ui_inspect.widgets[index];
    ui_inspect_strncpy(widget->action, sizeof(widget->action), action);
}

UIWidget
BeginUIWidget(const char *kind, const char *id, Rectangle bounds, int flags)
{
    UIWidget widget = {0};
    int index;

    ui_inspect_init_from_env();
    ui_inspect_strncpy(widget.kind, sizeof(widget.kind),
                      kind != NULL ? kind : "widget");
    if(id != NULL && id[0] != '\0') {
        ui_inspect_strncpy(widget.id, sizeof(widget.id), id);
    } else {
        snprintf(widget.id, sizeof(widget.id), "tmp:%s:%d",
                 widget.kind, g_ui_inspect.widget_count);
    }
    widget.bounds = bounds;
    widget.flags = flags;
    widget.index = -1;
    if((flags & (UI_WIDGET_MOVABLE | UI_WIDGET_RESIZABLE)) != 0)
        ui_widget_apply_bounds(widget.id, &widget.bounds);
    index = ui_inspect_register_widget(widget.id, widget.kind,
                                      &widget.bounds, flags);
    if(index >= 0) {
        widget.index = index;
        widget.active = 1;
        if(g_ui_inspect.widget_depth < UI_INSPECT_MAX_WIDGETS)
            g_ui_inspect.widget_stack[g_ui_inspect.widget_depth++] = index;
    }
    return widget;
}

void
UIWidgetSetBounds(UIWidget *widget, Rectangle bounds)
{
    UIInspectWidget *inspect;

    ui_inspect_init_from_env();
    if(widget == NULL || !widget->active)
        return;
    widget->bounds = bounds;
    if(widget->index < 0 || widget->index >= g_ui_inspect.widget_count)
        return;
    inspect = &g_ui_inspect.widgets[widget->index];
    inspect->bounds = bounds;
    inspect->screen_bounds = ui_inspect_rect_to_screen(bounds);
    inspect->screen_zoom = g_ui_camera.zoom > 0.0f ? g_ui_camera.zoom : 1.0f;
}

void
UIWidgetSetAction(UIWidget *widget, const char *action)
{
    if(widget == NULL || !widget->active)
        return;
    ui_inspect_set_widget_action(widget->index, action);
}

void
EndUIWidget(UIWidget *widget)
{
    if(widget == NULL || !widget->active)
        return;
    if(g_ui_inspect.widget_depth > 0)
        g_ui_inspect.widget_depth--;
    widget->active = 0;
}

static int
ui_inspect_point_in_resize_handle(Rectangle bounds, Vector2 point)
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
ui_inspect_select_at(Vector2 mouse)
{
    g_ui_inspect.selected = -1;
    for(int i = g_ui_inspect.widget_count - 1; i >= 0; i--) {
        if(CheckCollisionPointRec(mouse, g_ui_inspect.widgets[i].screen_bounds)) {
            g_ui_inspect.selected = i;
            return;
        }
    }
}

static Rectangle
ui_inspect_clamp_bounds(Rectangle bounds)
{
    if(bounds.width < 4.0f)
        bounds.width = 4.0f;
    if(bounds.height < 4.0f)
        bounds.height = 4.0f;
    return bounds;
}

static void
ui_inspect_commit_selected(Rectangle bounds)
{
    UIInspectWidget *widget;
    UIInspectOverride *override;

    if(g_ui_inspect.selected < 0 ||
       g_ui_inspect.selected >= g_ui_inspect.widget_count)
        return;
    widget = &g_ui_inspect.widgets[g_ui_inspect.selected];
    if(!ui_inspect_persistent_id(widget->id))
        return;
    override = ui_inspect_get_override(widget->id);
    if(override == NULL)
        return;
    bounds = ui_inspect_clamp_bounds(bounds);
    override->bounds = bounds;
    override->enabled = 1;
    widget->bounds = bounds;
    g_ui_inspect.dirty = 1;
}

static void
ui_inspect_update_interaction(void)
{
    Vector2 screen_mouse = ui_inspect_mouse_screen();
    UIInspectWidget *selected;
    Rectangle bounds;
    Rectangle screen_bounds;
    int can_move;
    int can_resize;

    if(g_ui_inspect.canvas_active &&
       !CheckCollisionPointRec(screen_mouse, g_ui_inspect.canvas_bounds) &&
       !g_ui_inspect.dragging && !g_ui_inspect.resizing)
        return;

    if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        ui_inspect_select_at(screen_mouse);
        if(g_ui_inspect.selected >= 0) {
            selected = &g_ui_inspect.widgets[g_ui_inspect.selected];
            can_move = (selected->flags & UI_WIDGET_MOVABLE) != 0;
            can_resize = (selected->flags & UI_WIDGET_RESIZABLE) != 0;
            g_ui_inspect.drag_start = screen_mouse;
            g_ui_inspect.edit_start = selected->bounds;
            g_ui_inspect.resizing = can_resize &&
                                   ui_inspect_point_in_resize_handle(
                                       selected->screen_bounds, screen_mouse);
            g_ui_inspect.dragging = can_move && !g_ui_inspect.resizing;
        }
    }
    if(IsMouseButtonDown(MOUSE_BUTTON_LEFT) &&
       (g_ui_inspect.dragging || g_ui_inspect.resizing) &&
       g_ui_inspect.selected >= 0) {
        float dx;
        float dy;
        float zoom;

        selected = &g_ui_inspect.widgets[g_ui_inspect.selected];
        dx = screen_mouse.x - g_ui_inspect.drag_start.x;
        dy = screen_mouse.y - g_ui_inspect.drag_start.y;
        zoom = selected->screen_zoom > 0.0f ? selected->screen_zoom : 1.0f;
        bounds = g_ui_inspect.edit_start;
        screen_bounds = selected->screen_bounds;
        if(g_ui_inspect.dragging) {
            bounds.x += dx / zoom;
            bounds.y += dy / zoom;
            screen_bounds.x += dx;
            screen_bounds.y += dy;
        } else if(g_ui_inspect.resizing) {
            bounds.width += dx / zoom;
            bounds.height += dy / zoom;
            screen_bounds.width += dx;
            screen_bounds.height += dy;
        }
        bounds = ui_inspect_clamp_bounds(bounds);
        ui_inspect_commit_selected(bounds);
        selected->screen_bounds = screen_bounds;
    }
    if(IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        g_ui_inspect.dragging = 0;
        g_ui_inspect.resizing = 0;
        UIConsumeRelease();
    }
    if(g_ui_inspect.selected >= 0 &&
       g_ui_inspect.selected < g_ui_inspect.widget_count) {
        float step = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT)
                         ? 8.0f
                         : 1.0f;

        selected = &g_ui_inspect.widgets[g_ui_inspect.selected];
        bounds = selected->bounds;
        if(IsKeyPressed(KEY_LEFT)) {
            bounds.x -= step;
            ui_inspect_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_RIGHT)) {
            bounds.x += step;
            ui_inspect_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_UP)) {
            bounds.y -= step;
            ui_inspect_commit_selected(bounds);
        }
        if(IsKeyPressed(KEY_DOWN)) {
            bounds.y += step;
            ui_inspect_commit_selected(bounds);
        }
    }
}

void
DrawUIInspectOverlay(void)
{
    Color outline = {40, 180, 255, 170};
    Color selected = {255, 204, 64, 230};
    Color handle = {255, 204, 64, 255};

    ui_inspect_init_from_env();
    if(!g_ui_inspect.enabled)
        return;
    if(!g_ui_inspect.visible)
        return;

    ui_inspect_update_interaction();
    for(int i = 0; i < g_ui_inspect.widget_count; i++) {
        UIInspectWidget *widget = &g_ui_inspect.widgets[i];
        Rectangle screen_bounds = widget->screen_bounds;
        Color color = i == g_ui_inspect.selected ? selected : outline;

        DrawRectangleLinesEx(screen_bounds, i == g_ui_inspect.selected ? 2 : 1,
                             color);
        if(i == g_ui_inspect.selected &&
           (widget->flags & UI_WIDGET_RESIZABLE) != 0) {
            int s = ScaleUIPx(10);

            DrawRectangle((int)(screen_bounds.x + screen_bounds.width - s),
                          (int)(screen_bounds.y + screen_bounds.height - s),
                          s, s, handle);
        }
    }
}

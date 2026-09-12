#include "ui_internal.h"
#include "ui_style_internal.h"
#include "runtime/button.h"
#include "runtime/card.h"
#include "runtime/style.h"
#include "runtime/surface.h"
#include "runtime/text.h"
#include "runtime/grid.h"
#include "ui_image_internal.h"
#include "ui_clip_internal.h"
#include "ui_blend_internal.h"
#include "ui_tree_layout_internal.h"
#include "ui_popup_input_internal.h"
#include "embedded_assets.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

static void
ui_paint_surface(Rectangle bounds, Style style)
{
    ui_draw_material(bounds, (Rectangle){0}, style.background, style.border,
        style.border, style.radius, style.border_width, 0, 0, 0,
        style.focus, 0, style.opacity, ui_style_fill(style), style.material);
}

#define UI_TREE_MAX_DEPTH UI_TREE_LAYOUT_DEPTH
#define UI_NODE_HOVERED (1U << 28)
#define UI_NODE_PRESSED (1U << 29)
#define UI_NODE_OWNS_STATE (1U << 30)
#define UI_NODE_PAINTED_IMMEDIATE (1U << 27)
#define UI_NODE_SCOPE_DISABLED (1U << 26)
#define UI_NODE_INHERIT_FOREGROUND (1U << 25)
#define UI_NODE_TEXT_DISABLED (1U << 24)

typedef struct TextFieldState {
    int cursor;
    int anchor;
    int focused;
    int dragging;
} TextFieldState;

static UIWidgetNode *ui_tree_nodes = NULL;
static int ui_tree_node_count = 0;
static int ui_tree_node_capacity = 0;
static UIWidgetNode *ui_committed_nodes = NULL;
static int ui_committed_node_count = 0;
static int ui_committed_node_capacity = 0;
static UIWidgetNode *ui_reconcile_old_nodes = NULL;
static int ui_reconcile_old_node_capacity = 0;
static int *ui_reconcile_slots = NULL;
static int ui_reconcile_slot_capacity = 0;
static int *ui_reconcile_matched_old = NULL;
static int ui_reconcile_matched_old_capacity = 0;
static int ui_tree_screen_id = 0;
static KeyID ui_tree_screen_key = 0;
static int ui_tree_building = 0;
static int ui_tree_build_activation = 0;
static NodeId ui_tree_stack[UI_TREE_MAX_DEPTH];
static int ui_tree_stack_depth = 0;
static unsigned long ui_tree_declaration;
static RenderTexture2D ui_tree_paint_target;
typedef struct UIPaintCapture {
    RenderTexture2D target;
    Matrix projection, modelview;
    Rectangle clip;
    int has_clip;
    UIBlendState blend;
} UIPaintCapture;
static UIPaintCapture *ui_tree_paint_captures;
static unsigned ui_tree_paint_capture_count, ui_tree_paint_capture_capacity;
static UIPopupInputToken *ui_tree_input_captures;
static unsigned ui_tree_input_capture_count, ui_tree_input_capture_capacity;
static UIPopupInputToken *ui_committed_input_captures;
static unsigned ui_committed_input_capture_capacity;
static unsigned ui_tree_generation = 0;
static unsigned ui_tree_invalid = UI_INVALIDATE_TREE |
                                  UI_INVALIDATE_LAYOUT |
                                  UI_INVALIDATE_PAINT;
static UIEvent *ui_event_queue = NULL;
static int ui_event_capacity = 0;
static int ui_event_head = 0;
static int ui_event_count = 0;
static KeyID ui_tree_text_last_click_key = 0;
static int ui_tree_text_last_click_x = 0;
static int ui_tree_text_last_click_y = 0;
static double ui_tree_text_last_click_time = 0.0;
static double ui_tree_backspace_next_repeat_at = 0.0;
static double ui_tree_delete_next_repeat_at = 0.0;
static UIAccessibilitySink ui_accessibility_sink;
static void *ui_accessibility_sink_userdata;
#if defined(__GNUC__) || defined(__clang__)
extern void kry_platform_accessibility_snapshot(
    const UIAccessibilityNode *nodes, int count) __attribute__((weak));
#else
static void (*kry_platform_accessibility_snapshot)(
    const UIAccessibilityNode *nodes, int count);
#endif

typedef struct UIWidgetOps {
    int (*measure_height)(UIWidgetNode node);
} UIWidgetOps;

static UIWidgetNode *ui_tree_node(NodeId id);
static void ui_tree_note_build_activation(int activated);
static void DrawTree(void);
/* Apply semantic metadata when the retained text is actually painted. */
#if defined(__GNUC__) || defined(__clang__)
extern void kry_dom_semantic_next(int kind, const char *label,
    const char *href, const char *role, int level, int tab_index)
    __attribute__((weak));
#endif

static void
ui_tree_heading_semantic(const char *text, int level)
{
#if defined(__GNUC__) || defined(__clang__)
    if(level > 0 && kry_dom_semantic_next != NULL)
        kry_dom_semantic_next(UI_SEMANTIC_HEADING, text, NULL, NULL, level, -1);
#else
    (void)text;
    (void)level;
#endif
}
static char *ui_tree_numeric_text(const char *label, const char *format, size_t *offset);

static unsigned
ui_tree_capture_input(void)
{
    UIPopupInputToken token = ui_popup_input_snapshot();
    if(!token.context) return 0;
    if(ui_tree_input_capture_count) {
        UIPopupInputToken last = ui_tree_input_captures[ui_tree_input_capture_count-1];
        if(last.context == token.context && last.generation == token.generation &&
           last.order == token.order && last.owner == token.owner)
            return ui_tree_input_capture_count;
    }
    if(ui_tree_input_capture_count == ui_tree_input_capture_capacity) {
        unsigned capacity = ui_tree_input_capture_capacity ? ui_tree_input_capture_capacity*2 : 8;
        size_t bytes = (size_t)capacity*sizeof(UIPopupInputToken);
        if(capacity < ui_tree_input_capture_capacity ||
           bytes/sizeof(UIPopupInputToken) != capacity) abort();
        UIPopupInputToken *items = realloc(ui_tree_input_captures,bytes);
        if(!items) abort();
        ui_tree_input_captures = items;
        ui_tree_input_capture_capacity = capacity;
    }
    ui_tree_input_captures[ui_tree_input_capture_count++] = token;
    return ui_tree_input_capture_count;
}

static UIPopupInputToken
ui_tree_input_snapshot(const UIWidgetNode *node)
{
    UIPopupInputToken *captures = ui_committed_node_count > 0 ?
        ui_committed_input_captures : ui_tree_input_captures;
    return node->popup_input_capture ?
        captures[node->popup_input_capture-1] : (UIPopupInputToken){0};
}

static int
ui_tree_input_blocked(const UIWidgetNode *node, Vector2 point)
{
    return ui_input_captures_snapshot(point,ui_tree_input_snapshot(node));
}

static unsigned
ui_tree_capture_paint(void)
{
    UIPaintCapture capture = {0};
    if(ui_tree_paint_target.id == 0 || !IsWindowReady()) return 0;
    capture.target = ui_tree_paint_target;
    capture.projection = rlGetMatrixProjection();
    capture.modelview = rlGetMatrixModelview();
    capture.has_clip = ui_clip_current(&capture.clip);
    capture.blend = ui_blend_save();
    if(ui_tree_paint_capture_count > 0) {
        UIPaintCapture *last = &ui_tree_paint_captures[ui_tree_paint_capture_count-1];
        if(last->target.id == capture.target.id &&
           last->target.texture.width == capture.target.texture.width &&
           last->target.texture.height == capture.target.texture.height &&
           last->has_clip == capture.has_clip &&
           memcmp(&last->blend,&capture.blend,sizeof(UIBlendState)) == 0 &&
           (!capture.has_clip || memcmp(&last->clip,&capture.clip,sizeof(Rectangle)) == 0) &&
           memcmp(&last->projection,&capture.projection,sizeof(Matrix)) == 0 &&
           memcmp(&last->modelview,&capture.modelview,sizeof(Matrix)) == 0)
            return ui_tree_paint_capture_count;
    }
    if(ui_tree_paint_capture_count == ui_tree_paint_capture_capacity) {
        unsigned capacity = ui_tree_paint_capture_capacity ? ui_tree_paint_capture_capacity*2 : 8;
        size_t bytes = (size_t)capacity*sizeof(UIPaintCapture);
        if(capacity < ui_tree_paint_capture_capacity ||
           bytes/sizeof(UIPaintCapture) != capacity) abort();
        UIPaintCapture *captures = realloc(ui_tree_paint_captures,bytes);
        if(captures == NULL) abort();
        ui_tree_paint_captures = captures;
        ui_tree_paint_capture_capacity = capacity;
    }
    ui_tree_paint_captures[ui_tree_paint_capture_count++] = capture;
    return ui_tree_paint_capture_count;
}

RenderTexture2D
ui_tree_set_paint_target(RenderTexture2D target)
{
    RenderTexture2D previous = ui_tree_paint_target;
    ui_tree_paint_target = target;
    /* Captured textures may be cleared every frame even for unchanged nodes. */
    ui_tree_invalid |= UI_INVALIDATE_PAINT;
    return previous;
}

static int
ui_tree_key_repeat_count(int key, double *next_repeat_at)
{
    double now;
    int count = 0;

    if(IsKeyPressed(key)) {
        *next_repeat_at = GetTime() + 0.34;
        return 1;
    }
    if(!IsKeyDown(key)) {
        *next_repeat_at = 0.0;
        return 0;
    }
    now = GetTime();
    if(*next_repeat_at <= 0.0) {
        *next_repeat_at = now + 0.34;
        return 0;
    }
    while(now >= *next_repeat_at && count < 8) {
        count++;
        *next_repeat_at += 0.045;
    }
    return count;
}

static char *
ui_tree_strdup(const char *text)
{
    size_t size;
    char *copy;

    if(text == NULL)
        text = "";
    size = strlen(text) + 1;
    copy = malloc(size);
    if(copy != NULL)
        memcpy(copy, text, size);
    return copy;
}

static void
ui_tree_clear_pending(void)
{
    int i;

    for(i = 0; i < ui_tree_node_count; i++) {
        free(ui_tree_nodes[i].owned_text);
        ui_tree_nodes[i].owned_text = NULL;
    }
}

static void
ui_event_push(UIEvent event)
{
    int tail;

    if(ui_event_count >= ui_event_capacity) {
        int next = ui_event_capacity > 0 ? ui_event_capacity * 2 : 64;
        UIEvent *grown = malloc((size_t)next * sizeof(*grown));
        int i;

        if(grown == NULL)
            return;
        for(i = 0; i < ui_event_count; i++)
            grown[i] = ui_event_queue[(ui_event_head + i) %
                                      ui_event_capacity];
        free(ui_event_queue);
        ui_event_queue = grown;
        ui_event_capacity = next;
        ui_event_head = 0;
    }
    tail = (ui_event_head + ui_event_count) % ui_event_capacity;
    ui_event_queue[tail] = event;
    ui_event_count++;
}

static void
ui_text_field_event(UIWidgetNode *node, UIEventKind kind, double timestamp)
{
    UIEvent event;
    TextFieldState *state = node != NULL ? node->state : NULL;

    if(node == NULL)
        return;
    memset(&event, 0, sizeof(event));
    event.key = node->key;
    event.kind = kind;
    event.timestamp = timestamp;
    if(kind == UI_EVENT_SELECTION_CHANGED && state != NULL) {
        event.data.selection.start = state->anchor < state->cursor
            ? state->anchor : state->cursor;
        event.data.selection.end = state->anchor > state->cursor
            ? state->anchor : state->cursor;
    } else if(kind == UI_EVENT_TEXT_CHANGED &&
              node->data.text_field.text != NULL) {
        event.data.text.bytes = (int)strlen(node->data.text_field.text);
    }
    ui_event_push(event);
}

static int
ui_tree_reserve(UIWidgetNode **nodes, int *capacity, int needed)
{
    UIWidgetNode *grown;
    int next;

    if(needed <= *capacity)
        return 1;
    next = *capacity > 0 ? *capacity : 64;
    while(next < needed) {
        if(next > 0x3fffffff)
            return 0;
        next *= 2;
    }
    grown = realloc(*nodes, (size_t)next * sizeof(*grown));
    if(grown == NULL)
        return 0;
    *nodes = grown;
    *capacity = next;
    return 1;
}

static int
ui_tree_reserve_ints(int **items, int *capacity, int needed)
{
    int *grown;
    int next;

    if(needed <= *capacity)
        return 1;
    next = *capacity > 0 ? *capacity : 64;
    while(next < needed) {
        if(next > 0x3fffffff)
            return 0;
        next *= 2;
    }
    grown = realloc(*items, (size_t)next * sizeof(*grown));
    if(grown == NULL)
        return 0;
    *items = grown;
    *capacity = next;
    return 1;
}

static unsigned long long
ui_reconcile_hash(KeyID parent, KeyID key, UIWidgetKind kind)
{
    unsigned long long hash = key ^ (parent + 0x9e3779b97f4a7c15ULL +
                                     (key << 6) + (key >> 2));

    hash ^= (unsigned long long)(unsigned)kind * 0x9e3779b185ebca87ULL;
    hash ^= hash >> 30;
    hash *= 0xbf58476d1ce4e5b9ULL;
    hash ^= hash >> 27;
    hash *= 0x94d049bb133111ebULL;
    return hash ^ (hash >> 31);
}

static int
ui_reconcile_same_identity(const UIWidgetNode *old_nodes, int old_index,
                           const UIWidgetNode *new_nodes, int new_index)
{
    const UIWidgetNode *old_node = &old_nodes[old_index];
    const UIWidgetNode *new_node = &new_nodes[new_index];
    KeyID old_parent = old_node->parent >= 0
        ? old_nodes[old_node->parent].key : 0;
    KeyID new_parent = new_node->parent >= 0
        ? new_nodes[new_node->parent].key : 0;

    return old_node->key == new_node->key &&
           old_node->kind == new_node->kind &&
           old_parent == new_parent;
}

static size_t
ui_tree_owned_text_size(const UIWidgetNode *node)
{
    if(node->owned_text == NULL) return 0;
    size_t offset = 0;
    if(node->kind == UI_WIDGET_SLIDER_NODE)
        offset = node->data.slider.format_offset;
    if(node->kind == UI_WIDGET_DRAG_NODE)
        offset = node->data.drag.format_offset;
    return offset+strlen(node->owned_text+offset)+1;
}

static int
ui_tree_button_like_kind(UIWidgetKind kind)
{
    return kind == UI_WIDGET_BUTTON_NODE || kind == UI_WIDGET_CARD_NODE;
}

static int
ui_tree_interactive_button_like(const UIWidgetNode *node)
{
    if(node == NULL || !ui_tree_button_like_kind(node->kind))
        return 0;
    if(node->kind == UI_WIDGET_CARD_NODE && node->data.button.props.id <= 0)
        return 0;
    return CanActivate(node->data.button.props.disabled,
                       node->data.button.props.loading);
}

static int
ui_reconcile_node_changed(const UIWidgetNode *old_node,
                          const UIWidgetNode *new_node)
{
    UIWidgetData old_data;
    UIWidgetData new_data;

    if(old_node == NULL || new_node == NULL)
        return 1;
    if(old_node->paint_capture != new_node->paint_capture)
        return 1;
    if(old_node->font_token != new_node->font_token)
        return 1;
    if(old_node->id != new_node->id ||
       old_node->key != new_node->key ||
       old_node->kind != new_node->kind ||
       old_node->parent != new_node->parent ||
       old_node->first_child != new_node->first_child ||
       old_node->next_sibling != new_node->next_sibling)
        return 1;
    if(memcmp(&old_node->declared_bounds, &new_node->declared_bounds,
              sizeof(old_node->declared_bounds)) != 0)
        return 1;
    old_data = old_node->data;
    if((old_node->flags & UI_NODE_SCOPE_DISABLED) != (new_node->flags & UI_NODE_SCOPE_DISABLED)) return 1;
    unsigned text_flags = UI_NODE_INHERIT_FOREGROUND | UI_NODE_TEXT_DISABLED;
    if((old_node->flags & text_flags) != (new_node->flags & text_flags))
        return 1;
    if(old_node->has_input_clip != new_node->has_input_clip ||
       (new_node->has_input_clip && memcmp(&old_node->input_clip,&new_node->input_clip,sizeof(new_node->input_clip)) != 0))
        return 1;
    new_data = new_node->data;
    if(ui_tree_button_like_kind(old_node->kind)) {
        old_data.button.props.label = NULL;
        new_data.button.props.label = NULL;
    }
    if(memcmp(&old_data, &new_data, sizeof(old_data)) != 0)
        return 1;
    size_t old_size = ui_tree_owned_text_size(old_node);
    size_t new_size = ui_tree_owned_text_size(new_node);
    return old_size != new_size ||
        (new_size > 0 && memcmp(old_node->owned_text,new_node->owned_text,new_size) != 0);
}

/* Resolve grid cells before handling immediate input and again during layout. */
static void
ui_layout_grid_children(UIWidgetNode *nodes, UIWidgetNode *parent)
{
    NodeId child;
    GridProps props = {0};
    GridCursor cursor;

    props.bounds = parent->bounds;
    props.columns = parent->data.layout.columns;
    props.min_item_width = parent->data.layout.min_item_width;
    props.max_columns = parent->data.layout.max_columns;
    props.gap = parent->data.layout.gap;
    props.padding = parent->data.layout.padding;
    cursor = BeginGridCursor(props);

    for(child = parent->first_child; child >= 0;
        child = nodes[child].next_sibling) {
        UIWidgetNode *node = &nodes[child];
        int height;

        if(node->declared_bounds.x != 0 || node->declared_bounds.y != 0)
            continue;
        height = (int)ceilf(node->bounds.height);
        if(height <= 0)
            height = GetNodeHeight(*node);
        cursor = GridStep(cursor, height, 1);
        node->bounds = cursor.item;
    }
}


static NodeId
ui_tree_add(int id, UIWidgetKind kind, Rectangle bounds, const void *props)
{
    UIWidgetNode *node;
    UIWidgetNode *parent;
    NodeId parent_id;
    int index;

    if(!ui_tree_building)
        return -1;
    if(!ui_tree_reserve(&ui_tree_nodes, &ui_tree_node_capacity,
                        ui_tree_node_count + 1))
        return -1;
    index = ui_tree_node_count++;
    node = &ui_tree_nodes[index];
    memset(node, 0, sizeof(*node));
    node->id = id;
    node->key = (KeyID)(unsigned)id;
    node->kind = kind;
    node->bounds = bounds;
    node->declared_bounds = bounds;
    node->has_input_clip = ui_current_input_clip(&node->input_clip);
    node->paint_capture = ui_tree_capture_paint();
    node->popup_input_capture = ui_tree_capture_input();
    node->font_token = ui_active_font_token();
    if(UIContentDisabled()) node->flags |= UI_NODE_SCOPE_DISABLED;
    node->props = props;
    node->parent = -1;
    node->first_child = -1;
    node->next_sibling = -1;
    if(ui_tree_building && ui_tree_stack_depth > 0) {
        parent_id = ui_tree_stack[ui_tree_stack_depth - 1];
        parent = ui_tree_node(parent_id);
        if(parent != NULL) {
            node->parent = parent_id;
            if(parent->first_child < 0) {
                parent->first_child = index;
            } else {
                NodeId child = parent->first_child;

                while(child >= 0 && ui_tree_nodes[child].next_sibling >= 0)
                    child = ui_tree_nodes[child].next_sibling;
                if(child >= 0)
                    ui_tree_nodes[child].next_sibling = index;
            }
        }
    }
    if(node->parent >= 0 && bounds.x == 0 && bounds.y == 0) {
        parent = ui_tree_node(node->parent);
        if(parent->kind == UI_WIDGET_GRID_NODE)
            ui_layout_grid_children(ui_tree_nodes, parent);
        if(parent->kind == UI_WIDGET_ROW_NODE || parent->kind == UI_WIDGET_COLUMN_NODE) {
            float pad = (float)parent->data.layout.padding;
            float cursor = parent->kind == UI_WIDGET_ROW_NODE ? parent->bounds.x+pad : parent->bounds.y+pad;
            for(NodeId sibling = parent->first_child; sibling >= 0 && sibling != index; sibling = ui_tree_nodes[sibling].next_sibling) {
                UIWidgetNode *previous = &ui_tree_nodes[sibling];
                if(previous->declared_bounds.x != 0 || previous->declared_bounds.y != 0) continue;
                cursor += (parent->kind == UI_WIDGET_ROW_NODE ? previous->bounds.width : previous->bounds.height) + parent->data.layout.gap;
            }
            if(parent->kind == UI_WIDGET_ROW_NODE) {
                node->bounds.x = cursor; node->bounds.y = parent->bounds.y+pad;
                if(node->bounds.height <= 0) node->bounds.height = parent->bounds.height-2*pad;
            } else {
                node->bounds.x = parent->bounds.x+pad; node->bounds.y = cursor;
                if(node->bounds.width <= 0) node->bounds.width = parent->bounds.width-2*pad;
            }
        }
    }
    return index;
}

static UIWidgetNode *
ui_tree_node(NodeId id)
{
    if(id < 0 || id >= ui_tree_node_count)
        return NULL;
    return &ui_tree_nodes[id];
}

static UIWidgetNode
ui_node(int id, UIWidgetKind kind, Rectangle bounds)
{
    UIWidgetNode node;

    memset(&node, 0, sizeof(node));
    node.id = id;
    node.kind = kind;
    node.bounds = bounds;
    node.declared_bounds = bounds;
    node.parent = -1;
    node.first_child = -1;
    node.next_sibling = -1;
    return node;
}

static void
ui_tree_store_node(NodeId id, UIWidgetNode src)
{
    UIWidgetNode *dst;

    dst = ui_tree_node(id);
    if(dst == NULL)
        return;
    src.parent = dst->parent;
    src.first_child = dst->first_child;
    src.next_sibling = dst->next_sibling;
    src.declared_bounds = dst->declared_bounds;
    src.input_clip = dst->input_clip;
    src.has_input_clip = dst->has_input_clip;
    src.paint_capture = dst->paint_capture;
    src.popup_input_capture = dst->popup_input_capture;
    src.flags |= dst->flags & UI_NODE_SCOPE_DISABLED;
    *dst = src;
}

static void
ui_tree_mark_painted_immediate(NodeId id)
{
    UIWidgetNode *node;

    if(!ui_tree_building)
        return;
    node = ui_tree_node(id);
    if(node != NULL)
        node->flags |= UI_NODE_PAINTED_IMMEDIATE;
}

static void
ui_tree_note_build_activation(int activated)
{
    if(ui_tree_building && activated)
        ui_tree_build_activation = 1;
}

static int
ui_tree_node_uses_retained_layout(NodeId id)
{
    UIWidgetNode *node = ui_tree_node(id);
    UIWidgetNode *parent;

    if(node == NULL || node->parent < 0)
        return 0;
    parent = ui_tree_node(node->parent);
    if(parent == NULL)
        return 0;
    return parent->kind == UI_WIDGET_COLUMN_NODE ||
           parent->kind == UI_WIDGET_ROW_NODE ||
           parent->kind == UI_WIDGET_GRID_NODE ||
           parent->kind == UI_WIDGET_STACK_NODE ||
           parent->kind == UI_WIDGET_ROUTER_NODE ||
           ui_tree_button_like_kind(parent->kind);
}

static int
ui_measure_bounds_height(UIWidgetNode node)
{
    return (int)ceilf(node.bounds.height);
}

static int
ui_measure_paragraph(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_paragraph_height(*(const ParagraphSpec *)node.props);
    return ui_paragraph_height(node.data.paragraph);
}

static int
ui_measure_readonly_text_box(UIWidgetNode node)
{
    const ReadonlyTextBoxProps *box;

    box = node.props != NULL ? node.props : &node.data.readonly_text_box;
    return ui_readonly_text_box_height(box->text, box->font,
                                       (int)box->bounds.width,
                                       box->style, box->line_gap);
}

static int
ui_measure_navigation_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_navigation_bar_height();
}

static int
ui_measure_tab_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_tab_bar_height();
}

static int
ui_measure_paragraph_modal(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_paragraph_modal_height(*(const ParagraphModalMeasureProps *)node.props);
    return ui_paragraph_modal_height(node.data.paragraph_modal);
}

static int
ui_measure_title_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_title_bar_height();
}

static const UIWidgetOps ui_widget_ops[] = {
    [UI_WIDGET_SCREEN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_BACKGROUND_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_RECT_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CIRCLE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_LINE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TRIANGLE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_BUTTON_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_FIELD_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_AREA_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_DROPDOWN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_SLIDER_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TOGGLE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CHECKBOX_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_PARAGRAPH_NODE] = {ui_measure_paragraph},
    [UI_WIDGET_READONLY_TEXT_BOX_NODE] = {ui_measure_readonly_text_box},
    [UI_WIDGET_NAVIGATION_BAR_NODE] = {ui_measure_navigation_bar},
    [UI_WIDGET_TAB_BAR_NODE] = {ui_measure_tab_bar},
    [UI_WIDGET_PARAGRAPH_MODAL_NODE] = {ui_measure_paragraph_modal},
    [UI_WIDGET_TITLE_BAR_NODE] = {ui_measure_title_bar},
    [UI_WIDGET_GROUP_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_COLUMN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_ROW_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_STACK_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_GRID_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_IMAGE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CUSTOM_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_DRAG_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_INPUT_PAINT_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_ROUTER_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CARD_NODE] = {ui_measure_bounds_height},
};

KeyID
Key(const char *text)
{
    KeyID hash = 1469598103934665603ULL;

    if(text == NULL)
        return 0;
    while(*text != '\0') {
        hash ^= (unsigned char)*text++;
        hash *= 1099511628211ULL;
    }
    return hash != 0 ? hash : 1;
}

void
BeginTree(KeyID screen_key)
{
    NodeId root;
    ui_tree_declaration++;

    /* Embedders that never call SetUIFrame still need valid screen-to-world
     * math for input routing; a zero camera would turn every hit test into
     * NaN comparisons that silently never match. */
    ui_camera_ensure_sane();
    /* A new declaration belongs to a new presentation frame. Its framebuffer
     * or DOM node stream may have been cleared even when the tree is unchanged. */
    if(IsWindowReady())
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    ui_tree_clear_pending();
    ui_tree_screen_key = screen_key != 0 ? screen_key : 1;
    ui_tree_screen_id = (int)(ui_tree_screen_key & 0x7fffffffU);
    ui_tree_node_count = 0;
    ui_tree_paint_target = (RenderTexture2D){0};
    ui_tree_paint_capture_count = 0;
    ui_tree_input_capture_count = 0;
    ui_tree_building = 1;
    ui_tree_build_activation = 0;
    ui_tree_stack_depth = 0;
    root = ui_tree_add(ui_tree_screen_id, UI_WIDGET_SCREEN_NODE,
                       (Rectangle){0, 0, ui_view_width, ui_view_height}, NULL);
    if(root >= 0) {
        ui_tree_nodes[root].key = ui_tree_screen_key;
        ui_tree_stack[ui_tree_stack_depth++] = root;
    }
}

void
EndTree(void)
{
    static int trace_enabled = -1;
    static unsigned long trace_frame;
    double start = GetTime(), reconcile, layout, input, update, draw;

    if(trace_enabled < 0)
        trace_enabled = getenv("KRYON_FRAME_TRACE") != NULL;
    ui_tree_building = 0;
    ui_tree_stack_depth = 0;
    ReconcileTree();
    reconcile = GetTime();
    LayoutTree();
    layout = GetTime();
    RouteInput();
    input = GetTime();
    UpdateTree();
    update = GetTime();
    DrawTree();
    if(ui_accessibility_sink != NULL ||
       kry_platform_accessibility_snapshot != NULL) {
        int count = GetAccessibilitySnapshot(NULL, 0);
        UIAccessibilityNode *nodes = count > 0
            ? malloc((size_t)count * sizeof(*nodes)) : NULL;

        if(nodes != NULL) {
            (void)GetAccessibilitySnapshot(nodes, count);
            if(kry_platform_accessibility_snapshot != NULL)
                kry_platform_accessibility_snapshot(nodes, count);
            if(ui_accessibility_sink != NULL)
                ui_accessibility_sink(nodes, count,
                                      ui_accessibility_sink_userdata);
            free(nodes);
        }
    }
    Overlays();
    draw = GetTime();
    trace_frame++;
    /* Trace is deliberately sampled, plus every budget violation.  Writing a
     * line per frame would itself perturb the latency measurement. */
    if(trace_enabled && (trace_frame % 120 == 0 ||
        (input - layout) * 1e6 >= 1000.0 || (draw - start) * 1e6 >= 4000.0)) {
        fprintf(stderr,
                "{\"kryon_frame\":true,\"frame\":%lu,\"nodes\":%d,\"reconcile_us\":%.1f,\"layout_us\":%.1f,\"input_us\":%.1f,\"update_us\":%.1f,\"draw_us\":%.1f,\"total_us\":%.1f}\n",
                trace_frame, ui_committed_node_count, (reconcile-start)*1e6,
                (layout-reconcile)*1e6, (input-layout)*1e6,
                (update-input)*1e6, (draw-update)*1e6,
                (draw-start)*1e6);
    }
}

UITreeLayoutScope
ui_tree_layout_suspend(void)
{
    UITreeLayoutScope scope = {0};
    if(ui_tree_building && ui_tree_stack_depth < 1) abort();
    scope.depth = ui_tree_stack_depth;
    scope.building = ui_tree_building;
    scope.declaration = ui_tree_declaration;
    memcpy(scope.stack,ui_tree_stack,(size_t)scope.depth*sizeof(NodeId));
    /* Keep the screen root, but detach from the owner's Row/Column path.
     * Popup children remain in the same retained tree and lifetime. */
    ui_tree_stack_depth = ui_tree_building ? 1 : 0;
    return scope;
}

void
ui_tree_layout_resume(UITreeLayoutScope scope)
{
    if(scope.depth < 0 || scope.depth > UI_TREE_MAX_DEPTH ||
       scope.declaration != ui_tree_declaration || scope.building != ui_tree_building ||
       ui_tree_stack_depth != (scope.building ? 1 : 0)) abort();
    memcpy(ui_tree_stack,scope.stack,(size_t)scope.depth*sizeof(NodeId));
    ui_tree_stack_depth = scope.depth;
}

void
End(void)
{
    if(ui_tree_stack_depth > 1)
        ui_tree_stack_depth--;
}

void
InvalidateTree(UIInvalidation invalidation)
{
    ui_tree_invalid |= (unsigned)invalidation;
}

int
NextEvent(UIEvent *event)
{
    if(event == NULL || ui_event_count <= 0)
        return 0;
    *event = ui_event_queue[ui_event_head];
    ui_event_head = (ui_event_head + 1) % ui_event_capacity;
    ui_event_count--;
    return 1;
}

int
SetSelection(KeyID key, int anchor, int cursor)
{
    int i;

    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        TextFieldState *state;
        int length;

        if(node->key != key ||
           (node->kind != UI_WIDGET_TEXT_FIELD_NODE &&
            node->kind != UI_WIDGET_TEXT_AREA_NODE))
            continue;
        state = node->state;
        if(node->kind == UI_WIDGET_TEXT_FIELD_NODE) {
            if(state == NULL || node->data.text_field.text == NULL)
                return 0;
            length = (int)strlen(node->data.text_field.text);
        } else {
            if(state == NULL || node->data.text_area.text == NULL)
                return 0;
            length = (int)strlen(node->data.text_area.text);
        }
        state->anchor = ui_clampi(anchor, 0, length);
        state->cursor = ui_clampi(cursor, 0, length);
        if(node->kind == UI_WIDGET_TEXT_FIELD_NODE) {
            if(node->data.text_field.cursor_position != NULL)
                *node->data.text_field.cursor_position = state->cursor;
        } else if(node->data.text_area.cursor_position != NULL) {
            *node->data.text_area.cursor_position = state->cursor;
        }
        ui_text_field_event(node, UI_EVENT_SELECTION_CHANGED, GetTime());
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
        return 1;
    }
    return 0;
}

void
ReconcileTree(void)
{
    int *slots;
    int *matched_old;
    UIWidgetNode *old_nodes;
    int old_count = ui_committed_node_count;
    int slot_count = 1;
    int tree_changed;
    unsigned invalid_before = ui_tree_invalid;
    int i;

    if(ui_tree_build_activation && ui_committed_node_count > 0 &&
       ui_committed_nodes[0].key == ui_tree_screen_key &&
       ui_tree_node_count < ui_committed_node_count) {
        ui_tree_clear_pending();
        ui_tree_node_count = 0;
        ui_tree_build_activation = 0;
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
        return;
    }
    ui_tree_build_activation = 0;
    if(!ui_tree_reserve(&ui_committed_nodes, &ui_committed_node_capacity,
                        ui_tree_node_count))
        return;
    tree_changed = old_count != ui_tree_node_count;
    if(old_count > 0) {
        if(!ui_tree_reserve(&ui_reconcile_old_nodes,
                            &ui_reconcile_old_node_capacity, old_count))
            return;
        old_nodes = ui_reconcile_old_nodes;
        memcpy(old_nodes, ui_committed_nodes,
               (size_t)old_count * sizeof(*old_nodes));
    } else {
        old_nodes = NULL;
    }
    while(slot_count < old_count * 2 + 1)
        slot_count *= 2;
    if(!ui_tree_reserve_ints(&ui_reconcile_slots,
                             &ui_reconcile_slot_capacity, slot_count))
        return;
    slots = ui_reconcile_slots;
    if(ui_tree_node_count > 0) {
        if(!ui_tree_reserve_ints(&ui_reconcile_matched_old,
                                 &ui_reconcile_matched_old_capacity,
                                 ui_tree_node_count))
            return;
        matched_old = ui_reconcile_matched_old;
        for(i = 0; i < ui_tree_node_count; i++)
            matched_old[i] = -1;
    } else {
        matched_old = NULL;
    }
    for(i = 0; i < slot_count; i++)
        slots[i] = -1;
    for(i = 0; i < old_count; i++) {
        UIWidgetNode *node = &old_nodes[i];
        KeyID parent = node->parent >= 0
            ? old_nodes[node->parent].key : 0;
        unsigned slot = (unsigned)(ui_reconcile_hash(parent, node->key,
                                                      node->kind) &
                                    (unsigned long long)(slot_count - 1));

        while(slots[slot] >= 0)
            slot = (slot + 1U) & (unsigned)(slot_count - 1);
        slots[slot] = i;
    }
    ui_tree_generation++;
    for(i = 0; i < ui_tree_node_count; i++) {
        UIWidgetNode next = ui_tree_nodes[i];
        KeyID parent = next.parent >= 0 ? ui_tree_nodes[next.parent].key : 0;
        unsigned slot = (unsigned)(ui_reconcile_hash(parent, next.key,
                                                      next.kind) &
                                    (unsigned long long)(slot_count - 1));

        next.generation = ui_tree_generation;
        next.state = NULL;
        while(slots[slot] >= 0) {
            int old = slots[slot];

            if(ui_reconcile_same_identity(old_nodes, old,
                                          ui_tree_nodes, i)) {
                next.state = old_nodes[old].state;
                next.flags |= old_nodes[old].flags &
                    (UI_NODE_OWNS_STATE | UI_NODE_HOVERED | UI_NODE_PRESSED);
                matched_old[i] = old;
                if(ui_reconcile_node_changed(&old_nodes[old], &next))
                    tree_changed = 1;
                old_nodes[old].flags &= ~UI_NODE_OWNS_STATE;
                break;
            }
            slot = (slot + 1U) & (unsigned)(slot_count - 1);
        }
        if(matched_old[i] < 0)
            tree_changed = 1;
        ui_committed_nodes[i] = next;
        ui_tree_nodes[i].owned_text = NULL;
    }
    if(!tree_changed && (invalid_before & UI_INVALIDATE_LAYOUT) == 0) {
        for(i = 0; i < ui_tree_node_count; i++) {
            int old = matched_old != NULL ? matched_old[i] : -1;

            if(old >= 0)
                ui_committed_nodes[i].bounds = old_nodes[old].bounds;
        }
    }
    for(i = 0; i < old_count; i++) {
        free(old_nodes[i].owned_text);
        if((old_nodes[i].flags & UI_NODE_OWNS_STATE) != 0)
            free(old_nodes[i].state);
    }
    for(i = 0; i < ui_tree_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];

        if((node->kind == UI_WIDGET_TEXT_FIELD_NODE ||
            node->kind == UI_WIDGET_TEXT_AREA_NODE) &&
           node->state == NULL) {
            TextFieldState *state = calloc(1, sizeof(*state));

            if(state != NULL) {
                char *text = node->kind == UI_WIDGET_TEXT_FIELD_NODE
                    ? node->data.text_field.text : node->data.text_area.text;
                int *cursor_position = node->kind == UI_WIDGET_TEXT_FIELD_NODE
                    ? node->data.text_field.cursor_position
                    : node->data.text_area.cursor_position;
                int *focused = node->kind == UI_WIDGET_TEXT_FIELD_NODE
                    ? node->data.text_field.focused : node->data.text_area.focused;
                int length = text != NULL ? (int)strlen(text) : 0;

                state->cursor = cursor_position != NULL
                    ? *cursor_position : length;
                state->anchor = state->cursor;
                state->focused = focused != NULL ? *focused != 0 : 0;
                node->state = state;
                node->flags |= UI_NODE_OWNS_STATE;
            }
        }
    }
    /* Keep the displayed tree's snapshots intact while its replacement is
     * being declared. HitTestNode may still query that committed tree. */
    if(ui_committed_input_capture_capacity < ui_tree_input_capture_count) {
        UIPopupInputToken *captures = realloc(ui_committed_input_captures,
            (size_t)ui_tree_input_capture_count*sizeof(*captures));
        if(!captures) abort();
        ui_committed_input_captures = captures;
        ui_committed_input_capture_capacity = ui_tree_input_capture_count;
    }
    if(ui_tree_input_capture_count)
        memcpy(ui_committed_input_captures,ui_tree_input_captures,
            (size_t)ui_tree_input_capture_count*sizeof(*ui_tree_input_captures));
    ui_committed_node_count = ui_tree_node_count;
    if(tree_changed)
        ui_tree_invalid |= UI_INVALIDATE_LAYOUT | UI_INVALIDATE_PAINT;
}

void
LayoutTree(void)
{
    int i;

    if((ui_tree_invalid & UI_INVALIDATE_LAYOUT) == 0)
        return;
    for(i = ui_committed_node_count - 1; i >= 0; i--) {
        UIWidgetNode *node = &ui_committed_nodes[i];

        if(node->bounds.height <= 0)
            node->bounds.height = (float)GetNodeHeight(*node);
    }
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *parent = &ui_committed_nodes[i];
        int child;
        float cursor;
        float content_x;
        float content_y;
        float content_w;
        float content_h;

        if(parent->kind != UI_WIDGET_COLUMN_NODE &&
           parent->kind != UI_WIDGET_ROW_NODE &&
           parent->kind != UI_WIDGET_STACK_NODE &&
           parent->kind != UI_WIDGET_GRID_NODE &&
           !ui_tree_button_like_kind(parent->kind))
            continue;
        if(ui_tree_button_like_kind(parent->kind)) {
            ButtonSpec *button = &parent->data.button;
            float scale = (float)Scale(1000) / 1000.0f;
            Style style = {.padding_x = 8, .padding_y = 8};
            if(button->style_resolved)
                style = ResolveButtonStyle(button->props, button->props.state);
            Rectangle content = InsetBounds(parent->bounds, style.padding_x, style.padding_y, scale);
            content_x = content.x;
            content_y = content.y;
            content_w = content.width;
            content_h = content.height;
        } else {
            content_x = parent->bounds.x + parent->data.layout.padding;
            content_y = parent->bounds.y + parent->data.layout.padding;
            content_w = parent->bounds.width - parent->data.layout.padding * 2;
            content_h = parent->bounds.height - parent->data.layout.padding * 2;
        }
        if(content_w < 0)
            content_w = 0;
        if(content_h < 0)
            content_h = 0;
        cursor = parent->kind == UI_WIDGET_ROW_NODE ? content_x : content_y;
        if(parent->kind == UI_WIDGET_GRID_NODE) {
            ui_layout_grid_children(ui_committed_nodes, parent);
            continue;
        }
        if(ui_tree_button_like_kind(parent->kind)) {
            for(child = parent->first_child; child >= 0;
                child = ui_committed_nodes[child].next_sibling) {
                UIWidgetNode *node = &ui_committed_nodes[child];

                Rectangle content = {content_x, content_y, content_w, content_h};
                node->bounds = CenterChild(node->declared_bounds, node->bounds, content);
            }
            continue;
        }
        for(child = parent->first_child; child >= 0;
            child = ui_committed_nodes[child].next_sibling) {
            UIWidgetNode *node = &ui_committed_nodes[child];

            if(node->declared_bounds.x != 0 || node->declared_bounds.y != 0) continue;
            if(parent->kind == UI_WIDGET_COLUMN_NODE) {
                node->bounds.x = content_x;
                node->bounds.y = cursor;
                if(node->bounds.width <= 0)
                    node->bounds.width = content_w;
                cursor += node->bounds.height + parent->data.layout.gap;
            } else if(parent->kind == UI_WIDGET_ROW_NODE) {
                node->bounds.x = cursor;
                node->bounds.y = content_y;
                if(node->bounds.height <= 0)
                    node->bounds.height = content_h;
                cursor += node->bounds.width + parent->data.layout.gap;
            } else {
                node->bounds.x = content_x;
                node->bounds.y = content_y;
                if(node->bounds.width <= 0)
                    node->bounds.width = content_w;
                if(node->bounds.height <= 0)
                    node->bounds.height = content_h;
            }
        }
    }
    ui_tree_invalid &= ~UI_INVALIDATE_LAYOUT;
}

void
RouteInput(void)
{
    Vector2 mouse;
    int hit;
    int target;
    int i;
    int pressed;
    int backspace_count;
    int delete_count;

    if(ui_committed_node_count <= 0)
        return;

    backspace_count = ui_tree_key_repeat_count(
        KEY_BACKSPACE, &ui_tree_backspace_next_repeat_at);
    delete_count = ui_tree_key_repeat_count(
        KEY_DELETE, &ui_tree_delete_next_repeat_at);

    /* The retained tree owns focus order. Register every interactive node
     * before routing input so Tab follows declaration order for fields and
     * buttons exactly as it does in the immediate UI API. */
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        int focus_id = 0;

        if((node->flags & UI_NODE_SCOPE_DISABLED) != 0) continue;
        if(node->kind == UI_WIDGET_TEXT_FIELD_NODE)
            focus_id = node->data.text_field.focus_id;
        else if(node->kind == UI_WIDGET_TEXT_AREA_NODE)
            focus_id = node->data.text_area.focus_id;
        else if(ui_tree_interactive_button_like(node))
            focus_id = node->data.button.props.id;
        if(node->has_input_clip) PushUIInputClip(node->input_clip);
        if(UIFocusFrameOpen() && focus_id > 0)
            (void)ui_register_focus_snapshot(focus_id, node->bounds,
                ui_tree_input_snapshot(node));
        if(node->has_input_clip) PopUIInputClip();
    }

    mouse = ui_mouse_world();
    hit = HitTestNode(mouse);
    pressed = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        unsigned before;

        if(!ui_tree_interactive_button_like(node))
            continue;
        before = node->flags;
        node->flags &= ~(UI_NODE_HOVERED | UI_NODE_PRESSED);
        if((node->flags & UI_NODE_SCOPE_DISABLED) == 0 &&
           !ui_tree_input_blocked(node,mouse) &&
           (!node->has_input_clip || CheckCollisionPointRec(mouse,node->input_clip)) &&
           CheckCollisionPointRec(mouse, node->bounds)) {
            node->flags |= UI_NODE_HOVERED;
            if(pressed)
                node->flags |= UI_NODE_PRESSED;
        }
        if(before != node->flags)
            ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    target = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? hit : -1;
    if(target >= 0 &&
       ui_tree_interactive_button_like(&ui_committed_nodes[target])) {
        UIEvent event;

        memset(&event, 0, sizeof(event));
        event.key = ui_committed_nodes[target].key;
        event.kind = UI_EVENT_CLICK;
        event.timestamp = GetTime();
        ui_event_push(event);
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        UIEvent event;

        if(!UIFocusFrameOpen() ||
           !ui_tree_interactive_button_like(node) ||
           !IsUIFocusActivatePressed(node->data.button.props.id))
            continue;
        memset(&event, 0, sizeof(event));
        event.key = node->key;
        event.kind = UI_EVENT_CLICK;
        event.timestamp = GetTime();
        ui_event_push(event);
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        TextFieldProps field_storage;
        TextFieldProps *field;
        TextFieldState *state;
        int start;
        int end;
        int changed = 0;
        int selection_changed = 0;
        int codepoint;
        int modifier;

        if((node->kind != UI_WIDGET_TEXT_FIELD_NODE &&
            node->kind != UI_WIDGET_TEXT_AREA_NODE) ||
           node->state == NULL)
            continue;
        if((node->flags & UI_NODE_SCOPE_DISABLED) != 0) {
            int id = node->kind == UI_WIDGET_TEXT_FIELD_NODE
                ? node->data.text_field.focus_id : node->data.text_area.focus_id;
            /* Input sent to a disabled focused editor must not be replayed
               after the editor is enabled again. */
            if(id > 0 && IsUIFocusActive(id))
                while(GetCharPressed() != 0) {}
            if(ui_text_composition_cancel(node->state)) {
                ui_text_field_event(node, UI_EVENT_COMPOSITION_CHANGED,
                                    GetTime());
                ui_tree_invalid |= UI_INVALIDATE_PAINT;
            }
            continue;
        }
        if(node->kind == UI_WIDGET_TEXT_FIELD_NODE) {
            field = &node->data.text_field;
        } else {
            TextAreaProps *area = &node->data.text_area;

            area->bounds = node->bounds;
            memset(&field_storage, 0, sizeof(field_storage));
            field_storage.bounds = area->bounds;
            field_storage.text = area->text;
            field_storage.text_size = area->text_size;
            field_storage.cursor_position = area->cursor_position;
            field_storage.focused = area->focused;
            field_storage.max_codepoints = area->max_codepoints;
            field_storage.font = area->font;
            field_storage.focus_id = area->focus_id;
            field_storage.style = area->style;
            field_storage.filter = area->filter;
            field_storage.filter_user_data = area->filter_user_data;
            field_storage.read_only = area->read_only;
            field = &field_storage;
        }
        state = node->state;
        if(UIFocusFrameOpen() && field->focus_id > 0) {
            int focused = IsUIFocusActive(field->focus_id);

            if(state->focused != focused) {
                state->focused = focused;
                ui_text_field_event(node, focused ? UI_EVENT_FOCUS
                                                  : UI_EVENT_BLUR, GetTime());
            }
        }
        if(target >= 0) {
            int focused = target == i;

            if(state->focused != focused) {
                state->focused = focused;
                ui_text_field_event(node, focused ? UI_EVENT_FOCUS
                                                  : UI_EVENT_BLUR, GetTime());
            }
            if(focused) {
                int font = field->font > 0 ? field->font : GetFontSize();
                int padding = field->style.padding_x > 0
                    ? field->style.padding_x : Scale(10);
                double now = GetTime();
                KeyID click_key = field->focus_id > 0
                    ? (KeyID)field->focus_id : node->key;
                int click_dx = (int)mouse.x - ui_tree_text_last_click_x;
                int click_dy = (int)mouse.y - ui_tree_text_last_click_y;
                int double_click = ui_tree_text_last_click_key == click_key &&
                    now - ui_tree_text_last_click_time <= 0.45 &&
                    abs(click_dx) <= Scale(6) &&
                    abs(click_dy) <= Scale(6);

                if(double_click) {
                    state->anchor = 0;
                    state->cursor = field->text != NULL
                        ? (int)strlen(field->text) : 0;
                    state->dragging = 0;
                } else {
                    if(node->kind == UI_WIDGET_TEXT_AREA_NODE)
                        state->cursor = ui_text_area_cursor_at_point(
                            node->data.text_area, (int)mouse.x, (int)mouse.y);
                    else
                        state->cursor = ui_text_cursor_at_x(
                            field->text, font, (int)node->bounds.x + padding,
                            (int)mouse.x);
                    state->anchor = state->cursor;
                    state->dragging = 1;
                }
                ui_tree_text_last_click_key = click_key;
                ui_tree_text_last_click_x = (int)mouse.x;
                ui_tree_text_last_click_y = (int)mouse.y;
                ui_tree_text_last_click_time = now;
                ui_text_field_event(node, UI_EVENT_SELECTION_CHANGED,
                                    GetTime());
            } else {
                state->dragging = 0;
            }
        }
        if(field->focused != NULL)
            *field->focused = state->focused;
        int keyboard_captured = ui_popup_input_snapshot_keyboard_captures(ui_tree_input_snapshot(node));
        if((!state->focused || field->read_only || keyboard_captured) &&
           ui_text_composition_cancel(state)) {
            ui_text_field_event(node,UI_EVENT_COMPOSITION_CHANGED,GetTime());
            ui_tree_invalid |= UI_INVALIDATE_PAINT;
        }
        if(!state->focused || field->text == NULL || field->text_size == 0 ||
           keyboard_captured)
            continue;
        if(state->dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int font = field->font > 0 ? field->font : GetFontSize();
            int padding = field->style.padding_x > 0
                ? field->style.padding_x : Scale(10);
            int cursor;

            if(node->kind == UI_WIDGET_TEXT_AREA_NODE)
                cursor = ui_text_area_cursor_at_point(
                    node->data.text_area, (int)mouse.x, (int)mouse.y);
            else
                cursor = ui_text_cursor_at_x(
                    field->text, font, (int)node->bounds.x + padding,
                    (int)mouse.x);

            if(cursor != state->cursor) {
                state->cursor = cursor;
                selection_changed = 1;
            }
        }
        if(state->dragging && IsMouseButtonReleased(MOUSE_BUTTON_LEFT))
            state->dragging = 0;
        start = state->anchor < state->cursor ? state->anchor : state->cursor;
        end = state->anchor > state->cursor ? state->anchor : state->cursor;
        modifier = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL) ||
                   IsKeyDown(KEY_LEFT_SUPER) || IsKeyDown(KEY_RIGHT_SUPER);
        if(modifier && IsKeyPressed(KEY_A)) {
            state->anchor = 0;
            state->cursor = (int)strlen(field->text);
            selection_changed = 1;
            start = 0;
            end = state->cursor;
        }
        if(modifier && IsKeyPressed(KEY_C) && !field->secure) {
            if(end > start)
                (void)ui_text_copy_range(field->text, start, end);
            else
                (void)SetUIClipboardTextValue(field->text);
        }
        if(modifier && IsKeyPressed(KEY_X) && !field->secure && !field->read_only) {
            if(end > start) {
                if(ui_text_copy_range(field->text, start, end))
                    changed |= ui_text_delete_range(
                        field->text, field->text_size, &state->cursor,
                        start, end);
            } else if(field->text[0] != '\0') {
                (void)SetUIClipboardTextValue(field->text);
                field->text[0] = '\0';
                state->cursor = 0;
                changed = 1;
            }
            state->anchor = state->cursor;
            selection_changed = 1;
            start = end = state->cursor;
        }
        if(modifier && IsKeyPressed(KEY_V) && !field->read_only) {
            if(end > start)
                changed |= ui_text_delete_range(
                    field->text, field->text_size, &state->cursor, start, end);
            {
                int allow_newlines =
                    node->kind == UI_WIDGET_TEXT_AREA_NODE;
                TextEdit edit;

                memset(&edit, 0, sizeof(edit));
                edit.text = field->text;
                edit.text_size = field->text_size;
                edit.cursor_position = &state->cursor;
                edit.max_codepoints = field->max_codepoints;
                edit.filter = field->filter;
                edit.filter_user_data = field->filter_user_data;
                changed |= ui_text_paste_clipboard(edit, allow_newlines);
            }
            state->anchor = state->cursor;
            selection_changed = 1;
            start = end = state->cursor;
        }
        {
            int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            int multiline = node->kind == UI_WIDGET_TEXT_AREA_NODE;
            int navigation_key = ui_text_navigation_key(multiline);
            int font = field->font > 0 ? field->font : GetFontSize();
            TextNavigationInput navigation = {
                .text = field->text,
                .area = multiline ? &node->data.text_area : NULL,
                .font = font,
                .key = navigation_key,
                .shift = shift,
                .modifier = modifier,
                .secure = field->secure
            };

            if(ui_text_navigate(navigation, &state->anchor, &state->cursor)) {
                selection_changed = 1;
                start = state->anchor < state->cursor
                    ? state->anchor : state->cursor;
                end = state->anchor > state->cursor
                    ? state->anchor : state->cursor;
            }
        }
        codepoint = GetCharPressed();
        while(codepoint > 0) {
            if(field->read_only) { codepoint = GetCharPressed(); continue; }
            if(end > start) {
                changed |= ui_text_delete_range(field->text, field->text_size,
                                                 &state->cursor, start, end);
                state->anchor = state->cursor;
                start = end = state->cursor;
            }
            if((field->filter == NULL ||
                field->filter(codepoint, field->filter_user_data)) &&
               ui_text_insert_codepoint(field->text, field->text_size,
                                        &state->cursor, codepoint,
                                        field->max_codepoints)) {
                state->anchor = state->cursor;
                changed = 1;
                selection_changed = 1;
            }
            codepoint = GetCharPressed();
        }
        {
            TextEdit edit = {
                .text = field->text,
                .text_size = field->text_size,
                .cursor_position = &state->cursor,
                .max_codepoints = field->max_codepoints,
                .filter = field->filter,
                .filter_user_data = field->filter_user_data
            };
            TextCompositionResult composition = ui_text_composition_apply(
                edit, &state->anchor, state, state->focused,
                field->read_only, node->kind == UI_WIDGET_TEXT_AREA_NODE);

            changed |= composition.text_changed;
            selection_changed |= composition.selection_changed;
            if(composition.presentation_changed) {
                ui_text_field_event(node, UI_EVENT_COMPOSITION_CHANGED,
                                    GetTime());
                ui_tree_invalid |= UI_INVALIDATE_PAINT;
            }
        }
        if(backspace_count > 0 && !field->read_only) {
            while(backspace_count-- > 0)
                changed |= ui_text_delete_key(
                    field->text, field->text_size, &state->anchor,
                    &state->cursor, KEY_BACKSPACE, modifier, field->secure);
            selection_changed = changed;
        } else if(delete_count > 0 && !field->read_only) {
            while(delete_count-- > 0)
                changed |= ui_text_delete_key(
                    field->text, field->text_size, &state->anchor,
                    &state->cursor, KEY_DELETE, modifier, field->secure);
            selection_changed = changed;
        }
        if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER)) {
            if(node->kind == UI_WIDGET_TEXT_AREA_NODE && !field->read_only) {
                if(end > start)
                    changed |= ui_text_delete_range(
                        field->text, field->text_size, &state->cursor,
                        start, end);
                if(ui_text_insert_ascii(field->text, field->text_size,
                                        &state->cursor, '\n',
                                        field->max_codepoints))
                    changed = 1;
                state->anchor = state->cursor;
                selection_changed = 1;
            } else {
                if(field->commit_pressed != NULL)
                    *field->commit_pressed = 1;
                ui_text_field_event(node, UI_EVENT_TEXT_COMMIT, GetTime());
            }
        }
        if(IsKeyPressed(KEY_ESCAPE)) {
            state->focused = 0;
            state->dragging = 0;
            if(field->focused != NULL)
                *field->focused = 0;
            ui_text_field_event(node, UI_EVENT_BLUR, GetTime());
        }
        if(field->cursor_position != NULL)
            *field->cursor_position = state->cursor;
        if(node->kind == UI_WIDGET_TEXT_AREA_NODE &&
           (changed || selection_changed))
            ui_text_area_reveal_cursor(node->data.text_area, state->cursor);
        if(changed)
            ui_text_field_event(node, UI_EVENT_TEXT_CHANGED, GetTime());
        if(selection_changed)
            ui_text_field_event(node, UI_EVENT_SELECTION_CHANGED, GetTime());
        if(changed || selection_changed)
            ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
}

void
UpdateTree(void)
{
    UIWidgetNode *root;

    if(ui_committed_node_count <= 0)
        return;
    root = &ui_committed_nodes[0];
    if(root->bounds.width != ui_view_width ||
       root->bounds.height != ui_view_height) {
        root->bounds.width = (float)ui_view_width;
        root->bounds.height = (float)ui_view_height;
        ui_tree_invalid |= UI_INVALIDATE_LAYOUT | UI_INVALIDATE_PAINT;
    }
}

void
ui_paint_text_box(const char *value, Rectangle bounds, int font, Color color,
                  int wrap, int align, int vertical_align, int font_token, int letter_spacing)
{
    int previous_font = ui_active_font_token();
    int previous_spacing = ui_set_text_letter_spacing(Scale(letter_spacing));
    int y = (int)bounds.y;
    int text_width;
    int text_height;
    int needs_clip;

    PopUIFont(font_token);
    text_width = TextWidth(value, font);
    text_height = TextHeight(value, font);
    needs_clip = wrap == TextWrapAuto ||
                 text_width > (int)bounds.width ||
                 text_height > (int)bounds.height;
    if(needs_clip) {
        BeginUIClip((int)bounds.x, (int)bounds.y,
                    (int)bounds.width, (int)bounds.height);
    }
    if(wrap == TextWrapAuto) {
        ParagraphSpec paragraph = {
            .text = value, .width = (int)bounds.width, .font = font,
            .line_gap = Scale(2), .color = color
        };
        int height = ui_paragraph_height(paragraph);
        y += (int)TextAlignmentOffset((int)bounds.height, height, vertical_align);
        ui_draw_paragraph_aligned(paragraph, (int)bounds.x, &y, align);
    } else {
        int x = (int)bounds.x;
        x += (int)TextAlignmentOffset((int)bounds.width, text_width, align);
        if(vertical_align == TextAlignCenter)
            y = TextBaselineY(value, (int)bounds.y, (int)bounds.height, font);
        else
            y += (int)TextAlignmentOffset((int)bounds.height, text_height, vertical_align);
        RenderText(value, x, y, font, color);
    }
    if(needs_clip)
        EndUIClip();
    PopUIFont(previous_font);
    ui_set_text_letter_spacing(previous_spacing);
}

static void
ui_tree_inherit_foreground(int parent, Color foreground, bool disabled)
{
    for(int child = ui_committed_nodes[parent].first_child; child >= 0;
        child = ui_committed_nodes[child].next_sibling) {
        UIWidgetNode *node = &ui_committed_nodes[child];
        /* Nested buttons establish their own style when they are painted. */
        if(ui_tree_button_like_kind(node->kind))
            continue;
        if(node->kind == UI_WIDGET_TEXT_NODE &&
           (node->flags & UI_NODE_INHERIT_FOREGROUND) != 0) {
            TextAppearance appearance = ResolveTextStyle(0, 0, 16, 0,
                ColorToInt(foreground), 0, true, false,
                (node->flags & UI_NODE_TEXT_DISABLED) != 0, disabled, 0);
            node->data.primitive.color = GetColor(Opacity(appearance.color,
                node->data.primitive.style.opacity));
        }
        if(node->first_child >= 0)
            ui_tree_inherit_foreground(child, foreground, disabled);
    }
}

static void
DrawTree(void)
{
    int i;
    int window_ready = IsWindowReady();

    if((ui_tree_invalid & UI_INVALIDATE_PAINT) == 0)
        return;
    /* Requests made while painting belong to the next animation frame. */
    ui_tree_invalid &= ~UI_INVALIDATE_PAINT;
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        UIClipState parent_clip = {0};
        UIBlendState parent_blend = {{0}};

        if((node->flags & UI_NODE_PAINTED_IMMEDIATE) != 0)
            continue;

        /* Slider/toggle/checkbox helpers also route their legacy input. Keep
         * that state update alive in headless and software parity runs while
         * suppressing draw-only nodes until a window exists. */
        if(!window_ready && node->kind != UI_WIDGET_SLIDER_NODE &&
           node->kind != UI_WIDGET_DRAG_NODE &&
           node->kind != UI_WIDGET_TOGGLE_NODE &&
           node->kind != UI_WIDGET_CHECKBOX_NODE)
            continue;

        if(window_ready && node->paint_capture != 0) {
            UIPaintCapture *capture = &ui_tree_paint_captures[node->paint_capture-1];
            parent_clip = ui_clip_save();
            parent_blend = ui_blend_save();
            BeginTextureMode(capture->target);
            ui_blend_restore(capture->blend);
            rlSetMatrixProjection(capture->projection);
            rlSetMatrixModelview(capture->modelview);
            ResetUIClip();
            if(capture->has_clip)
                BeginUIClip((int)capture->clip.x,(int)capture->clip.y,
                            (int)capture->clip.width,(int)capture->clip.height);
        }
        BeginDisabled((node->flags & UI_NODE_SCOPE_DISABLED) != 0);
        if(node->has_input_clip) {
            PushUIInputClip(node->input_clip);
            if(window_ready) BeginUIClip((int)node->input_clip.x,(int)node->input_clip.y,(int)node->input_clip.width,(int)node->input_clip.height);
        }
        switch(node->kind) {
        case UI_WIDGET_TEXT_INPUT_PAINT_NODE:
            ui_paint_text_input(node->bounds, node->owned_text,
                                node->data.text_input_paint);
            break;
        case UI_WIDGET_DRAG_NODE: {
            DragProps drag = node->data.drag.props;
            drag.bounds = node->bounds;
            drag.label = node->owned_text;
            drag.format = node->owned_text != NULL
                ? node->owned_text + node->data.drag.format_offset : NULL;
            if(drag.kind == NumericInt) {
                ui_paint_drag_int((UIIntDragProps){
                    drag.bounds, drag.id, drag.label, drag.int_values,
                    drag.value_count, drag.speed, (int)drag.min, (int)drag.max,
                    drag.format, drag.disabled});
            } else {
                ui_paint_drag_float((UIFloatDragProps){
                    drag.bounds, drag.id, drag.label, drag.float_values,
                    drag.value_count, drag.speed, (float)drag.min,
                    (float)drag.max, drag.format, drag.disabled});
            }
            break;
        }
        case UI_WIDGET_SLIDER_NODE: {
            SliderProps slider = node->data.slider.props;
            slider.bounds = node->bounds;
            slider.label = node->owned_text;
            slider.format = node->owned_text != NULL
                ? node->owned_text + node->data.slider.format_offset
                : NULL;
            if(slider.angle) {
                ui_paint_slider_angle((UIAngleSliderProps){
                    slider.bounds, slider.id, slider.label, slider.float_value,
                    (float)slider.min, (float)slider.max, slider.format,
                    slider.disabled});
            } else if(slider.kind == NumericInt) {
                ui_paint_slider_int((UIIntSliderProps){
                    slider.bounds, slider.id, slider.label, slider.int_values,
                    slider.value_count, (int)slider.min, (int)slider.max,
                    slider.format, slider.disabled}, slider.vertical);
            } else {
                ui_paint_slider_float((UIFloatSliderProps){
                    slider.bounds, slider.id, slider.label, slider.float_values,
                    slider.value_count, (float)slider.min, (float)slider.max,
                    slider.format, slider.disabled}, slider.vertical);
            }
            break;
        }
        case UI_WIDGET_BACKGROUND_NODE:
            DrawRectangleRec(node->bounds, node->data.primitive.color);
            break;
        case UI_WIDGET_TEXT_NODE:
            if((node->flags & UI_NODE_PAINTED_IMMEDIATE) != 0)
                break;
            if(node->data.primitive.heading_level > 0)
                ui_tree_heading_semantic(
                    node->owned_text != NULL ? node->owned_text : "",
                    node->data.primitive.heading_level);
            ui_paint_text_box(
                node->owned_text != NULL ? node->owned_text : "", node->bounds,
                node->data.primitive.font, node->data.primitive.color,
                node->data.primitive.wrap, node->data.primitive.align,
                node->data.primitive.vertical_align,
                node->data.primitive.font_token, node->data.primitive.letter_spacing);
            break;
        case UI_WIDGET_RECT_NODE:
            if(node->data.primitive.styled) {
                ui_paint_surface(node->bounds, node->data.primitive.style);
                break;
            }
            DrawRectangleRec(node->bounds, node->data.primitive.color);
            if(node->data.primitive.border.a != 0)
                DrawRectangleLinesEx(node->bounds, 1.0f,
                                     node->data.primitive.border);
            break;
        case UI_WIDGET_CIRCLE_NODE:
            DrawCircle((int)(node->bounds.x + node->bounds.width / 2.0f),
                       (int)(node->bounds.y + node->bounds.height / 2.0f),
                       node->bounds.width / 2.0f,
                       node->data.primitive.color);
            break;
        case UI_WIDGET_RING_NODE:
            DrawRing((Vector2){node->bounds.x + node->bounds.width / 2.0f,
                               node->bounds.y + node->bounds.height / 2.0f},
                     (float)node->data.primitive.x1,
                     node->bounds.width / 2.0f,
                     0.0f, 360.0f, 0,
                     node->data.primitive.color);
            break;
        case UI_WIDGET_LINE_NODE:
            DrawLine((int)node->bounds.x, (int)node->bounds.y,
                     node->data.primitive.x2, node->data.primitive.y2,
                     node->data.primitive.color);
            break;
        case UI_WIDGET_TRIANGLE_NODE:
            DrawTriangle((Vector2){(float)node->data.primitive.x1,
                                   (float)node->data.primitive.y1},
                         (Vector2){(float)node->data.primitive.x2,
                                   (float)node->data.primitive.y2},
                         (Vector2){(float)node->data.primitive.x3,
                                   (float)node->data.primitive.y3},
                         node->data.primitive.color);
            break;
        case UI_WIDGET_BUTTON_NODE:
        case UI_WIDGET_CARD_NODE: {
            ButtonSpec spec = node->data.button;
            int hovered;
            int pressed;

            if((node->flags & UI_NODE_PAINTED_IMMEDIATE) != 0)
                break;
            spec.props.bounds = node->bounds;
            spec.props.label = node->first_child >= 0 ? "" :
                (node->owned_text != NULL ? node->owned_text : "");
            hovered = (node->flags & UI_NODE_HOVERED) != 0;
            pressed = (node->flags & UI_NODE_PRESSED) != 0;
            Color foreground = ui_paint_button(spec, hovered, pressed);
            ui_tree_inherit_foreground(i, foreground, node->data.button.props.disabled);
            break;
        }
        case UI_WIDGET_TEXT_AREA_NODE: {
            TextFieldState *state = node->state;
            TextAreaProps area = node->data.text_area;
            int cursor = state != NULL ? state->cursor : 0;
            int anchor = state != NULL ? state->anchor : cursor;
            int previous_font = ui_active_font_token();
            TextCompositionView composition = {0};
            const char *preedit = NULL;
            int preedit_cursor = 0;
            int preedit_selection_length = 0;
            int composing = state != NULL && ui_text_composition_get(
                state, &preedit, &preedit_cursor,
                &preedit_selection_length);

            area.bounds = node->bounds;
            PopUIFont(node->font_token);
            if(composing && ui_text_composition_view(
                    area.text,
                    anchor < cursor ? anchor : cursor,
                    anchor > cursor ? anchor : cursor,
                    preedit, preedit_cursor,
                    preedit_selection_length, &composition)) {
                area.text = composition.text;
                area.content_version = 0;
                ui_paint_text_area_composition(
                    area, composition.cursor,
                    state->focused, composition.selection_start,
                    composition.selection_end, composition.composition_start,
                    composition.composition_end);
                ui_text_composition_view_free(&composition);
            } else {
                ui_paint_text_area(area, cursor,
                    state != NULL ? state->focused : 0,
                    anchor < cursor ? anchor : cursor,
                    anchor > cursor ? anchor : cursor);
            }
            PopUIFont(previous_font);
            break;
        }
        case UI_WIDGET_TEXT_FIELD_NODE: {
            TextFieldProps field;
            TextFieldState *state = node->state;
            const char *display;
            char *masked = NULL;
            TextCompositionView composition = {0};
            int cursor;
            int selection_start;
            int selection_end;
            int composition_start = 0;
            int composition_end = 0;
            const char *preedit = NULL;
            int preedit_cursor = 0;
            int preedit_selection_length = 0;

            field = node->data.text_field;
            display = field.text != NULL ? field.text : "";
            cursor = state != NULL ? state->cursor : 0;
            selection_start = state != NULL && state->anchor < cursor
                ? state->anchor : cursor;
            selection_end = state != NULL && state->anchor > cursor
                ? state->anchor : cursor;

            if(state != NULL && !field.secure &&
               ui_text_composition_get(state, &preedit, &preedit_cursor,
                                       &preedit_selection_length) &&
               ui_text_composition_view(
                   display, selection_start, selection_end,
                   preedit, preedit_cursor, preedit_selection_length,
                   &composition)) {
                display = composition.text;
                cursor = composition.cursor;
                selection_start = composition.selection_start;
                selection_end = composition.selection_end;
                composition_start = composition.composition_start;
                composition_end = composition.composition_end;
            }

            if(field.secure) {
                size_t length = strlen(display);

                char *secure_mask = malloc(length + 1);
                if(secure_mask != NULL) {
                    masked = secure_mask;
                    memset(secure_mask, '*', length);
                    secure_mask[length] = '\0';
                    display = secure_mask;
                }
            }
            {
                UIWidgetTextInputPaint paint = {
                    .style = field.style,
                    .cursor = cursor,
                    .focused = state != NULL ? state->focused : 0,
                    .editable = !field.read_only,
                    .caret = state != NULL && state->focused &&
                             !field.read_only,
                    .font = field.font,
                    .font_token = node->font_token,
                    .selection_start = selection_start,
                    .selection_end = selection_end,
                    .composition_start = composition_start,
                    .composition_end = composition_end
                };
                ui_paint_text_input(node->bounds, display, paint);
            }
            free(masked);
            ui_text_composition_view_free(&composition);
            break;
        }
        case UI_WIDGET_TOGGLE_NODE:
        case UI_WIDGET_CHECKBOX_NODE: {
            int *value = node->kind == UI_WIDGET_TOGGLE_NODE
                ? node->data.toggle.value : node->data.checkbox.value;
            int changed;

            if(node->kind == UI_WIDGET_TOGGLE_NODE) {
                changed = ToggleSwitch(
                    (int)node->bounds.x, (int)node->bounds.y,
                    (int)node->bounds.width, (int)node->bounds.height,
                    value, node->data.toggle.off_label,
                    node->data.toggle.on_label,
                    IsUIFocusActive(node->id) &&
                    !ui_popup_input_snapshot_keyboard_captures(
                        ui_tree_input_snapshot(node)));
            } else {
                changed = RenderCheckboxToggle(
                    (int)node->bounds.x, (int)node->bounds.y,
                    node->data.checkbox.label, value);
                if(IsUIFocusActive(node->id) &&
                   !ui_popup_input_snapshot_keyboard_captures(
                       ui_tree_input_snapshot(node)) && IsWindowReady())
                    RenderFocus(node->bounds);
            }
            if(changed && value != NULL) {
                UIEvent event = {0};
                event.key = node->key;
                event.kind = UI_EVENT_VALUE_CHANGED;
                event.timestamp = GetTime();
                event.data.value = *value;
                ui_event_push(event);
            }
            break;
        }
        default:
            break;
        }
        if(node->has_input_clip) {
            if(window_ready) EndUIClip();
            PopUIInputClip();
        }
        EndDisabled();
        if(window_ready && node->paint_capture != 0) {
            EndTextureMode();
            ui_blend_restore(parent_blend);
            ui_clip_restore(parent_clip);
        }
    }
}

void
Overlays(void)
{
    if(!IsWindowReady())
        return;
    RenderFrameOverlays();
}

const UIWidgetNode *
GetTreeNodes(int *count)
{
    if(count != NULL)
        *count = ui_committed_node_count > 0 ? ui_committed_node_count
                                             : ui_tree_node_count;
    return ui_committed_node_count > 0 ? ui_committed_nodes : ui_tree_nodes;
}

const UIWidgetNode *
GetNode(NodeId id)
{
    if(ui_committed_node_count > 0) {
        if(id < 0 || id >= ui_committed_node_count)
            return NULL;
        return &ui_committed_nodes[id];
    }
    return ui_tree_node(id);
}

NodeId
HitTestNode(Vector2 point)
{
    int i;
    UIWidgetNode *nodes = ui_committed_node_count > 0
        ? ui_committed_nodes : ui_tree_nodes;
    int count = ui_committed_node_count > 0
        ? ui_committed_node_count : ui_tree_node_count;

    for(i = count - 1; i >= 0; i--) {
        if((nodes[i].flags & UI_NODE_SCOPE_DISABLED) != 0) continue;
        if(ui_tree_input_blocked(&nodes[i],point)) continue;
        if(nodes[i].bounds.width <= 0 || nodes[i].bounds.height <= 0)
            continue;
        if(nodes[i].has_input_clip && !CheckCollisionPointRec(point,nodes[i].input_clip)) continue;
        if(CheckCollisionPointRec(point, nodes[i].bounds))
            return i;
    }
    return -1;
}

static const char *
ui_accessibility_role(UIWidgetKind kind)
{
    switch(kind) {
    case UI_WIDGET_SCREEN_NODE: return "main";
    case UI_WIDGET_TEXT_NODE:
    case UI_WIDGET_PARAGRAPH_NODE:
    case UI_WIDGET_READONLY_TEXT_BOX_NODE: return "text";
    case UI_WIDGET_BUTTON_NODE: return "button";
    case UI_WIDGET_CARD_NODE: return "group";
    case UI_WIDGET_TEXT_INPUT_PAINT_NODE:
    case UI_WIDGET_TEXT_FIELD_NODE:
    case UI_WIDGET_TEXT_AREA_NODE: return "textbox";
    case UI_WIDGET_DROPDOWN_NODE: return "combobox";
    case UI_WIDGET_SLIDER_NODE: return "slider";
    case UI_WIDGET_TOGGLE_NODE:
    case UI_WIDGET_CHECKBOX_NODE: return "checkbox";
    case UI_WIDGET_TAB_BAR_NODE: return "tablist";
    case UI_WIDGET_COLUMN_NODE:
    case UI_WIDGET_ROW_NODE:
    case UI_WIDGET_STACK_NODE:
    case UI_WIDGET_GRID_NODE:
    case UI_WIDGET_ROUTER_NODE:
    case UI_WIDGET_GROUP_NODE: return "group";
    case UI_WIDGET_IMAGE_NODE: return "img";
    default: return NULL;
    }
}

static const char *
ui_tree_first_text(const UIWidgetNode *nodes, int count, int parent)
{
    int child;

    if(parent < 0 || parent >= count)
        return NULL;
    for(child = nodes[parent].first_child; child >= 0 && child < count;
        child = nodes[child].next_sibling) {
        const char *nested;

        if(nodes[child].kind == UI_WIDGET_TEXT_NODE &&
           nodes[child].owned_text != NULL &&
           nodes[child].owned_text[0] != '\0')
            return nodes[child].owned_text;
        nested = ui_tree_first_text(nodes, count, child);
        if(nested != NULL)
            return nested;
    }
    return NULL;
}

int
GetAccessibilitySnapshot(UIAccessibilityNode *nodes, int capacity)
{
    int count = 0;
    int i;

    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        const char *role = ui_accessibility_role(node->kind);
        const char *label = node->owned_text;

        if(node->kind == UI_WIDGET_TEXT_NODE && node->parent >= 0 &&
           ui_tree_button_like_kind(ui_committed_nodes[node->parent].kind))
            continue;
        if(node->kind == UI_WIDGET_CARD_NODE && node->data.button.props.id > 0)
            role = "button";
        if(role == NULL)
            continue;
        if(ui_tree_button_like_kind(node->kind)) {
            if(label == NULL || label[0] == '\0')
                label = ui_tree_first_text(ui_committed_nodes,
                                           ui_committed_node_count, i);
            if(label == NULL)
                label = node->data.button.props.label;
        }
        else if(label == NULL && node->kind == UI_WIDGET_CHECKBOX_NODE)
            label = node->data.checkbox.label;
        if(nodes != NULL && count < capacity) {
            memset(&nodes[count], 0, sizeof(nodes[count]));
            nodes[count].bounds = node->bounds;
            nodes[count].role = role;
            nodes[count].label = label != NULL ? label : "";
            nodes[count].focused = node->kind == UI_WIDGET_TEXT_FIELD_NODE &&
                node->state != NULL &&
                ((TextFieldState *)node->state)->focused;
            nodes[count].disabled = ui_tree_button_like_kind(node->kind) &&
                !CanActivate(node->data.button.props.disabled, node->data.button.props.loading);
            nodes[count].checked = node->kind == UI_WIDGET_CHECKBOX_NODE &&
                node->data.checkbox.value != NULL &&
                *node->data.checkbox.value != 0;
        }
        count++;
    }
    return count;
}

void
SetAccessibilitySink(UIAccessibilitySink sink, void *userdata)
{
    ui_accessibility_sink = sink;
    ui_accessibility_sink_userdata = userdata;
}

int
GetNodeHeight(UIWidgetNode node)
{
    const UIWidgetOps *ops;

    if(node.kind < 0 ||
       node.kind >= (int)(sizeof(ui_widget_ops) / sizeof(ui_widget_ops[0])))
        return ui_measure_bounds_height(node);
    ops = &ui_widget_ops[node.kind];
    if(ops->measure_height == NULL)
        return ui_measure_bounds_height(node);
    return ops->measure_height(node);
}

int
GetNodeHeightById(int id)
{
    int i;

    for(i = ui_tree_node_count - 1; i >= 0; i--) {
        if(ui_tree_nodes[i].id == id)
            return GetNodeHeight(ui_tree_nodes[i]);
    }
    return 0;
}

UIWidgetNode
NodeParagraph(ParagraphSpec paragraph, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_PARAGRAPH_NODE,
                   (Rectangle){x, y, paragraph.width, 0});
    node.data.paragraph = paragraph;
    return node;
}

UIWidgetNode
NodeReadonlyTextBox(ReadonlyTextBoxProps box)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_READONLY_TEXT_BOX_NODE, box.bounds);
    node.data.readonly_text_box = box;
    return node;
}

UIWidgetNode
NodeNavigationBar(NavigationBarProps nav)
{
    UIWidgetNode node;
    int height;

    height = nav.height > 0 ? nav.height : 0;
    node = ui_node(0, UI_WIDGET_NAVIGATION_BAR_NODE,
                   (Rectangle){0, 0, nav.view_width, height});
    return node;
}

UIWidgetNode
NodeTabBar(TabBarProps bar)
{
    return ui_node(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds);
}

UIWidgetNode
NodeParagraphModal(ParagraphModalMeasureProps measure)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_PARAGRAPH_MODAL_NODE,
                   (Rectangle){0, 0, measure.width, 0});
    node.data.paragraph_modal = measure;
    return node;
}

UIWidgetNode
NodeTitleBar(int height)
{
    return ui_node(0, UI_WIDGET_TITLE_BAR_NODE,
                   (Rectangle){0, 0, ui_view_width, height});
}

void
RenderImage(ImageProps image)
{
    Texture2D texture;
    Color fallback;

    ui_tree_add(0, UI_WIDGET_IMAGE_NODE, image.bounds, image.asset_path);
    texture = LoadImageTexture(image.asset_path);
    if(texture.id == 0) {
        fallback = image.style.enabled && image.style.background.a > 0
                     ? image.style.background
                     : GetThemeSurface();
        DrawRectangleRec(image.bounds, fallback);
        DrawRectangleLinesEx(image.bounds, 1.0f, GetThemeButtonHover());
        RenderText("Missing image", (int)image.bounds.x + Scale(8),
                   (int)image.bounds.y + Scale(8), Text12,
                   GetThemeIcon());
        return;
    }
    ImageTexture(texture, image);
}

void
Background(Color color)
{
    NodeId node = ui_tree_add(0, UI_WIDGET_BACKGROUND_NODE,
                                (Rectangle){0, 0, ui_view_width,
                                            ui_view_height}, NULL);

    if(node >= 0)
        ui_tree_nodes[node].data.primitive.color = color;
    if(ui_tree_building) {
        /* A backdrop declared in a retained screen must paint now, in
         * declaration order: widgets that render during this declaration
         * already draw before EndTree, so deferring the fill to the tree
         * paint pass would cover them. Keep the node for layout and input
         * but stop the tree pass from painting it twice. */
        ui_tree_mark_painted_immediate(node);
    }
    DrawRectangleRec((Rectangle){0, 0, GetUIViewWidth(), GetUIViewHeight()},
                     color);
}

void
Text(TextProps props)
{
    int previous_typeface = PushUIFont(props.typeface);
    int previous_spacing;
    const char *value = props.text != NULL ? props.text : "";
    int font;
    int inherited_font = 0;
    bool inherit_foreground = props.color.a == 0 &&
        (props.style.fields & StyleForeground) == 0;
    Color inherited_color = {0};
    bool inherited_color_set = false;
    int inherited_disabled = 0;
    int bounded = props.bounds.width > 0;
    Rectangle bounds = props.bounds;
    float measured_width = 0;
    float measured_height = 0;
    NodeId node;

    if(ui_tree_building) {
        for(int i = ui_tree_stack_depth - 1; i >= 0; i--) {
            UIWidgetNode *parent = ui_tree_node(ui_tree_stack[i]);

            if(parent != NULL && ui_tree_button_like_kind(parent->kind)) {
                ButtonSpec *button = &parent->data.button;

                inherited_font = button->props.font;
                if(inherited_font <= 0) {
                    Style style = ResolveButtonStyle(button->props, button->props.state);
                    inherited_font = ResolveFont(button->props.font, Scale(style.font_size), GetFontSize());
                }
                inherited_color = button->paint.foreground;
                inherited_color_set = true;
                inherited_disabled = button->props.disabled;
                break;
            }
        }
    }
    props.disabled = props.disabled || (UIContentDisabled() && !inherited_disabled);
    Style style = MergeStyle((Style){.foreground = props.color, .opacity = 1}, props.style);
    if((props.style.fields & StyleFontSize) != 0)
        props.font = Scale((int)style.font_size);
    TextAppearance appearance = ResolveTextStyle(props.font, inherited_font, GetFontSize(),
        ColorToInt(style.foreground), ColorToInt(inherited_color), ColorToInt(GetThemeText()),
        inherited_color_set, !inherit_foreground, props.disabled, inherited_disabled, props.letter_spacing);
    font = appearance.font;
    props.color = GetColor(Opacity(appearance.color, style.opacity));
    props.letter_spacing = appearance.letter_spacing;
    previous_spacing = ui_set_text_letter_spacing(Scale(props.letter_spacing));
    props.wrap = (TextWrap)TextWrapPolicy(bounds.width, props.wrap);
    if(!bounded)
        measured_width = (float)TextWidth(value, font);
    bounds.width = TextExtent(bounds.width, measured_width);
    if(bounds.height <= 0) {
        if(bounded && props.wrap == TextWrapAuto) {
            ParagraphSpec paragraph = {
                .text = value, .width = (int)bounds.width, .font = font,
                .line_gap = Scale(2), .color = props.color
            };
            measured_height = (float)ui_paragraph_height(paragraph);
        } else {
            measured_height = (float)TextHeight(value, font);
        }
    }
    bounds.height = TextExtent(bounds.height, measured_height);
    node = ui_tree_add(0, UI_WIDGET_TEXT_NODE, bounds, NULL);
    if(node >= 0) {
        ui_tree_nodes[node].owned_text = ui_tree_strdup(value);
        ui_tree_nodes[node].data.primitive.font = font;
        ui_tree_nodes[node].data.primitive.font_token = ui_active_font_token();
        ui_tree_nodes[node].data.primitive.letter_spacing = props.letter_spacing;
        ui_tree_nodes[node].data.primitive.color = props.color;
        ui_tree_nodes[node].data.primitive.style = style;
        ui_tree_nodes[node].data.primitive.wrap = props.wrap;
        ui_tree_nodes[node].data.primitive.align = props.align;
        ui_tree_nodes[node].data.primitive.vertical_align = props.vertical_align;
        if(inherit_foreground && inherited_color_set)
            ui_tree_nodes[node].flags |= UI_NODE_INHERIT_FOREGROUND;
        if(props.disabled)
            ui_tree_nodes[node].flags |= UI_NODE_TEXT_DISABLED;
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    if(ui_tree_building && IsWindowReady() &&
       !ui_tree_node_uses_retained_layout(node)) {
        ui_paint_text_box(value, bounds, font, props.color, props.wrap,
                          props.align, props.vertical_align,
                          ui_active_font_token(), props.letter_spacing);
        ui_tree_mark_painted_immediate(node);
    } else if(!ui_tree_building) {
        ui_paint_text_box(value, bounds, font, props.color, props.wrap,
                          props.align, props.vertical_align,
                          ui_active_font_token(), props.letter_spacing);
    }
    ui_set_text_letter_spacing(previous_spacing);
    PopUIFont(previous_typeface);
}

void
ui_tree_heading(const char *text, Rectangle bounds, int font, Color color, int level)
{
    if(!ui_tree_building)
        ui_tree_heading_semantic(text, level);
    Text((TextProps){.bounds=bounds,.text=text,.font=font,.color=color,
                     .wrap=TextWrapNone});
    if(ui_tree_building && ui_tree_node_count > 0)
        ui_tree_nodes[ui_tree_node_count - 1].data.primitive.heading_level = level;
}

void
Paragraph(ParagraphSpec paragraph, int x, int *y)
{
    UIWidgetNode node;
    NodeId id;
    int start_y = y != NULL ? *y : 0;

    id = ui_tree_add(0, UI_WIDGET_PARAGRAPH_NODE,
                     (Rectangle){x, start_y, paragraph.width, 0}, NULL);
    node = NodeParagraph(paragraph, x, start_y);
    ui_tree_store_node(id, node);
    ui_draw_paragraph(paragraph, x, y);
}

void
Surface(Rectangle bounds, Style style)
{
    Style defaults = {0};
    defaults.fields = StyleBackground | StyleRadius | StyleBorderWidth | StyleOpacity | StyleMaterial;
    defaults.material = MaterialFlat;
    defaults.background = GetThemeSurface();
    defaults.radius = GetThemeMetrics().radius_medium;
    defaults.border_width = GetThemeMetrics().border_width;
    defaults.opacity = 1.0f;
    style = MergeStyle(defaults, style);
    NodeId node = ui_tree_add(0, UI_WIDGET_RECT_NODE, bounds, NULL);
    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.styled = 1;
        ui_tree_nodes[node].data.primitive.style = style;
    }
    if(!ui_tree_building)
        ui_paint_surface(bounds, style);
}

static ButtonProps
ui_card_button_props(CardProps card)
{
    ThemeMetrics metrics = GetThemeMetrics();
    return CardButtonProps(card, metrics.radius_large, metrics.border_width,
                           metrics.control_padding_large,
                           metrics.control_padding_medium, GetThemeSurface(),
                           GetThemeButtonHover());
}

static void
rect_shape_impl(int x, int y, int w, int h, Color fill, Color border)
{
    NodeId node = ui_tree_add(0, UI_WIDGET_RECT_NODE,
                                (Rectangle){x, y, w, h}, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.color = fill;
        ui_tree_nodes[node].data.primitive.border = border;
    }
    if(ui_tree_building)
        return;
    DrawRectangleRec((Rectangle){x, y, w, h}, fill);
    if(border.a != 0)
        DrawRectangleLinesEx((Rectangle){x, y, w, h}, 1, border);
}

#ifdef KRYON_BACKEND_LIBDRAW
void
kry_ui_rect_shape(int x, int y, int w, int h, Color fill, Color border)
#else
void
Rect(int x, int y, int w, int h, Color fill, Color border)
#endif
{
    rect_shape_impl(x, y, w, h, fill, border);
}

void
Box(Rectangle bounds, Color fill, Color border)
{
    rect_shape_impl((int)bounds.x, (int)bounds.y,
                    (int)bounds.width, (int)bounds.height,
                    fill, border);
}

void
Circle(int center_x, int center_y, int radius, Color color)
{
    int diameter = radius * 2;
    NodeId node = ui_tree_add(0, UI_WIDGET_CIRCLE_NODE,
                              (Rectangle){center_x - radius,
                                          center_y - radius,
                                          diameter, diameter},
                              NULL);

    if(node >= 0)
        ui_tree_nodes[node].data.primitive.color = color;
    if(ui_tree_building)
        return;
    DrawCircle(center_x, center_y, (float)radius, color);
}

void
Ring(int center_x, int center_y, int inner_radius, int outer_radius,
     Color color)
{
    int diameter = outer_radius * 2;
    NodeId node = ui_tree_add(0, UI_WIDGET_RING_NODE,
                              (Rectangle){center_x - outer_radius,
                                          center_y - outer_radius,
                                          diameter, diameter},
                              NULL);

    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.x1 = inner_radius;
        ui_tree_nodes[node].data.primitive.color = color;
    }
    if(ui_tree_building)
        return;
    DrawRing((Vector2){(float)center_x, (float)center_y},
             (float)inner_radius, (float)outer_radius,
             0.0f, 360.0f, 0, color);
}

void
Line(int x1, int y1, int x2, int y2, Color color)
{
    int x = x1 < x2 ? x1 : x2;
    int y = y1 < y2 ? y1 : y2;
    int w = abs(x2 - x1);
    int h = abs(y2 - y1);

    NodeId node = ui_tree_add(0, UI_WIDGET_LINE_NODE,
                                (Rectangle){x, y, w, h}, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.x1 = x1;
        ui_tree_nodes[node].data.primitive.y1 = y1;
        ui_tree_nodes[node].data.primitive.x2 = x2;
        ui_tree_nodes[node].data.primitive.y2 = y2;
        ui_tree_nodes[node].data.primitive.color = color;
    }
    if(ui_tree_building)
        return;
    DrawLine(x1, y1, x2, y2, color);
}

void
Triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color)
{
    int min_x = x1 < x2 ? x1 : x2;
    int min_y = y1 < y2 ? y1 : y2;
    int max_x = x1 > x2 ? x1 : x2;
    int max_y = y1 > y2 ? y1 : y2;
    NodeId node;

    if(x3 < min_x)
        min_x = x3;
    if(y3 < min_y)
        min_y = y3;
    if(x3 > max_x)
        max_x = x3;
    if(y3 > max_y)
        max_y = y3;

    node = ui_tree_add(0, UI_WIDGET_TRIANGLE_NODE,
                       (Rectangle){min_x, min_y, max_x - min_x,
                                   max_y - min_y}, NULL);
    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.x1 = x1;
        ui_tree_nodes[node].data.primitive.y1 = y1;
        ui_tree_nodes[node].data.primitive.x2 = x2;
        ui_tree_nodes[node].data.primitive.y2 = y2;
        ui_tree_nodes[node].data.primitive.x3 = x3;
        ui_tree_nodes[node].data.primitive.y3 = y3;
        ui_tree_nodes[node].data.primitive.color = color;
    }
    if(ui_tree_building)
        return;
    DrawTriangle((Vector2){(float)x1, (float)y1},
                 (Vector2){(float)x2, (float)y2},
                 (Vector2){(float)x3, (float)y3}, color);
}

void
Bevel(int x, int y, int w, int h, Color light, Color dark)
{
    ui_tree_add(0, UI_WIDGET_RECT_NODE, (Rectangle){x, y, w, h}, NULL);
    RenderBevel(x, y, w, h, light, dark);
}

int
ButtonNode(ButtonSpec button)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(button.props.id, UI_WIDGET_BUTTON_NODE, button.props.bounds,
                       NULL);
    if(node >= 0) {
        ui_tree_nodes[node].owned_text = ui_tree_strdup(button.props.label);
        ui_tree_nodes[node].data.button = button;
        ui_tree_nodes[node].data.button.props.label =
            ui_tree_nodes[node].owned_text;
    }
    clicked = ui_tree_building ? HandleButton(button) : ui_button_render(button);
    ui_tree_note_build_activation(clicked);
    return clicked;
}

void
ui_tree_submit_text_input(Rectangle bounds, const char *text,
                          UIWidgetTextInputPaint paint, int focus_id)
{
    if(!ui_tree_building) {
        ui_paint_text_input(bounds, text, paint);
        return;
    }
    NodeId id = ui_tree_add(focus_id, UI_WIDGET_TEXT_INPUT_PAINT_NODE, bounds, NULL);
    if(id >= 0) {
        ui_tree_nodes[id].owned_text = ui_tree_strdup(text);
        ui_tree_nodes[id].data.text_input_paint = paint;
        InvalidateTree(UI_INVALIDATE_PAINT);
    }
}

int
TextField(TextFieldProps field)
{
    NodeId node = ui_tree_add(field.focus_id, UI_WIDGET_TEXT_FIELD_NODE,
                                field.bounds, NULL);

    if(field.commit_pressed != NULL)
        *field.commit_pressed = 0;
    if(node >= 0) {
        ui_tree_nodes[node].key = (KeyID)(unsigned)field.focus_id;
        ui_tree_nodes[node].data.text_field = field;
    }
    if(ui_tree_building)
        return 0;
    return ui_text_field_render(field);
}

void
Icon(int id, int x, int y, int size, UIIconType icon, Color tint)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, size, size},
                NULL);
    DrawIcon(icon, (Rectangle){x, y, size, size}, tint);
}

int
Dropdown(DropdownProps dropdown)
{
    ui_tree_add(dropdown.id, UI_WIDGET_DROPDOWN_NODE, dropdown.bounds,
                dropdown.selected_index);
    return ui_dropdown(dropdown);
}

int
Toggle(ToggleProps toggle)
{
    int focused = 0;
    int changed;
    int paint_value;
    int id = toggle.id;
    int *value = toggle.value;
    Rectangle bounds = toggle.bounds;
    NodeId node = ui_tree_add(id, UI_WIDGET_TOGGLE_NODE, bounds, NULL);
    if(node >= 0) {
        ui_tree_nodes[node].data.toggle.value = value;
        ui_tree_nodes[node].data.toggle.off_label = toggle.off_label;
        ui_tree_nodes[node].data.toggle.on_label = toggle.on_label;
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    changed = value != NULL && !toggle.disabled && ui_focusable_pressed(
        node >= 0 ? ui_tree_nodes[node].bounds
                  : bounds,
        id, value == NULL || toggle.disabled, &focused);
    if(changed) {
        *value = !*value;
        if(node >= 0) {
            UIEvent event = {0};
            event.key = ui_tree_nodes[node].key;
            event.kind = UI_EVENT_VALUE_CHANGED;
            event.timestamp = GetTime();
            event.data.value = *value;
            ui_event_push(event);
        }
    }
    if(ui_tree_building)
        return changed;
    paint_value = value != NULL ? *value : 0;
    (void)ToggleSwitch((int)bounds.x, (int)bounds.y, (int)bounds.width,
                       (int)bounds.height,
                       value != NULL && !toggle.disabled ? &paint_value : NULL,
                       toggle.off_label, toggle.on_label, focused);
    return changed;
}

int
Checkbox(CheckboxProps checkbox)
{
    int changed;
    NodeId node = ui_tree_add(checkbox.id, UI_WIDGET_CHECKBOX_NODE,
                              checkbox.bounds, NULL);
    if(node >= 0) {
        ui_tree_nodes[node].data.checkbox.value = checkbox.value;
        ui_tree_nodes[node].data.checkbox.label = checkbox.label;
        ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    changed = RenderCheckbox(checkbox);
    if(changed) {
        if(node >= 0) {
            UIEvent event = {0};
            event.key = ui_tree_nodes[node].key;
            event.kind = UI_EVENT_VALUE_CHANGED;
            event.timestamp = GetTime();
            event.data.value = checkbox.value != NULL ? *checkbox.value :
                (checkbox.flags != NULL ? *checkbox.flags : 0);
            ui_event_push(event);
        }
    }
    return changed;
}

void
Separator(SeparatorProps separator)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, separator.bounds, &separator);
    RenderSeparator(separator);
}

int
DragDropSource(DragDropSourceProps source)
{
    ui_tree_add(source.id, UI_WIDGET_CUSTOM_NODE, source.bounds, &source);
    return RenderDragDropSource(source);
}

int
DragDropTarget(DragDropTargetProps target)
{
    ui_tree_add(target.id, UI_WIDGET_CUSTOM_NODE, target.bounds, &target);
    return RenderDragDropTarget(target);
}

int
MultiSelectList(MultiSelectListProps list)
{
    ui_tree_add(list.id, UI_WIDGET_CUSTOM_NODE, list.bounds, &list);
    return RenderMultiSelectList(list);
}

MenuBarResult
MenuBar(int id, Rectangle bounds, const Menu *menus,
              int menu_count, int *open_index)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, bounds, open_index);
    return RenderMenuBar(id, bounds, menus, menu_count, open_index);
}

int
PopupMenu(int id, int x, int y, const MenuItem *items,
                int item_count)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, 0, 0}, items);
    return RenderPopupMenu(id, x, y, items, item_count);
}

int
ContextMenu(ContextMenuProps menu)
{
    ui_tree_add(menu.id, UI_WIDGET_CUSTOM_NODE, menu.trigger, &menu);
    return RenderContextMenu(menu);
}

int
Radio(RadioProps radio)
{
    ui_tree_add(radio.id, UI_WIDGET_CUSTOM_NODE, radio.bounds, &radio);
    return RenderRadio(radio);
}

void
Progress(ProgressProps progress)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, progress.bounds, &progress);
    RenderProgress(progress);
}

void
Plot(PlotProps plot)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, plot.bounds, &plot);
    if(plot.mode == 1)
        RenderPlotHistogram(plot);
    else
        RenderPlotLines(plot);
}

int
ui_tree_drag_float(UIFloatDragProps drag)
{
    NodeId id = ui_tree_add(drag.id, UI_WIDGET_DRAG_NODE, drag.bounds, NULL);
    if(id >= 0) {
        InvalidateTree(UI_INVALIDATE_PAINT);
        UIWidgetNode *node = &ui_tree_nodes[id];
        drag.bounds = node->bounds;
        node->data.drag.props = (DragProps){.bounds = drag.bounds,
            .id = drag.id, .label = NULL, .kind = NumericFloat,
            .float_values = drag.values, .value_count = drag.value_count,
            .speed = drag.speed, .min = drag.min, .max = drag.max,
            .format = NULL, .disabled = drag.disabled};
        node->owned_text = ui_tree_numeric_text(drag.label,
            drag.format != NULL ? drag.format : "%.3f",
            &node->data.drag.format_offset);
    }
    int changed = ui_update_drag_float(drag);
    if(!ui_tree_building)
        ui_paint_drag_float(drag);
    return changed;
}

int
ui_tree_drag_int(UIIntDragProps drag)
{
    NodeId id = ui_tree_add(drag.id, UI_WIDGET_DRAG_NODE, drag.bounds, NULL);
    if(id >= 0) {
        InvalidateTree(UI_INVALIDATE_PAINT);
        UIWidgetNode *node = &ui_tree_nodes[id];
        drag.bounds = node->bounds;
        node->data.drag.props = (DragProps){.bounds = drag.bounds,
            .id = drag.id, .label = NULL, .kind = NumericInt,
            .int_values = drag.values, .value_count = drag.value_count,
            .speed = drag.speed, .min = drag.min, .max = drag.max,
            .format = NULL, .disabled = drag.disabled};
        node->owned_text = ui_tree_numeric_text(drag.label,
            drag.format != NULL ? drag.format : "%d",
            &node->data.drag.format_offset);
    }
    int changed = ui_update_drag_int(drag);
    if(!ui_tree_building)
        ui_paint_drag_int(drag);
    return changed;
}

static Rectangle
ui_tree_drag_range_begin(Rectangle bounds, int id)
{
    NodeId row = Row((RowProps){.bounds = bounds});
    if(row >= 0) {
        UIWidgetNode *node = &ui_tree_nodes[row];
        node->id = id;
        node->key = (KeyID)(unsigned)id;
        bounds = node->bounds;
    }
    return bounds;
}

static void
ui_tree_drag_range_end(Rectangle bounds, const char *label)
{
    End();
    if(label != NULL && (ui_tree_building || IsWindowReady())) {
        int font = GetSmallFontSize();
        Text((TextProps){.bounds={(int)bounds.x + Scale(6), (int)bounds.y - font - Scale(2), 0, 0}, .text=label, .font=font, .color=c_text, .wrap=TextWrapNone});
    }
}

int
ui_tree_drag_float_range(UIFloatDragRangeProps drag)
{
    if(drag.current_min == NULL || drag.current_max == NULL)
        return 0;
    Rectangle bounds = ui_tree_drag_range_begin(drag.bounds, drag.id);
    Rectangle low = bounds, high = bounds;
    low.width *= 0.5f;
    high.x += low.width;
    high.width -= low.width;
    float old_min = *drag.current_min;
    int changed = ui_tree_drag_float((UIFloatDragProps){low, drag.id, NULL, drag.current_min,
        1, drag.speed, drag.min, *drag.current_max, drag.format, drag.disabled});
    changed |= ui_tree_drag_float((UIFloatDragProps){high, ui_numeric_focus_id(drag.id,1,0), NULL, drag.current_max,
        1, drag.speed, old_min, drag.max,
        drag.format_max != NULL ? drag.format_max : drag.format, drag.disabled});
    if(*drag.current_min > *drag.current_max)
        *drag.current_min = *drag.current_max;
    ui_tree_drag_range_end(bounds, drag.label);
    return changed;
}

int
ui_tree_drag_int_range(UIIntDragRangeProps drag)
{
    if(drag.current_min == NULL || drag.current_max == NULL)
        return 0;
    Rectangle bounds = ui_tree_drag_range_begin(drag.bounds, drag.id);
    Rectangle low = bounds, high = bounds;
    low.width *= 0.5f;
    high.x += low.width;
    high.width -= low.width;
    int old_min = *drag.current_min;
    int changed = ui_tree_drag_int((UIIntDragProps){low, drag.id, NULL, drag.current_min,
        1, drag.speed, drag.min, *drag.current_max, drag.format, drag.disabled});
    changed |= ui_tree_drag_int((UIIntDragProps){high, ui_numeric_focus_id(drag.id,1,1), NULL, drag.current_max,
        1, drag.speed, old_min, drag.max,
        drag.format_max != NULL ? drag.format_max : drag.format, drag.disabled});
    if(*drag.current_min > *drag.current_max)
        *drag.current_min = *drag.current_max;
    ui_tree_drag_range_end(bounds, drag.label);
    return changed;
}

int
Drag(DragProps drag)
{
    int count = drag.value_count;

    if(drag.mode == DragRange) {
        if(drag.kind == NumericInt) {
            return ui_tree_drag_int_range((UIIntDragRangeProps){
                drag.bounds, drag.id, drag.label, drag.int_min, drag.int_max,
                drag.speed, (int)drag.min, (int)drag.max, drag.format,
                drag.format_max, drag.disabled});
        }
        return ui_tree_drag_float_range((UIFloatDragRangeProps){
            drag.bounds, drag.id, drag.label, drag.float_min, drag.float_max,
            drag.speed, (float)drag.min, (float)drag.max, drag.format,
            drag.format_max, drag.disabled});
    }
    if(count <= 0) count = 1;
    if(drag.kind == NumericInt) {
        return ui_tree_drag_int((UIIntDragProps){
            drag.bounds, drag.id, drag.label, drag.int_values, count,
            drag.speed, (int)drag.min, (int)drag.max, drag.format,
            drag.disabled});
    }
    return ui_tree_drag_float((UIFloatDragProps){
        drag.bounds, drag.id, drag.label, drag.float_values, count,
        drag.speed, (float)drag.min, (float)drag.max, drag.format,
        drag.disabled});
}

static char *
ui_tree_numeric_text(const char *label, const char *format, size_t *offset)
{
    if(label == NULL) label = "";
    *offset = strlen(label)+1;
    size_t format_size = strlen(format)+1;
    char *text = malloc(*offset+format_size);
    if(text != NULL) {
        memcpy(text,label,*offset);
        memcpy(text+*offset,format,format_size);
    }
    return text;
}

static int
ui_tree_float_slider(UIFloatSliderProps slider, int vertical)
{
    NodeId id = ui_tree_add(slider.id, UI_WIDGET_SLIDER_NODE,
                            slider.bounds, NULL);
    if(id >= 0) {
        /* Caller-owned values can change without an input event. */
        InvalidateTree(UI_INVALIDATE_PAINT);
        UIWidgetNode *node = &ui_tree_nodes[id];
        slider.bounds = node->bounds;
        node->data.slider.props = (SliderProps){.bounds = slider.bounds,
            .id = slider.id, .label = NULL, .kind = NumericFloat,
            .float_values = slider.values, .value_count = slider.value_count,
            .min = slider.min, .max = slider.max, .format = NULL,
            .disabled = slider.disabled, .vertical = vertical};
        node->owned_text = ui_tree_numeric_text(slider.label,
            slider.format != NULL ? slider.format : "%.3f",
            &node->data.slider.format_offset);
    }
    int changed = ui_update_slider_float(slider,vertical);
    if(!ui_tree_building) ui_paint_slider_float(slider,vertical);
    if(changed) InvalidateTree(UI_INVALIDATE_PAINT);
    return changed;
}

static int
ui_tree_int_slider(UIIntSliderProps slider, int vertical)
{
    NodeId id = ui_tree_add(slider.id, UI_WIDGET_SLIDER_NODE,
                            slider.bounds, NULL);
    if(id >= 0) {
        InvalidateTree(UI_INVALIDATE_PAINT);
        UIWidgetNode *node = &ui_tree_nodes[id];
        slider.bounds = node->bounds;
        node->data.slider.props = (SliderProps){.bounds = slider.bounds,
            .id = slider.id, .label = NULL, .kind = NumericInt,
            .int_values = slider.values, .value_count = slider.value_count,
            .min = slider.min, .max = slider.max, .format = NULL,
            .disabled = slider.disabled, .vertical = vertical};
        node->owned_text = ui_tree_numeric_text(slider.label,
            slider.format != NULL ? slider.format : "%d",
            &node->data.slider.format_offset);
    }
    int changed = ui_update_slider_int(slider,vertical);
    if(!ui_tree_building) ui_paint_slider_int(slider,vertical);
    if(changed) InvalidateTree(UI_INVALIDATE_PAINT);
    return changed;
}

int
ui_tree_slider_float(UIFloatSliderProps slider)
{
    return ui_tree_float_slider(slider, 0);
}

int
ui_tree_slider_int(UIIntSliderProps slider)
{
    return ui_tree_int_slider(slider, 0);
}

int
ui_tree_vslider_float(UIFloatSliderProps slider)
{
    return ui_tree_float_slider(slider, 1);
}

int
ui_tree_vslider_int(UIIntSliderProps slider)
{
    return ui_tree_int_slider(slider, 1);
}

int
ui_tree_slider_angle(UIAngleSliderProps slider)
{
    NodeId id = ui_tree_add(slider.id, UI_WIDGET_SLIDER_NODE,
                            slider.bounds, NULL);
    if(id >= 0) {
        InvalidateTree(UI_INVALIDATE_PAINT);
        UIWidgetNode *node = &ui_tree_nodes[id];
        slider.bounds = node->bounds;
        node->data.slider.props = (SliderProps){.bounds = slider.bounds,
            .id = slider.id, .label = NULL, .kind = NumericFloat,
            .float_value = slider.value, .min = slider.min_degrees,
            .max = slider.max_degrees, .format = NULL,
            .disabled = slider.disabled, .angle = 1};
        node->owned_text = ui_tree_numeric_text(slider.label,
            slider.format != NULL ? slider.format : "%.3f",
            &node->data.slider.format_offset);
    }
    int changed = ui_update_slider_angle(slider);
    if(!ui_tree_building)
        ui_paint_slider_angle(slider);
    if(changed)
        InvalidateTree(UI_INVALIDATE_PAINT);
    return changed;
}

int
Slider(SliderProps slider)
{
    int count = slider.value_count;

    if(count <= 0) count = 1;
    if(slider.angle) {
        return ui_tree_slider_angle((UIAngleSliderProps){
            slider.bounds, slider.id, slider.label, slider.float_value,
            (float)slider.min, (float)slider.max, slider.format,
            slider.disabled});
    }
    if(slider.kind == NumericInt) {
        UIIntSliderProps props = {slider.bounds, slider.id, slider.label,
                                slider.int_values, count, (int)slider.min,
                                (int)slider.max, slider.format,
                                slider.disabled};
        return slider.vertical ? ui_tree_vslider_int(props) : ui_tree_slider_int(props);
    }
    UIFloatSliderProps props = {slider.bounds, slider.id, slider.label,
                              slider.float_values, count, (float)slider.min,
                              (float)slider.max, slider.format,
                              slider.disabled};
    return slider.vertical ? ui_tree_vslider_float(props) : ui_tree_slider_float(props);
}

static int
ui_numeric_input_begin(int id, Rectangle *bounds)
{
    int depth = ui_tree_stack_depth;
    NodeId node = ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, *bounds, NULL);
    if(node >= 0) {
        *bounds = ui_tree_nodes[node].bounds;
        if(ui_tree_stack_depth >= UI_TREE_MAX_DEPTH) abort();
        ui_tree_stack[ui_tree_stack_depth++] = node;
    }
    return depth;
}

int
ui_tree_input_float(UIFloatInputProps input)
{
    int depth = ui_numeric_input_begin(input.id, &input.bounds);
    int changed = RenderInputScalar(input);
    ui_tree_stack_depth = depth;
    return changed;
}

int
ui_tree_input_int(UIIntInputProps input)
{
    int depth = ui_numeric_input_begin(input.id, &input.bounds);
    int changed = RenderInputWhole(input);
    ui_tree_stack_depth = depth;
    return changed;
}

int
ui_tree_input_double(UIDoubleInputProps input)
{
    int depth = ui_numeric_input_begin(input.id, &input.bounds);
    int changed = RenderInputDouble(input);
    ui_tree_stack_depth = depth;
    return changed;
}

int
Input(InputProps input)
{
    int count = input.value_count;

    if(count <= 0) count = 1;
    if(input.kind == NumericInt) {
        return ui_tree_input_int((UIIntInputProps){
            input.bounds, input.id, input.label, input.int_values, count,
            (int)input.step, (int)input.step_fast, input.format,
            input.disabled});
    }
    if(input.kind == NumericDouble) {
        return ui_tree_input_double((UIDoubleInputProps){
            input.bounds, input.id, input.label, input.double_values, count,
            input.step, input.step_fast, input.format, input.disabled});
    }
    return ui_tree_input_float((UIFloatInputProps){
        input.bounds, input.id, input.label, input.float_values, count,
        (float)input.step, (float)input.step_fast, input.format,
        input.disabled});
}

int
Spinbox(SpinboxProps spinbox)
{
    ui_tree_add(spinbox.id, UI_WIDGET_CUSTOM_NODE, spinbox.bounds, &spinbox);
    return RenderSpinbox(spinbox);
}

void
LabelFrame(LabelFrameProps frame)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, frame.bounds, &frame);
    RenderLabelFrame(frame);
}

int
ListBox(ListBoxProps list)
{
    ui_tree_add(list.id, UI_WIDGET_CUSTOM_NODE, list.bounds, &list);
    return RenderListBox(list);
}

int
TreeView(TreeViewProps tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return RenderTreeView(tree);
}

int
TableView(TableViewProps table)
{
    ui_tree_add(table.id, UI_WIDGET_CUSTOM_NODE, table.bounds, &table);
    return RenderTableView(table);
}

int
TextArea(TextAreaProps area)
{
    NodeId node = ui_tree_add(area.focus_id, UI_WIDGET_TEXT_AREA_NODE,
                                area.bounds, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].key = (KeyID)(unsigned)area.focus_id;
        ui_tree_nodes[node].data.text_area = area;
    }
    /* The retained painter calls the multiline renderer in node order. Keep
     * paint live for caret blinking and text input even when layout is stable. */
    ui_tree_invalid |= UI_INVALIDATE_PAINT;
    if(ui_tree_building)
        return 0;
    return ui_text_area_render(area);
}

void
CanvasGrid(Rectangle bounds, int step, Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    RenderCanvasGrid(bounds, step, color);
}

int
PanedView(PanedViewProps panes)
{
    ui_tree_add(panes.id, UI_WIDGET_CUSTOM_NODE, panes.bounds, &panes);
    return RenderPanedView(panes);
}

int
Collapsible(CollapsibleProps section)
{
    ui_tree_add(section.id, UI_WIDGET_CUSTOM_NODE, section.bounds, &section);
    return RenderCollapsible(section);
}

int
ColorPicker(ColorPickerProps picker)
{
    ui_tree_add(picker.id, UI_WIDGET_CUSTOM_NODE, picker.bounds, &picker);
    return RenderColorPicker(picker);
}

/* Paint declarations preceding an immediate overlay before its scrim. Keep
 * pending declarations intact for the normal end-of-frame reconciliation. */
static void
ui_tree_paint_before_overlay(void)
{
    if(!ui_tree_building || !IsWindowReady())
        return;
    ReconcileTree();
    LayoutTree();
    InvalidateTree(UI_INVALIDATE_PAINT);
    DrawTree();
    for(int i = 0; i < ui_tree_node_count; i++) {
        if(ui_committed_nodes[i].owned_text != NULL) {
            size_t size = ui_tree_owned_text_size(&ui_committed_nodes[i]);
            ui_tree_nodes[i].owned_text = malloc(size);
            if(ui_tree_nodes[i].owned_text != NULL)
                memcpy(ui_tree_nodes[i].owned_text,ui_committed_nodes[i].owned_text,size);
        }
        ui_tree_nodes[i].flags |= UI_NODE_PAINTED_IMMEDIATE;
    }
}

int
Modal(ModalProps modal)
{
    ui_tree_paint_before_overlay();
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return RenderActionModal(modal);
}

void
Focus(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    RenderFocus(bounds);
}

void
FocusDebugOverlay(const UIAccessibilityNode *nodes, int count)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, nodes);
    RenderFocusDebugOverlay(nodes, count);
}

void
TransitionFade(const UITransition *transition, int width, int height,
                     Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height},
                transition);
    RenderTransitionFade(transition, width, height, color);
}

NavigationBarResult
NavigationBar(NavigationBarProps nav)
{
    ui_tree_add(0, UI_WIDGET_NAVIGATION_BAR_NODE,
                (Rectangle){0, 0, nav.view_width, nav.view_height}, &nav);
    return RenderNavigationBar(nav);
}

ToolbarResult
Toolbar(ToolbarProps toolbar)
{
    ui_tree_add(toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){toolbar.x, toolbar.y, toolbar.width, toolbar.height}, &toolbar);
    return RenderToolbar(toolbar);
}

int
TabBar(TabBarProps bar)
{
    int clicked;

    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    clicked = RenderTabBar(bar);
    ui_tree_note_build_activation(clicked >= 0);
    return clicked;
}

int
TitleBar(TitleBarProps title_bar)
{
    int id = title_bar.has_dropdown ? title_bar.dropdown.id : 0;
    int height = title_bar.height > 0 ? title_bar.height : ui_title_bar_height();

    ui_tree_add(id, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, &title_bar);
    return RenderTitleBar(title_bar);
}

static Rectangle
resolve_button_bounds(ButtonProps button, int disclosure)
{
    button.disabled = button.disabled || UIContentDisabled();
    Rectangle bounds = button.bounds;
    ThemeMetrics metrics = GetThemeMetrics();
    Style style = ResolveButtonStyle(button, button.state);
    int height = Scale(SizeValue(button.size, metrics.control_height_small,
        metrics.control_height_medium, metrics.control_height_large));
    int font = ResolveFont(button.font, Scale(style.font_size), GetFontSize());
    float available_width = 0.0f;
    float scale = (float)Scale(1000) / 1000.0f;
    if(button.full_width && bounds.width <= 0) {
        float right = (float)GetUIViewWidth();

        if(ui_tree_stack_depth > 0) {
            Rectangle parent = ui_tree_nodes[
                ui_tree_stack[ui_tree_stack_depth - 1]].bounds;
            if(parent.width > 0)
                right = parent.x + parent.width;
        }
        available_width = right - bounds.x;
    }
    return MeasureButton(button, style, height, font, available_width, scale, disclosure);
}

static ButtonSpec
ui_tree_button_spec(ButtonProps button, Rectangle surface_bounds, int disclosure)
{
    Style paint;
    button.disabled = button.disabled || button.state == ButtonStateDisabled || UIContentDisabled();
    button.loading = button.loading || button.state == ButtonStateLoading;
    ButtonSpec spec = {
        .props = button,
        .surface_bounds = surface_bounds,
        .disclosure = disclosure,
        .style_resolved = 1
    };
    paint = ResolveButtonStyle(button, button.state);
    spec.paint = paint;
    spec.paint.radius = ui_radius_px(button.bounds, paint.radius);
    spec.hover_background = paint.background;
    return spec;
}

static int
ui_tree_surface_button(ButtonProps button, Rectangle surface_bounds, int disclosure)
{
    button.bounds = resolve_button_bounds(button, disclosure);
    button.id = ResolveUIFocusID(button.id);
    ButtonSpec spec = ui_tree_button_spec(button, surface_bounds, disclosure);
    NodeId node = ui_tree_add(button.id, UI_WIDGET_BUTTON_NODE,
                                button.bounds, NULL);
    int clicked;

    if(node >= 0) {
        spec.props.bounds = ui_tree_nodes[node].bounds;
        ui_tree_nodes[node].owned_text = ui_tree_strdup(button.label);
        ui_tree_nodes[node].data.button = spec;
        ui_tree_nodes[node].data.button.props.label =
            ui_tree_nodes[node].owned_text;
    }
    clicked = ui_tree_building ? HandleButton(spec) : ui_button_render(spec);
    ui_tree_note_build_activation(clicked);
    return clicked;
}

static const char *
ui_tree_arrow_button_label(int glyph)
{
    if(glyph == 62)
        return ">";
    if(glyph == 94)
        return "^";
    if(glyph == 118)
        return "v";
    return "<";
}

int
Button(ButtonProps button)
{
    int open_local = 0;
    int activated = 0;

    if(button.info) {
        int diameter = (int)button.bounds.width;
        if(button.bounds.height > 0 && button.bounds.height < button.bounds.width)
            diameter = (int)button.bounds.height;
        if(diameter <= 0)
            diameter = Scale(18);
        Rectangle bounds = button.bounds;
        if(bounds.width <= 0)
            bounds.width = diameter;
        if(bounds.height <= 0)
            bounds.height = diameter;
        NodeId node = ui_tree_add(button.id, UI_WIDGET_BUTTON_NODE,
                                  bounds, NULL);
        int clicked = RenderButtonInfoIndicator((int)(bounds.x + bounds.width / 2),
                                       (int)(bounds.y + bounds.height / 2),
                                       diameter);
        ui_tree_note_build_activation(clicked);
        ui_tree_mark_painted_immediate(node);
        return clicked;
    }
    if(button.arrow) {
        button.label = ui_tree_arrow_button_label(
            ButtonArrowGlyph((int)button.direction));
        if(button.font <= 0)
            button.font = GetSmallFontSize();
        return ui_tree_surface_button(button, (Rectangle){0}, 0);
    }
    if(button.menu || button.split) {
        if(button.open == NULL)
            button.open = &open_local;
        if(button.activated_id == NULL)
            button.activated_id = &activated;
        *button.activated_id = 0;
    }
    if(button.split) {
        ButtonProps action = button;
        ButtonProps menu = button;
        if(action.id == 0) {
            action.id = ResolveUIFocusID(0);
            menu.id = ResolveUIFocusID(0);
        } else {
            menu.id = action.id + 1;
        }
        action.bounds = resolve_button_bounds(action, 0);
        ButtonSplitLayout layout = ButtonResolveSplitLayout(action.bounds.width,
                                                            action.bounds.height);
        action.bounds.width = layout.action_width;
        menu.bounds = (Rectangle){action.bounds.x + layout.menu_offset,
                                  action.bounds.y, layout.menu_width,
                                  action.bounds.height};
        menu.label = "Open menu";
        menu.icon_type = UI_ICON_TYPE_NONE;
        menu.icon_only = 1;
        menu.square = 1;
        menu.menu = 0;
        menu.split = 0;
        Rectangle surface_bounds = {action.bounds.x, action.bounds.y,
                                    layout.width, action.bounds.height};
        int clicked = ui_tree_surface_button(action, surface_bounds, 0);
        *button.open = ButtonToggleMenuOpen(*button.open,
            ui_tree_surface_button(menu, surface_bounds, 1));
        {
            Color divider = GetThemeBorder();
            int inset = Scale((int)layout.divider_inset);
            divider.a = GetThemeMetrics().border_alpha;
            Line((int)menu.bounds.x, (int)menu.bounds.y + inset,
                 (int)menu.bounds.x,
                 (int)(menu.bounds.y + menu.bounds.height) - inset,
                 divider);
        }
        if(*button.open) {
            *button.activated_id = PopupMenu(
                button.menu_id, (int)action.bounds.x,
                (int)(action.bounds.y + action.bounds.height),
                button.items, button.item_count);
            *button.open = ButtonCloseMenuAfterActivation(
                *button.open, *button.activated_id);
        }
        return clicked;
    }
    if(button.menu) {
        button.icon_placement = IconPlacementTrailing;
        button.icon_type = UI_ICON_TYPE_NONE;
        button.bounds = resolve_button_bounds(button, 1);
        *button.open = ButtonToggleMenuOpen(*button.open,
            ui_tree_surface_button(button, (Rectangle){0}, 1));
        if(*button.open) {
            *button.activated_id = PopupMenu(button.menu_id,
                (int)button.bounds.x,
                (int)(button.bounds.y + button.bounds.height),
                button.items, button.item_count);
            *button.open = ButtonCloseMenuAfterActivation(
                *button.open, *button.activated_id);
        }
        return *button.activated_id != 0;
    }
    return ui_tree_surface_button(button, (Rectangle){0}, 0);
}

NodeId
BeginButton(ButtonProps button)
{
    ButtonSpec spec;
    NodeId node;

    button.bounds = resolve_button_bounds(button, 0);
    button.id = ResolveUIFocusID(button.id);
    spec = ui_tree_button_spec(button, (Rectangle){0}, 0);
    spec.props.label = "";

    node = ui_tree_add(button.id, UI_WIDGET_BUTTON_NODE, button.bounds, NULL);
    if(node < 0)
        return node;
    spec.props.bounds = ui_tree_nodes[node].bounds;
    ui_tree_nodes[node].owned_text = ui_tree_strdup(button.label);
    ui_tree_nodes[node].data.button = spec;
    if(ui_tree_stack_depth < UI_TREE_MAX_DEPTH)
        ui_tree_stack[ui_tree_stack_depth++] = node;
    if(button.label != NULL && button.label[0] != '\0')
        Text((TextProps){.text = button.label, .wrap = TextWrapNone});
    return node;
}

int
Card(CardProps card)
{
    ButtonProps button = ui_card_button_props(card);
    ButtonSpec spec;
    NodeId node;
    int clicked = 0;

    button.bounds = resolve_button_bounds(button, 0);
    if(card.clickable)
        button.id = ResolveUIFocusID(button.id);
    spec = ui_tree_button_spec(button, (Rectangle){0}, 0);
    spec.props.label = "";

    node = ui_tree_add(button.id, UI_WIDGET_CARD_NODE, button.bounds, NULL);
    if(node >= 0) {
        spec.props.bounds = ui_tree_nodes[node].bounds;
        ui_tree_nodes[node].data.button = spec;
    }
    if(card.clickable) {
        clicked = ui_tree_building ? HandleButton(spec) : ui_button_render(spec);
        ui_tree_note_build_activation(clicked);
    } else if(!ui_tree_building) {
        ui_paint_button(spec, 0, 0);
    }
    return clicked;
}

NodeId
BeginCard(CardProps card)
{
    ButtonProps button = ui_card_button_props(card);
    ButtonSpec spec;
    NodeId node;

    button.bounds = resolve_button_bounds(button, 0);
    if(card.clickable)
        button.id = ResolveUIFocusID(button.id);
    spec = ui_tree_button_spec(button, (Rectangle){0}, 0);
    spec.props.label = "";

    node = ui_tree_add(button.id, UI_WIDGET_CARD_NODE, button.bounds, NULL);
    if(node < 0)
        return node;
    spec.props.bounds = ui_tree_nodes[node].bounds;
    ui_tree_nodes[node].data.button = spec;
    if(card.clickable)
        ui_tree_note_build_activation(ui_tree_building ? HandleButton(spec)
                                                       : ui_button_render(spec));
    else if(!ui_tree_building)
        ui_paint_button(spec, 0, 0);
    if(ui_tree_stack_depth < UI_TREE_MAX_DEPTH)
        ui_tree_stack[ui_tree_stack_depth++] = node;
    return node;
}

int
Selectable(SelectableProps selectable)
{
    ui_tree_add(selectable.id, UI_WIDGET_CUSTOM_NODE, selectable.bounds,
                &selectable);
    return RenderSelectable(selectable);
}

int
InvisibleButton(InvisibleButtonProps button)
{
    NodeId node = ui_tree_add(button.id, UI_WIDGET_BUTTON_NODE, button.bounds, &button);
    int clicked = RenderInvisibleButton(button);

    ui_tree_note_build_activation(clicked);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

void
Bullet(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    RenderBullet(bounds);
}

/* Retained layout containers. Every container closes with End(). */

static NodeId
ui_begin_layout_node(UIWidgetKind kind, KeyID key, Rectangle bounds,
                     int gap, int padding)
{
    NodeId node;

    if(key == 0)
        key = (KeyID)(unsigned)(ui_tree_node_count + 1);
    node = ui_tree_add((int)(key & 0x7fffffffU), kind, bounds, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].key = key;
        ui_tree_nodes[node].data.layout.gap = gap;
        ui_tree_nodes[node].data.layout.padding = padding;
        ui_tree_nodes[node].data.layout.columns = 1;
        if(ui_tree_stack_depth < UI_TREE_MAX_DEPTH)
            ui_tree_stack[ui_tree_stack_depth++] = node;
    }
    return node;
}

NodeId
Column(ColumnProps props)
{
    return ui_begin_layout_node(UI_WIDGET_COLUMN_NODE, props.key, props.bounds,
                                props.gap, props.padding);
}

NodeId
Row(RowProps props)
{
    return ui_begin_layout_node(UI_WIDGET_ROW_NODE, props.key, props.bounds,
                                props.gap, props.padding);
}

NodeId
Grid(GridProps props)
{
    NodeId node = ui_begin_layout_node(UI_WIDGET_GRID_NODE, props.key,
                                       props.bounds, props.gap,
                                       props.padding);

    if(node >= 0) {
        ui_tree_nodes[node].data.layout.columns = props.columns;
        ui_tree_nodes[node].data.layout.min_item_width = props.min_item_width;
        ui_tree_nodes[node].data.layout.max_columns = props.max_columns;
    }
    return node;
}

NodeId
Stack(ColumnProps props)
{
    return ui_begin_layout_node(UI_WIDGET_STACK_NODE, props.key, props.bounds,
                                props.gap, props.padding);
}

NodeId
Screen(ColumnProps props)
{
    return ui_begin_layout_node(UI_WIDGET_GROUP_NODE, props.key, props.bounds,
                                props.gap, props.padding);
}

static const RouterRoute *
router_find_route_index(const RouterRoute *routes, int route_count, int route_id,
                        int *index_out)
{
    int i;

    if(index_out != NULL)
        *index_out = -1;
    if(routes == NULL || route_count <= 0)
        return NULL;
    for(i = 0; i < route_count; i++) {
        if(routes[i].id == route_id) {
            if(index_out != NULL)
                *index_out = i;
            return &routes[i];
        }
    }
    return NULL;
}

const RouterRoute *
RouterFindRoute(const RouterRoute *routes, int route_count, int route_id)
{
    return router_find_route_index(routes, route_count, route_id, NULL);
}

void
RouterStateInit(RouterState *state, int initial_route)
{
    if(state == NULL)
        return;
    state->initialized = 1;
    state->current_route = initial_route;
    state->previous_route = initial_route;
    state->requested_route = ROUTER_NO_ROUTE;
    state->changed = 0;
    state->route_version = GetRouteVersion();
    state->generation = 0;
}

void
RouterNavigate(RouterState *state, int route_id)
{
    if(state == NULL)
        return;
    state->requested_route = route_id;
    InvalidateTree(UI_INVALIDATE_PAINT);
}

static void
router_copy_hash_id(char *dst, size_t dst_size, const char *hash)
{
    size_t n = 0;

    if(dst == NULL || dst_size == 0)
        return;
    dst[0] = '\0';
    if(hash == NULL)
        return;
    while(*hash == ' ' || *hash == '\t' || *hash == '#')
        hash++;
    if(*hash == '/')
        hash++;
    while(hash[n] != '\0' && hash[n] != '/' && hash[n] != '?' &&
          hash[n] != '&' && n + 1 < dst_size) {
        dst[n] = hash[n];
        n++;
    }
    dst[n] = '\0';
}

static int
router_route_for_hash(RouterProps props)
{
    char id[128];
    int i;

    router_copy_hash_id(id, sizeof(id), GetRouteHash());
    if(id[0] == '\0')
        return ROUTER_NO_ROUTE;
    for(i = 0; i < props.route_count; i++) {
        const char *path = props.routes[i].path;

        if(path == NULL)
            continue;
        while(*path == '#')
            path++;
        if(*path == '/')
            path++;
        if(strcmp(path, id) == 0)
            return props.routes[i].id;
    }
    return ROUTER_NO_ROUTE;
}

static void
router_write_url(RouterProps props, int route_id, int push)
{
    const RouterRoute *route;
    const char *base;
    const char *path;
    char url[320];

    if(!props.sync_url)
        return;
    route = RouterFindRoute(props.routes, props.route_count, route_id);
    if(route == NULL || route->path == NULL || route->path[0] == '\0')
        return;
    base = GetRoutePath();
    if(base == NULL || base[0] == '\0')
        base = "/";
    path = route->path;
    while(*path == '#')
        path++;
    if(*path == '/')
        path++;
    snprintf(url, sizeof(url), "%s#/%s", base, path);
    if(push)
        PushRoute(url);
    else
        ReplaceRoute(url);
    if(props.state != NULL)
        props.state->route_version = GetRouteVersion();
}

int
RouterSetRoute(RouterProps props, int route_id, int push)
{
    RouterState *state = props.state;

    if(state == NULL ||
       RouterFindRoute(props.routes, props.route_count, route_id) == NULL)
        return 0;
    if(!state->initialized)
        RouterStateInit(state, props.initial_route);
    state->changed = state->current_route != route_id;
    state->previous_route = state->current_route;
    state->current_route = route_id;
    state->requested_route = ROUTER_NO_ROUTE;
    if(state->changed)
        state->generation++;
    router_write_url(props, route_id, push);
    return state->changed;
}

RouterResult
Router(RouterProps props)
{
    RouterResult result = {0};
    RouterState *state = props.state;
    int next = ROUTER_NO_ROUTE;
    int push = 0;
    NodeId node;

    if(state == NULL)
        return result;
    if(!state->initialized) {
        int initial = props.initial_route;
        int from_hash = props.sync_url ? router_route_for_hash(props)
                                       : ROUTER_NO_ROUTE;

        if(from_hash != ROUTER_NO_ROUTE)
            initial = from_hash;
        RouterStateInit(state, initial);
        if(props.sync_url && from_hash == ROUTER_NO_ROUTE &&
           props.replace_on_init)
            router_write_url(props, initial, 0);
    }

    state->changed = 0;
    state->previous_route = state->current_route;
    if(props.sync_url) {
        int version = GetRouteVersion();

        if(version != state->route_version) {
            int from_hash = router_route_for_hash(props);

            state->route_version = version;
            if(from_hash != ROUTER_NO_ROUTE)
                next = from_hash;
        }
    }
    if(state->requested_route != ROUTER_NO_ROUTE) {
        next = state->requested_route;
        push = 1;
        state->requested_route = ROUTER_NO_ROUTE;
    }
    if(next != ROUTER_NO_ROUTE &&
       RouterFindRoute(props.routes, props.route_count, next) != NULL &&
       next != state->current_route) {
        state->previous_route = state->current_route;
        state->current_route = next;
        state->changed = 1;
        state->generation++;
        router_write_url(props, next, push);
        InvalidateTree(UI_INVALIDATE_PAINT);
    }

    node = ui_tree_add((int)((props.key != 0 ? props.key : Key("Router")) &
                             0x7fffffffU),
                       UI_WIDGET_ROUTER_NODE, props.bounds, NULL);
    if(node >= 0)
        ui_tree_nodes[node].key = props.key != 0 ? props.key : Key("Router");

    result.route = state->current_route;
    result.previous_route = state->previous_route;
    result.requested_route = state->requested_route;
    result.changed = state->changed;
    result.route_info = RouterFindRoute(props.routes, props.route_count,
                                        state->current_route);
    return result;
}

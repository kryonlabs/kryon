#include "ui_internal.h"
#include "ui_picture_internal.h"
#include "embedded_assets.h"
#include <stdio.h>
#include <stdlib.h>

#define UI_TREE_MAX_DEPTH 128
#define UI_NODE_HOVERED (1U << 28)
#define UI_NODE_PRESSED (1U << 29)
#define UI_NODE_OWNS_STATE (1U << 30)
#define UI_NODE_PAINTED_IMMEDIATE (1U << 27)

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
static int ui_tree_screen_id = 0;
static KeyID ui_tree_screen_key = 0;
static int ui_tree_building = 0;
static NodeId ui_tree_stack[UI_TREE_MAX_DEPTH];
static int ui_tree_stack_depth = 0;
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

typedef struct UIWidgetOps {
    int (*measure_height)(UIWidgetNode node);
} UIWidgetOps;

static UIWidgetNode *ui_tree_node(NodeId id);
static void DrawTree(void);

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

static int
ui_reconcile_text_changed(const char *a, const char *b)
{
    if(a == NULL)
        a = "";
    if(b == NULL)
        b = "";
    return strcmp(a, b) != 0;
}

static int
ui_reconcile_node_changed(const UIWidgetNode *old_node,
                          const UIWidgetNode *new_node)
{
    UIWidgetData old_data;
    UIWidgetData new_data;

    if(old_node == NULL || new_node == NULL)
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
    new_data = new_node->data;
    if(old_node->kind == UI_WIDGET_BUTTON_NODE) {
        old_data.button.spec.label = NULL;
        new_data.button.spec.label = NULL;
    }
    if(memcmp(&old_data, &new_data, sizeof(old_data)) != 0)
        return 1;
    return ui_reconcile_text_changed(old_node->owned_text,
                                     new_node->owned_text);
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
ui_measure_label_text_field(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_label_text_field_height(*(const LabelTextFieldProps *)node.props);
    return ui_label_text_field_height(node.data.label_text_field);
}

static int
ui_measure_section_label(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_section_label_height(*(const SectionLabelProps *)node.props);
    return ui_section_label_height(node.data.section_label);
}

static int
ui_measure_checkbox_row(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_checkbox_row_height(*(const CheckboxRowProps *)node.props);
    return ui_checkbox_row_height(node.data.checkbox_row);
}

static int
ui_measure_button_row(UIWidgetNode node)
{
    if(node.props != NULL)
        return GetUIButtonRowHeight(*(const ButtonRowProps *)node.props);
    return GetUIButtonRowHeight(node.data.button_row);
}

static int
ui_measure_bottom_nav(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_bottom_nav_height();
}

static int
ui_measure_tab_bar(UIWidgetNode node)
{
    if(node.bounds.height > 0)
        return (int)ceilf(node.bounds.height);
    return ui_tab_bar_height();
}

static int
ui_measure_theme_settings(UIWidgetNode node)
{
    if(node.props != NULL)
        return ui_theme_settings_height(*(const ThemeSettingsProps *)node.props);
    return ui_theme_settings_height(node.data.theme_settings);
}

static int
ui_measure_theme_picker(UIWidgetNode node)
{
    return ui_theme_picker_height((int)node.bounds.width);
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
    [UI_WIDGET_LINE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_BUTTON_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_FIELD_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TEXT_AREA_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_DROPDOWN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_SLIDER_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_TOGGLE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CHECKBOX_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_THEME_SETTINGS_NODE] = {ui_measure_theme_settings},
    [UI_WIDGET_PARAGRAPH_NODE] = {ui_measure_paragraph},
    [UI_WIDGET_READONLY_TEXT_BOX_NODE] = {ui_measure_readonly_text_box},
    [UI_WIDGET_LABEL_TEXT_FIELD_NODE] = {ui_measure_label_text_field},
    [UI_WIDGET_SECTION_LABEL_NODE] = {ui_measure_section_label},
    [UI_WIDGET_CHECKBOX_ROW_NODE] = {ui_measure_checkbox_row},
    [UI_WIDGET_BUTTON_ROW_NODE] = {ui_measure_button_row},
    [UI_WIDGET_BOTTOM_NAV_NODE] = {ui_measure_bottom_nav},
    [UI_WIDGET_TAB_BAR_NODE] = {ui_measure_tab_bar},
    [UI_WIDGET_THEME_PICKER_NODE] = {ui_measure_theme_picker},
    [UI_WIDGET_PARAGRAPH_MODAL_NODE] = {ui_measure_paragraph_modal},
    [UI_WIDGET_TITLE_BAR_NODE] = {ui_measure_title_bar},
    [UI_WIDGET_GROUP_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_COLUMN_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_ROW_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_STACK_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_GRID_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_PICTURE_NODE] = {ui_measure_bounds_height},
    [UI_WIDGET_CUSTOM_NODE] = {ui_measure_bounds_height},
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

    /* Embedders that never call SetUIFrame still need valid screen-to-world
     * math for input routing; a zero camera would turn every hit test into
     * NaN comparisons that silently never match. */
    ui_camera_ensure_sane();
    ui_tree_clear_pending();
    ui_tree_screen_key = screen_key != 0 ? screen_key : 1;
    ui_tree_screen_id = (int)(ui_tree_screen_key & 0x7fffffffU);
    ui_tree_node_count = 0;
    ui_tree_building = 1;
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
    int *slots = NULL;
    int *matched_old = NULL;
    UIWidgetNode *old_nodes = NULL;
    int old_count = ui_committed_node_count;
    int slot_count = 1;
    int tree_changed;
    unsigned invalid_before = ui_tree_invalid;
    int i;

    if(!ui_tree_reserve(&ui_committed_nodes, &ui_committed_node_capacity,
                        ui_tree_node_count))
        return;
    tree_changed = old_count != ui_tree_node_count;
    if(old_count > 0) {
        old_nodes = malloc((size_t)old_count * sizeof(*old_nodes));
        if(old_nodes == NULL)
            return;
        memcpy(old_nodes, ui_committed_nodes,
               (size_t)old_count * sizeof(*old_nodes));
    }
    while(slot_count < old_count * 2 + 1)
        slot_count *= 2;
    slots = malloc((size_t)slot_count * sizeof(*slots));
    if(slots == NULL) {
        free(old_nodes);
        return;
    }
    if(ui_tree_node_count > 0) {
        matched_old = malloc((size_t)ui_tree_node_count * sizeof(*matched_old));
        if(matched_old == NULL) {
            free(slots);
            free(old_nodes);
            return;
        }
        for(i = 0; i < ui_tree_node_count; i++)
            matched_old[i] = -1;
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
    free(matched_old);
    free(slots);
    free(old_nodes);
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
           parent->kind != UI_WIDGET_GRID_NODE)
            continue;
        content_x = parent->bounds.x + parent->data.layout.padding;
        content_y = parent->bounds.y + parent->data.layout.padding;
        content_w = parent->bounds.width - parent->data.layout.padding * 2;
        content_h = parent->bounds.height - parent->data.layout.padding * 2;
        if(content_w < 0)
            content_w = 0;
        if(content_h < 0)
            content_h = 0;
        cursor = parent->kind == UI_WIDGET_ROW_NODE ? content_x : content_y;
        if(parent->kind == UI_WIDGET_GRID_NODE) {
            int columns = parent->data.layout.columns > 0
                ? parent->data.layout.columns : 1;
            int index = 0;
            float row_y = content_y;
            float row_h = 0;
            float cell_w = columns > 0
                ? (content_w - parent->data.layout.gap * (columns - 1)) /
                    columns
                : content_w;

            if(cell_w < 0)
                cell_w = 0;
            for(child = parent->first_child; child >= 0;
                child = ui_committed_nodes[child].next_sibling) {
                UIWidgetNode *node = &ui_committed_nodes[child];
                int col = index % columns;

                if(index > 0 && col == 0) {
                    row_y += row_h + parent->data.layout.gap;
                    row_h = 0;
                }
                node->bounds.x = content_x + col *
                    (cell_w + parent->data.layout.gap);
                node->bounds.y = row_y;
                if(node->bounds.width <= 0 || node->bounds.width > cell_w)
                    node->bounds.width = cell_w;
                if(node->bounds.height > row_h)
                    row_h = node->bounds.height;
                index++;
            }
            continue;
        }
        for(child = parent->first_child; child >= 0;
            child = ui_committed_nodes[child].next_sibling) {
            UIWidgetNode *node = &ui_committed_nodes[child];

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

    if(ui_committed_node_count <= 0)
        return;

    /* The retained tree owns focus order. Register every interactive node
     * before routing input so Tab follows declaration order for fields and
     * buttons exactly as it does in the immediate UI API. */
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        int focus_id = 0;

        if(node->kind == UI_WIDGET_TEXT_FIELD_NODE)
            focus_id = node->data.text_field.focus_id;
        else if(node->kind == UI_WIDGET_TEXT_AREA_NODE)
            focus_id = node->data.text_area.focus_id;
        else if(node->kind == UI_WIDGET_BUTTON_NODE &&
                !node->data.button.spec.disabled)
            focus_id = node->data.button.spec.focus_id;
        if(UIFocusFrameOpen() && focus_id > 0)
            (void)RegisterUIFocus(focus_id, node->bounds);
    }

    mouse = GetMousePosition();
    hit = HitTestNode(mouse);
    pressed = IsMouseButtonDown(MOUSE_BUTTON_LEFT);
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];
        unsigned before;

        if(node->kind != UI_WIDGET_BUTTON_NODE)
            continue;
        before = node->flags;
        node->flags &= ~(UI_NODE_HOVERED | UI_NODE_PRESSED);
        if(!node->data.button.spec.disabled &&
           CheckCollisionPointRec(mouse, node->bounds)) {
            node->flags |= UI_NODE_HOVERED;
            if(pressed)
                node->flags |= UI_NODE_PRESSED;
        }
        if(before != node->flags)
            ui_tree_invalid |= UI_INVALIDATE_PAINT;
    }
    target = IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ? hit : -1;
    if(target >= 0 && ui_committed_nodes[target].kind == UI_WIDGET_BUTTON_NODE &&
       !ui_committed_nodes[target].data.button.spec.disabled) {
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
           node->kind != UI_WIDGET_BUTTON_NODE ||
           node->data.button.spec.disabled ||
           !IsUIFocusActivatePressed(node->data.button.spec.focus_id))
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
                int font = field->font > 0 ? field->font : GetUIFontSize();
                int padding = field->style.padding_x > 0
                    ? field->style.padding_x : ScaleUIPx(10);
                double now = GetTime();
                KeyID click_key = field->focus_id > 0
                    ? (KeyID)field->focus_id : node->key;
                int click_dx = (int)mouse.x - ui_tree_text_last_click_x;
                int click_dy = (int)mouse.y - ui_tree_text_last_click_y;
                int double_click = ui_tree_text_last_click_key == click_key &&
                    now - ui_tree_text_last_click_time <= 0.45 &&
                    abs(click_dx) <= ScaleUIPx(6) &&
                    abs(click_dy) <= ScaleUIPx(6);

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
        if(!state->focused || field->text == NULL || field->text_size == 0)
            continue;
        if(state->dragging && IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            int font = field->font > 0 ? field->font : GetUIFontSize();
            int padding = field->style.padding_x > 0
                ? field->style.padding_x : ScaleUIPx(10);
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
        if(modifier && IsKeyPressed(KEY_X) && !field->secure) {
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
        if(modifier && IsKeyPressed(KEY_V)) {
            if(end > start)
                changed |= ui_text_delete_range(
                    field->text, field->text_size, &state->cursor, start, end);
            {
                TextEdit edit;

                memset(&edit, 0, sizeof(edit));
                edit.text = field->text;
                edit.text_size = field->text_size;
                edit.cursor_position = &state->cursor;
                edit.max_codepoints = field->max_codepoints;
                edit.filter = field->filter;
                edit.filter_user_data = field->filter_user_data;
                changed |= ui_text_paste_clipboard(edit, 0);
            }
            state->anchor = state->cursor;
            selection_changed = 1;
            start = end = state->cursor;
        }
        if(IsKeyPressed(KEY_LEFT) || IsKeyPressed(KEY_RIGHT) ||
           IsKeyPressed(KEY_HOME) || IsKeyPressed(KEY_END)) {
            int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
            int cursor = state->cursor;

            if(IsKeyPressed(KEY_HOME))
                cursor = 0;
            else if(IsKeyPressed(KEY_END))
                cursor = (int)strlen(field->text);
            else if(IsKeyPressed(KEY_LEFT))
                cursor = ui_utf8_prev_offset(field->text, cursor);
            else
                cursor = ui_utf8_next_offset(field->text, cursor);
            state->cursor = cursor;
            if(!shift)
                state->anchor = cursor;
            selection_changed = 1;
            start = state->anchor < state->cursor
                ? state->anchor : state->cursor;
            end = state->anchor > state->cursor
                ? state->anchor : state->cursor;
        }
        codepoint = GetCharPressed();
        while(codepoint > 0) {
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
        if(IsKeyPressed(KEY_BACKSPACE)) {
            if(end > start)
                changed |= ui_text_delete_range(field->text, field->text_size,
                                                 &state->cursor, start, end);
            else if(state->cursor > 0)
                changed |= ui_text_delete_range(
                    field->text, field->text_size, &state->cursor,
                    ui_utf8_prev_offset(field->text, state->cursor),
                    state->cursor);
            state->anchor = state->cursor;
            selection_changed = changed;
        } else if(IsKeyPressed(KEY_DELETE)) {
            if(end > start)
                changed |= ui_text_delete_range(field->text, field->text_size,
                                                 &state->cursor, start, end);
            else
                changed |= ui_text_delete_range(
                    field->text, field->text_size, &state->cursor,
                    state->cursor,
                    ui_utf8_next_offset(field->text, state->cursor));
            state->anchor = state->cursor;
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

static void
DrawTree(void)
{
    int i;

    if(!IsWindowReady())
        return;
    if((ui_tree_invalid & UI_INVALIDATE_PAINT) == 0)
        return;
    for(i = 0; i < ui_committed_node_count; i++) {
        UIWidgetNode *node = &ui_committed_nodes[i];

        switch(node->kind) {
        case UI_WIDGET_BACKGROUND_NODE:
            DrawRectangleRec(node->bounds, node->data.primitive.color);
            break;
        case UI_WIDGET_TEXT_NODE:
            if((node->flags & UI_NODE_PAINTED_IMMEDIATE) != 0)
                break;
            ui_draw_text_with_font_token(
                node->owned_text != NULL ? node->owned_text : "",
                (int)node->bounds.x, (int)node->bounds.y,
                node->data.primitive.font, node->data.primitive.color,
                node->data.primitive.font_token);
            break;
        case UI_WIDGET_RECT_NODE:
            DrawRectangleRec(node->bounds, node->data.primitive.color);
            if(node->data.primitive.border.a != 0)
                DrawRectangleLinesEx(node->bounds, 1.0f,
                                     node->data.primitive.border);
            break;
        case UI_WIDGET_LINE_NODE:
            DrawLine((int)node->bounds.x, (int)node->bounds.y,
                     node->data.primitive.x2, node->data.primitive.y2,
                     node->data.primitive.color);
            break;
        case UI_WIDGET_BUTTON_NODE: {
            ButtonSpec spec = node->data.button.spec;
            Color background;
            Color border;
            Color text;
            int font;
            int hovered;

            if((node->flags & UI_NODE_PAINTED_IMMEDIATE) != 0)
                break;
            spec.bounds = node->bounds;
            spec.label = node->owned_text != NULL ? node->owned_text : "";
            font = spec.font > 0 ? spec.font : GetUIFontSize();
            background = spec.background.a != 0 ? spec.background : c_button;
            hovered = (node->flags & UI_NODE_HOVERED) != 0;
            if(hovered)
                background = spec.hover_background.a != 0
                    ? spec.hover_background : c_button_hover;
            if(spec.disabled && background.a > 120)
                background.a = 120;
            border = spec.border.a != 0 ? spec.border
                                         : LightenUIColor(background, 32);
            text = spec.text.a != 0 ? spec.text : c_text;
            if(spec.disabled && text.a > 150)
                text.a = 150;
            ui_draw_control_background(spec.bounds, background, border,
                                       spec.radius > 0.0f ? spec.radius : 0.06f);
            DrawCenteredUIControlText(spec.label,
                (int)(spec.bounds.x + spec.bounds.width * 0.5f),
                (int)(spec.bounds.y + spec.bounds.height * 0.5f), font, text);
            break;
        }
        case UI_WIDGET_TEXT_AREA_NODE: {
            TextFieldState *state = node->state;
            int cursor = state != NULL ? state->cursor : 0;
            int anchor = state != NULL ? state->anchor : cursor;

            node->data.text_area.bounds = node->bounds;
            ui_paint_text_area(node->data.text_area, cursor,
                state != NULL ? state->focused : 0,
                anchor < cursor ? anchor : cursor,
                anchor > cursor ? anchor : cursor);
            break;
        }
        case UI_WIDGET_TEXT_FIELD_NODE: {
            TextFieldProps field;
            TextFieldState *state = node->state;
            const char *display;
            char *masked = NULL;

            memset(&field, 0, sizeof(field));
            if(node->kind == UI_WIDGET_TEXT_FIELD_NODE) {
                field = node->data.text_field;
            } else {
                TextAreaProps area = node->data.text_area;

                field.bounds = area.bounds;
                field.text = area.text;
                field.text_size = area.text_size;
                field.cursor_position = area.cursor_position;
                field.focused = area.focused;
                field.max_codepoints = area.max_codepoints;
                field.font = area.font;
                field.focus_id = area.focus_id;
                field.style = area.style;
                field.filter = area.filter;
                field.filter_user_data = area.filter_user_data;
                field.read_only = area.read_only;
            }
            display = field.text != NULL ? field.text : "";

            if(field.secure) {
                size_t length = strlen(display);

                masked = malloc(length + 1);
                if(masked != NULL) {
                    memset(masked, '*', length);
                    masked[length] = '\0';
                    display = masked;
                }
            }
            ui_draw_text_input_selection(
                node->bounds, display, state != NULL ? state->cursor : 0,
                state != NULL ? state->focused : 0, field.font, field.style,
                state != NULL && state->anchor < state->cursor
                    ? state->anchor : (state != NULL ? state->cursor : 0),
                state != NULL && state->anchor > state->cursor
                    ? state->anchor : (state != NULL ? state->cursor : 0));
            free(masked);
            break;
        }
        default:
            break;
        }
    }
    ui_tree_invalid &= ~UI_INVALIDATE_PAINT;
}

void
Overlays(void)
{
    if(!IsWindowReady())
        return;
    DrawUIFrameOverlays();
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
        if(nodes[i].bounds.width <= 0 || nodes[i].bounds.height <= 0)
            continue;
        if(CheckCollisionPointRec(point, nodes[i].bounds))
            return i;
    }
    return -1;
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
NodeLabelTextField(LabelTextFieldProps row, int x, int y, int w)
{
    UIWidgetNode node;

    node = ui_node(row.field.focus_id, UI_WIDGET_LABEL_TEXT_FIELD_NODE,
                   (Rectangle){x, y, w, 0});
    node.data.label_text_field = row;
    return node;
}

UIWidgetNode
NodeSectionLabel(SectionLabelProps label, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_SECTION_LABEL_NODE, (Rectangle){x, y, 0, 0});
    node.data.section_label = label;
    return node;
}

UIWidgetNode
NodeCheckboxRow(CheckboxRowProps row, int x, int y)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_CHECKBOX_ROW_NODE, (Rectangle){x, y, 0, 0});
    node.data.checkbox_row = row;
    return node;
}

UIWidgetNode
NodeButtonRow(ButtonRowProps row)
{
    UIWidgetNode node;

    node = ui_node(0, UI_WIDGET_BUTTON_ROW_NODE,
                   (Rectangle){row.x, row.y, row.width, 0});
    node.data.button_row = row;
    return node;
}

UIWidgetNode
NodeBottomNav(BottomNavProps nav)
{
    UIWidgetNode node;
    int height;

    height = nav.height > 0 ? nav.height : 0;
    node = ui_node(0, UI_WIDGET_BOTTOM_NAV_NODE,
                   (Rectangle){0, 0, nav.view_width, height});
    return node;
}

UIWidgetNode
NodeTopNav(TopNavProps nav)
{
    return ui_node(nav.id, UI_WIDGET_CUSTOM_NODE,
                   (Rectangle){nav.x, nav.y, nav.width, nav.height});
}

UIWidgetNode
NodeTabBar(TabBarProps bar)
{
    return ui_node(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds);
}

UIWidgetNode
NodeThemeSettings(ThemeSettingsProps settings)
{
    UIWidgetNode node;

    node = ui_node(settings.id_base, UI_WIDGET_THEME_SETTINGS_NODE,
                   (Rectangle){settings.x, settings.y, settings.w, 0});
    node.data.theme_settings = settings;
    return node;
}

UIWidgetNode
NodeThemePicker(int x, int y, int w)
{
    return ui_node(0, UI_WIDGET_THEME_PICKER_NODE, (Rectangle){x, y, w, 0});
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
Picture(PictureProps picture)
{
    Texture2D texture;

    ui_tree_add(0, UI_WIDGET_PICTURE_NODE, picture.bounds, picture.asset_path);
    texture = LoadPictureTexture(picture.asset_path);
    if(texture.id == 0) {
        DrawRectangleRec(picture.bounds, GetThemeSurface());
        DrawRectangleLinesEx(picture.bounds, 1.0f, GetThemeButtonHover());
        DrawUIText("Missing image", (int)picture.bounds.x + ScaleUIPx(8),
                   (int)picture.bounds.y + ScaleUIPx(8), Text12,
                   GetThemeIcon());
        return;
    }
    PictureTexture(texture, picture);
}

void
Background(Color color)
{
    NodeId node = ui_tree_add(0, UI_WIDGET_BACKGROUND_NODE,
                                (Rectangle){0, 0, ui_view_width,
                                            ui_view_height}, NULL);

    if(node >= 0)
        ui_tree_nodes[node].data.primitive.color = color;
    if(ui_tree_building)
        return;
    DrawRectangleRec((Rectangle){0, 0, GetUIViewWidth(), GetUIViewHeight()},
                     color);
}

void
Text(const char *text, int x, int y, int font_size, Color color)
{
    NodeId node = ui_tree_add(0, UI_WIDGET_TEXT_NODE,
                                (Rectangle){x, y, 0,
                                    TextHeight(text, font_size)}, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].owned_text = ui_tree_strdup(text);
        ui_tree_nodes[node].data.primitive.font = font_size;
        ui_tree_nodes[node].data.primitive.font_token = ui_active_font_token();
        ui_tree_nodes[node].data.primitive.color = color;
    }
    DrawUIText(text, x, y, font_size, color);
    if(ui_tree_building && node >= 0)
        ui_tree_nodes[node].flags |= UI_NODE_PAINTED_IMMEDIATE;
}

void
TextInRect(const char *text, Rectangle rect, int font_size, Color color)
{
    ui_tree_add(0, UI_WIDGET_TEXT_NODE, rect, text);
    DrawUITextInRect(text, rect, font_size, color);
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
TextLines(const char **lines, int count, int x, int *y, int font,
                int line_h, Color color)
{
    int start_y = y != NULL ? *y : 0;

    ui_tree_add(0, UI_WIDGET_TEXT_NODE,
                (Rectangle){x, start_y, 0, count * line_h}, lines);
    DrawUITextLines(lines, count, x, y, font, line_h, color);
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
Line(int x1, int y1, int x2, int y2, Color color)
{
    int x = x1 < x2 ? x1 : x2;
    int y = y1 < y2 ? y1 : y2;
    int w = abs(x2 - x1);
    int h = abs(y2 - y1);

    NodeId node = ui_tree_add(0, UI_WIDGET_LINE_NODE,
                                (Rectangle){x, y, w, h}, NULL);

    if(node >= 0) {
        ui_tree_nodes[node].data.primitive.x2 = x2;
        ui_tree_nodes[node].data.primitive.y2 = y2;
        ui_tree_nodes[node].data.primitive.color = color;
    }
    if(ui_tree_building)
        return;
    DrawLine(x1, y1, x2, y2, color);
}

void
Bevel(int x, int y, int w, int h, Color light, Color dark)
{
    ui_tree_add(0, UI_WIDGET_RECT_NODE, (Rectangle){x, y, w, h}, NULL);
    DrawUIBevel(x, y, w, h, light, dark);
}

int
ButtonNode(ButtonSpec button)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                       &button);
    clicked = RenderButton(button);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
IconButton(IconButtonProps button)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(button.focus_id, UI_WIDGET_BUTTON_NODE, button.bounds,
                       &button);
    clicked = DrawUIIconButton(button);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
Href(HrefProps link)
{
    if(link.bounds.height <= 0)
        link.bounds.height = TextHeight(link.text, link.font);
    ui_tree_add(link.focus_id, UI_WIDGET_TEXT_NODE, link.bounds, &link);
    return DrawUIHref(link);
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
    return RenderTextField(field);
}

int
IconBtn(int id, int x, int y, UIIconSize size, Texture2D icon,
              int *hover)
{
    int s = GetUIIconButtonSize(size);
    NodeId node;
    int clicked;

    node = ui_tree_add(id, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, s, s},
                       hover);
    clicked = DrawUIIconBtn(x, y, size, icon, hover);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
PaddedIconBtn(int id, int x, int y, int size, int padding,
                    Texture2D icon, int *hover)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                       (Rectangle){x, y, size + padding * 2,
                                   size + padding * 2},
                       hover);
    clicked = DrawUIPaddedIconBtn(x, y, size, padding, icon, hover);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
StyledButton(int x, int y, int w, int h, const char *label,
             ButtonStyle style, int disabled, int *hover)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(0, UI_WIDGET_BUTTON_NODE, (Rectangle){x, y, w, h},
                       hover);
    clicked = RenderStyledButton(x, y, w, h, label, style, disabled, hover);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
InfoButton(int id, int center_x, int center_y, int diameter)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                       (Rectangle){center_x - diameter / 2,
                                   center_y - diameter / 2,
                                   diameter, diameter}, NULL);
    clicked = DrawUIInfoButton(center_x, center_y, diameter);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

void
IconLink(int id, int x, int y, int icon_size, Texture2D icon,
               const char *url)
{
    NodeId node;

    node = ui_tree_add(id, UI_WIDGET_BUTTON_NODE,
                       (Rectangle){x, y, icon_size, icon_size}, url);
    DrawUIIconLink(x, y, icon_size, icon, url);
    ui_tree_mark_painted_immediate(node);
}

void
IconTexture(int id, int x, int y, int size, Texture2D icon, Color tint)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, size, size},
                NULL);
    DrawUIIconTexture(x, y, size, icon, tint);
}

int
Dropdown(int id, int x, int y, int w, int h,
               const char **options, int option_count, int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return DrawUIDropdown(id, x, y, w, h, options, option_count,
                          selected_index);
}

int
DropdownEx(int id, int x, int y, int w, int h,
                 const UIDropdownOption *options, int option_count,
                 int *selected_index)
{
    ui_tree_add(id, UI_WIDGET_DROPDOWN_NODE, (Rectangle){x, y, w, h},
                selected_index);
    return DrawUIDropdownEx(id, x, y, w, h, options, option_count,
                            selected_index);
}

int
Slider(int id, int x, int y, int w, const char *label,
             int min, int max, int *value, const char *suffix,
             const char *value_text_override)
{
    ui_tree_add(id, UI_WIDGET_SLIDER_NODE,
                (Rectangle){x, y, w, ScaleUIPx(56)}, value);
    return DrawUISlider(id, x, y, w, label, min, max, value, suffix,
                        value_text_override);
}

int
Toggle(int id, int x, int y, int w, int h, int *value,
             const char *off_label, const char *on_label)
{
    ui_tree_add(id, UI_WIDGET_TOGGLE_NODE, (Rectangle){x, y, w, h}, value);
    (void)id;
    return DrawUIToggleSwitch(x, y, w, h, value, off_label, on_label);
}

int
Checkbox(int id, int x, int y, const char *label, int *value)
{
    ui_tree_add(id, UI_WIDGET_CHECKBOX_NODE, (Rectangle){x, y, 0, 0}, value);
    (void)id;
    return DrawUICheckboxToggle(x, y, label, value);
}

int
ThemeSettings(ThemeSettingsProps settings, UIThemeSettingsState *state,
                    UIThemeSettingsResult *result)
{
    UIWidgetNode node;
    NodeId id;
    UIThemeSettingsResult next = {0};

    id = ui_tree_add(settings.id_base, UI_WIDGET_THEME_SETTINGS_NODE,
                     (Rectangle){settings.x, settings.y, settings.w, 0},
                     NULL);
    node = NodeThemeSettings(settings);
    ui_tree_store_node(id, node);
    DrawUIThemeSettings(settings, state);
    next = DrawUIThemeSettingsMenus(settings, state);
    if(result != NULL)
        *result = next;
    return next.changed;
}

void
Separator(Rectangle bounds, int vertical)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUISeparator(bounds, vertical);
}

MenuBarResult
MenuBar(int id, Rectangle bounds, const Menu *menus,
              int menu_count, int *open_index)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, bounds, open_index);
    return DrawUIMenuBar(id, bounds, menus, menu_count, open_index);
}

int
PopupMenu(int id, int x, int y, const MenuItem *items,
                int item_count)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, 0, 0}, items);
    return DrawUIPopupMenu(id, x, y, items, item_count);
}

int
ContextMenu(ContextMenuProps menu)
{
    ui_tree_add(menu.id, UI_WIDGET_CUSTOM_NODE, menu.trigger, &menu);
    return DrawUIContextMenu(menu);
}

int
Radio(RadioButtonProps radio)
{
    ui_tree_add(radio.id, UI_WIDGET_CUSTOM_NODE, radio.bounds, &radio);
    return DrawUIRadioButton(radio);
}

void
Progress(ProgressBarProps progress)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, progress.bounds, &progress);
    DrawUIProgressBar(progress);
}

int
Spinbox(SpinboxProps spinbox)
{
    ui_tree_add(spinbox.id, UI_WIDGET_CUSTOM_NODE, spinbox.bounds, &spinbox);
    return DrawUISpinbox(spinbox);
}

int
Combobox(ComboboxProps combo)
{
    ui_tree_add(combo.id, UI_WIDGET_DROPDOWN_NODE, combo.bounds, &combo);
    return DrawUICombobox(combo);
}

void
LabelFrame(LabelFrameProps frame)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, frame.bounds, &frame);
    DrawUILabelFrame(frame);
}

void
ImageBox(ImageBoxProps image)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, image.bounds, &image);
    DrawUIImageBox(image);
}

int
ListBox(ListBoxProps list)
{
    ui_tree_add(list.id, UI_WIDGET_CUSTOM_NODE, list.bounds, &list);
    return DrawUIListBox(list);
}

int
TreeView(TreeViewProps tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return DrawUITreeView(tree);
}

int
CascadingTreeView(CascadingTreeViewProps tree)
{
    ui_tree_add(tree.id, UI_WIDGET_CUSTOM_NODE, tree.bounds, &tree);
    return DrawUICascadingTreeView(tree);
}

int
SourceView(SourceViewProps source)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, source.bounds, &source);
    return DrawUISourceView(source);
}

int
TableView(TableViewProps table)
{
    ui_tree_add(table.id, UI_WIDGET_CUSTOM_NODE, table.bounds, &table);
    return DrawUITableView(table);
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
    if(ui_tree_building)
        return 0;
    return RenderTextArea(area);
}

void
CanvasGrid(Rectangle bounds, int step, Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUICanvasGrid(bounds, step, color);
}

int
Notebook(NotebookProps notebook)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, notebook.bounds, &notebook);
    return DrawUINotebook(notebook);
}

int
PanedView(PanedViewProps panes)
{
    ui_tree_add(panes.id, UI_WIDGET_CUSTOM_NODE, panes.bounds, &panes);
    return DrawUIPanedView(panes);
}

int
Collapsible(CollapsibleProps section)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, section.bounds, &section);
    return DrawUICollapsible(section);
}

int
ColorPicker(Rectangle bounds, Color *color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, color);
    return DrawUIColorPicker(bounds, color);
}

int
ActionModal(ModalProps modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIActionModal(modal);
}

int
MessageDialog(MessageDialogProps dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIMessageDialog(dialog);
}

int
ConfirmDialog(ConfirmDialogProps dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIConfirmDialog(dialog);
}

int
PromptDialog(PromptDialogProps dialog)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &dialog);
    return DrawUIPromptDialog(dialog);
}

int
TextPopover(TextPopoverProps popover)
{
    ui_tree_add(popover.id, UI_WIDGET_CUSTOM_NODE, popover.anchor, &popover);
    return DrawUITextPopover(popover);
}

int
PickerDialog(PickerDialogProps picker)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &picker);
    return DrawUIPickerDialog(picker);
}

void
Focus(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUIFocus(bounds);
}

void
FocusDebugOverlay(const UIAccessibilityNode *nodes, int count)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, ui_view_width, ui_view_height}, nodes);
    DrawUIFocusDebugOverlay(nodes, count);
}

UIGuideResult
GuideOverlay(GuideOverlayProps guide)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, guide.view_width, guide.view_height}, &guide);
    return DrawUIGuideOverlay(guide);
}

int
ThemeSwitcher(int x, int y, int w, const char *label,
                    const char *light_label, const char *dark_label,
                    int *theme_id, int *dark_mode)
{
    ui_tree_add(0, UI_WIDGET_THEME_SETTINGS_NODE,
                (Rectangle){x, y, w, ScaleUIPx(58)}, theme_id);
    return DrawUIThemeSwitcher(x, y, w, label, light_label, dark_label,
                               theme_id, dark_mode);
}

int
ThemePicker(int x, int y, int w, int dark_mode, int *theme_id)
{
    ui_tree_add(0, UI_WIDGET_THEME_PICKER_NODE,
                (Rectangle){x, y, w, 0}, theme_id);
    return DrawUIThemePicker(x, y, w, dark_mode, theme_id);
}

void
TutorialImagePlaceholder(const char *label, int x, int y, int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, label);
    DrawUITutorialImagePlaceholder(label, x, y, w, h);
}

void
TutorialImage(Texture2D texture, const char *fallback, int x, int y,
                    int w, int h)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, fallback);
    DrawUITutorialImage(texture, fallback, x, y, w, h);
}

void
TransitionFade(const UITransition *transition, int width, int height,
                     Color color)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height},
                transition);
    DrawUITransitionFade(transition, width, height, color);
}

void
InfoRows(InfoRowsProps rows)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){rows.x, rows.y, rows.width,
                            rows.row_height * rows.row_count}, &rows);
    DrawUIInfoRows(rows);
}

int
LabelTextField(LabelTextFieldProps row, int x, int y, int w)
{
    UIWidgetNode node;
    NodeId id;

    id = ui_tree_add(row.field.focus_id, UI_WIDGET_LABEL_TEXT_FIELD_NODE,
                     (Rectangle){x, y, w, 0}, NULL);
    node = NodeLabelTextField(row, x, y, w);
    ui_tree_store_node(id, node);
    return DrawUILabelTextField(row, x, y, w);
}

int
SectionLabel(SectionLabelProps label, int x, int y)
{
    UIWidgetNode node;
    NodeId id;

    id = ui_tree_add(0, UI_WIDGET_SECTION_LABEL_NODE,
                     (Rectangle){x, y, 0, 0}, NULL);
    node = NodeSectionLabel(label, x, y);
    ui_tree_store_node(id, node);
    return DrawUISectionLabel(label, x, y);
}

int
CheckboxRow(CheckboxRowProps row, int x, int y)
{
    UIWidgetNode node;
    NodeId id;

    id = ui_tree_add(0, UI_WIDGET_CHECKBOX_ROW_NODE,
                     (Rectangle){x, y, 0, 0}, NULL);
    node = NodeCheckboxRow(row, x, y);
    ui_tree_store_node(id, node);
    return DrawUICheckboxRow(row, x, y);
}

int
OverlayButton(OverlayButtonProps button)
{
    NodeId node;
    int clicked;

    node = ui_tree_add(0, UI_WIDGET_BUTTON_NODE, button.bounds, &button);
    clicked = DrawUIOverlayButton(button);
    ui_tree_mark_painted_immediate(node);
    return clicked;
}

int
ButtonRow(ButtonRowProps row)
{
    UIWidgetNode node;
    NodeId id;

    id = ui_tree_add(0, UI_WIDGET_BUTTON_ROW_NODE,
                     (Rectangle){row.x, row.y, row.width, 0}, NULL);
    node = NodeButtonRow(row);
    ui_tree_store_node(id, node);
    return DrawUIButtonRow(row);
}

int
IconSliderPopup(IconSliderPopupProps popup)
{
    ui_tree_add(popup.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){popup.x, popup.y, 0, 0}, &popup);
    return DrawUIIconSliderPopup(popup);
}

IconRowResult
BottomIconRow(BottomIconRowProps row)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, row.view_width, row.view_height}, &row);
    return DrawUIBottomIconRow(row);
}

BottomNavResult
BottomNav(BottomNavProps nav)
{
    ui_tree_add(0, UI_WIDGET_BOTTOM_NAV_NODE,
                (Rectangle){0, 0, nav.view_width, nav.view_height}, &nav);
    return DrawUIBottomNav(nav);
}

BottomNavConfigResult
BottomNavConfig(BottomNavConfigProps modal)
{
    ui_tree_add(modal.id, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIBottomNavConfigModal(modal);
}

TopNavResult
TopNav(TopNavProps nav)
{
    ui_tree_add(nav.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){nav.x, nav.y, nav.width, nav.height}, &nav);
    return DrawUITopNav(nav);
}

ToolbarResult
Toolbar(ToolbarProps toolbar)
{
    ui_tree_add(toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){toolbar.x, toolbar.y, toolbar.width, toolbar.height}, &toolbar);
    return DrawUIToolbar(toolbar);
}

ToolbarHeaderResult
ToolbarHeader(ToolbarHeaderProps header)
{
    ui_tree_add(header.toolbar.id, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){0, 0, header.toolbar.width, header.toolbar.height}, &header);
    return DrawUIToolbarHeader(header);
}

int
SubtabBar(SubtabBarProps bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return DrawUISubtabBar(bar);
}

int
TabBar(TabBarProps bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return DrawUITabBar(bar);
}

PaneTabBarResult
PaneTabs(PaneTabBar bar)
{
    ui_tree_add(0, UI_WIDGET_TAB_BAR_NODE, bar.bounds, &bar);
    return DrawUIPaneTabBar(bar);
}

void
PaneDropPreview(Rectangle bounds, PaneDropZone zone)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, &zone);
    DrawUIPaneDropPreview(bounds, zone);
}

SidebarAccountHeaderResult
SidebarAccountHeader(SidebarAccountHeaderProps header)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE,
                (Rectangle){header.x, header.y, header.width, header.height}, &header);
    return DrawUISidebarAccountHeader(header);
}

ProfilePicturePickerResult
ProfilePicturePicker(ProfilePicturePickerProps modal)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, &modal);
    return DrawUIProfilePicturePickerModal(modal);
}

void
ReorderHandle(int id, int x, int y, int w, int h, int active)
{
    ui_tree_add(id, UI_WIDGET_CUSTOM_NODE, (Rectangle){x, y, w, h}, NULL);
    DrawUIReorderHandle(x, y, w, h, active);
}

void
ReorderPlaceholder(Rectangle bounds)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, bounds, NULL);
    DrawUIReorderPlaceholder(bounds);
}

int
Modal(const char *title, const char *message,
            const char *cancel_btn, const char *confirm_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return DrawUIModal(title, message, cancel_btn, confirm_btn);
}

int
Modal3Button(const char *title, const char *message,
                   const char *left_btn, const char *middle_btn,
                   const char *right_btn)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, 0, 0}, title);
    return DrawUIModal3Button(title, message, left_btn, middle_btn, right_btn);
}

void
TitleBar(const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    DrawUITitleBar(title, height);
}

int
ReturnTitleBar(Texture2D return_icon, const char *title, int height)
{
    ui_tree_add(0, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, title);
    return DrawUIReturnTitleBar(return_icon, title, height);
}

int
ReturnDropdownTitleBar(Texture2D return_icon,
                             UITitleBarDropdown dropdown, int height)
{
    ui_tree_add(dropdown.id, UI_WIDGET_TITLE_BAR_NODE,
                (Rectangle){0, 0, ui_view_width, height}, &dropdown);
    return DrawUIReturnDropdownTitleBar(return_icon, dropdown, height);
}

UIPanelFrame
ModalFrame(int width, int height, const char *title,
                 Texture2D left_icon, Texture2D right_icon)
{
    ui_tree_add(0, UI_WIDGET_CUSTOM_NODE, (Rectangle){0, 0, width, height}, title);
    return DrawUIModalFrame(width, height, title, left_icon, right_icon);
}

int
Button(ButtonProps button)
{
    ButtonSpec spec = {
        .bounds = button.bounds,
        .label = button.label,
        .font = button.font,
        .focus_id = button.id,
        .disabled = button.disabled
    };
    NodeId node = ui_tree_add(button.id, UI_WIDGET_BUTTON_NODE,
                                button.bounds, NULL);
    int clicked;

    if(spec.font <= 0)
        spec.font = GetUIFontSize();
    if(node >= 0) {
        ui_tree_nodes[node].owned_text = ui_tree_strdup(button.label);
        ui_tree_nodes[node].data.button.spec = spec;
        ui_tree_nodes[node].data.button.spec.label =
            ui_tree_nodes[node].owned_text;
        ui_tree_nodes[node].data.button.style = button.style;
    }
    clicked = RenderButton(spec);
    ui_tree_mark_painted_immediate(node);
    return clicked;
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
GridLayout(GridLayoutProps props)
{
    NodeId node = ui_begin_layout_node(UI_WIDGET_GRID_NODE, props.key,
                                       props.bounds, props.gap,
                                       props.padding);

    if(node >= 0)
        ui_tree_nodes[node].data.layout.columns =
            props.columns > 0 ? props.columns : 1;
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

#ifndef KRYON_TREE_H
#define KRYON_TREE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "kryon_compat.generated.h"
#include "kryon_key.h"
#include "ui_controls.h"
#include "ui_button_props.generated.h"
#include "ui_card_props.generated.h"
#include "ui_checkbox_props.generated.h"
#include "ui_collapsible_props.generated.h"
#include "ui_color_picker_props.generated.h"
#include "ui_drawing_props.generated.h"
#include "ui_drag_drop_props.generated.h"
#include "ui_drag_props.generated.h"
#include "ui_dropdown_props.generated.h"
#include "ui_fieldset_props.generated.h"
#include "ui_grid_props.generated.h"
#include "ui_input_props.generated.h"
#include "ui_layout_props.generated.h"
#include "ui_list_box_props.generated.h"
#include "ui_plot_props.generated.h"
#include "ui_progress_props.generated.h"
#include "ui_radio_props.generated.h"
#include "ui_separator_props.generated.h"
#include "ui_selectable_props.generated.h"
#include "ui_slider_props.generated.h"
#include "ui_spinbox_props.generated.h"
#include "ui_table_view_props.generated.h"
#include "ui_toggle_props.generated.h"
#include "ui_tree_view_props.generated.h"
#include "ui_modal_props.generated.h"
#include "ui_title_bar_props.generated.h"
#include "ui_nav.h"
#include "ui_profile.h"
#include "ui_image_props.generated.h"
#include "ui_router_props.generated.h"
#include "ui_accessibility_node.generated.h"
#include "ui_menu_props.generated.h"
#include "ui_accessibility_props.generated.h"

struct TransitionState;

typedef int NodeId;

typedef enum EventKind {
    EVENT_NONE = 0,
    EVENT_CLICK,
    EVENT_VALUE_CHANGED,
    EVENT_TEXT_CHANGED,
    EVENT_TEXT_COMMIT,
    EVENT_SELECTION_CHANGED,
    EVENT_COMPOSITION_CHANGED,
    EVENT_FOCUS,
    EVENT_BLUR
} EventKind;

typedef struct Event {
    KeyID key;
    EventKind kind;
    double timestamp;
    union {
        int value;
        struct { int start, end; } selection;
        struct { int bytes; } text;
    } data;
} Event;

typedef enum Invalidation {
    INVALIDATE_NONE = 0,
    INVALIDATE_PAINT = 1 << 0,
    INVALIDATE_LAYOUT = 1 << 1,
    INVALIDATE_TREE = 1 << 2
} Invalidation;

typedef struct TreeNode TreeNode;

typedef void (*AccessibilitySink)(const AccessibilityNode *nodes,
                                    int count, void *userdata);

/* BeginTree starts a declaration pass. EndTree atomically reconciles, lays out,
 * routes, updates, and paints it. Every container closes with End(). */
void BeginTree(KeyID screen_key);
void EndTree(void);
void End(void);
KeyID Key(const char *text);
void InvalidateTree(Invalidation invalidation);
int NextEvent(Event *event);
int SetSelection(KeyID key, int anchor, int cursor);
void ReconcileTree(void);
void LayoutTree(void);
void RouteInput(void);
void UpdateTree(void);
void Overlays(void);
const TreeNode *GetTreeNodes(int *count);
int GetNodeId(const TreeNode *node);
int GetNodeKind(const TreeNode *node);
const char *GetNodeKindName(int kind);
Rectangle GetNodeBounds(const TreeNode *node);
int GetNodeParent(const TreeNode *node);
int GetNodeFirstChild(const TreeNode *node);
int GetNodeNextSibling(const TreeNode *node);
int GetNodeHeightById(int id);
const TreeNode *GetNode(NodeId id);
NodeId HitTestNode(Vector2 point);
int GetAccessibilitySnapshot(AccessibilityNode *nodes, int capacity);
void SetAccessibilitySink(AccessibilitySink sink, void *userdata);
int QueueAccessibilityAction(int focus_id, uint64_t generation,
                             AccessibilityAction action);
int QueueAccessibilityValue(int focus_id, uint64_t generation, const char *value);
int QueueAccessibilitySelection(int focus_id, uint64_t generation,
                                int anchor, int cursor);
int QueueAccessibilityItem(int focus_id, uint64_t generation, int index, int selected);

void AppBackground(void);
void Background(Color color);
void Surface(Rectangle bounds, Style style);
int Card(CardProps card);
void Text(TextProps props);
void Paragraph(ParagraphSpec paragraph, int x, int *y);
void Box(Rectangle bounds, Color fill, Color border);
void Circle(int center_x, int center_y, int radius, Color color);
void Ring(int center_x, int center_y, int inner_radius, int outer_radius,
          Color color);
void Line(int x1, int y1, int x2, int y2, Color color);
void Triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color);
void Bevel(int x, int y, int w, int h, Color light, Color dark);
void Icon(int id, int x, int y, int size, IconType icon, Color tint);
int TextField(TextFieldProps field);
int Dropdown(DropdownProps dropdown);
int Toggle(ToggleProps toggle);
int Checkbox(CheckboxProps checkbox);
void Separator(SeparatorProps separator);
int DragDrop(DragDropProps drag_drop);
int Radio(RadioProps radio);
void Progress(ProgressProps progress);
void Plot(PlotProps plot);
int Drag(DragProps drag);
int Input(InputProps input);
int Slider(SliderProps slider);
int Spinbox(SpinboxProps spinbox);
void Fieldset(FieldsetProps frame);
int ListBox(ListBoxProps list);
int TreeView(TreeViewProps tree);
int TableView(TableViewProps table);
int TextArea(TextAreaProps area);
void CanvasGrid(Rectangle bounds, int step, Color color);
int PanedView(PanedViewProps panes);
int Collapsible(CollapsibleProps section);
int ColorPicker(ColorPickerProps picker);
void Focus(Rectangle bounds);
NavigationBarResult NavigationBar(NavigationBarProps nav);
ToolbarResult Toolbar(ToolbarProps toolbar);
int TabBar(TabBarProps bar);
int Modal(ModalProps modal);
int TitleBar(TitleBarProps title_bar);

int Button(ButtonProps button);
int Selectable(SelectableProps selectable);
void Bullet(Rectangle bounds);
/* Layout nodes: auto-position children like flexbox. */
typedef ColumnProps RowProps;

enum {
    ROUTER_NO_ROUTE = -2147483647
};

void RouterStateInit(RouterState *state, int initial_route);
void RouterNavigate(RouterState *state, int route_id);
int RouterSetRoute(RouterProps props, int route_id, int push);
const RouterRoute *RouterFindRoute(const RouterRoute *routes, int route_count,
                                   int route_id);
RouterResult Router(RouterProps props);
NodeId Column(ColumnProps props);
NodeId Row(RowProps props);
NodeId Grid(GridProps props);
NodeId Group(ColumnProps props);
NodeId Stack(ColumnProps props);
NodeId Screen(ColumnProps props);

#ifdef __cplusplus
}
#endif

#endif

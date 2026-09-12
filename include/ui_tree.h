#ifndef UI_TREE_H
#define UI_TREE_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_button_props.generated.h"
#include "ui_card_props.generated.h"
#include "ui_grid_props.generated.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_overlay.h"
#include "ui_profile.h"
#include "ui_rows.h"
#include "ui_image.h"
#include "ui_tk.h"

struct UITransition;

typedef int NodeId;
typedef unsigned long long KeyID;

typedef enum UIEventKind {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLICK,
    UI_EVENT_VALUE_CHANGED,
    UI_EVENT_TEXT_CHANGED,
    UI_EVENT_TEXT_COMMIT,
    UI_EVENT_SELECTION_CHANGED,
    UI_EVENT_COMPOSITION_CHANGED,
    UI_EVENT_FOCUS,
    UI_EVENT_BLUR
} UIEventKind;

typedef struct UIEvent {
    KeyID key;
    UIEventKind kind;
    double timestamp;
    union {
        int value;
        struct { int start, end; } selection;
        struct { int bytes; } text;
    } data;
} UIEvent;

typedef enum UIInvalidation {
    UI_INVALIDATE_NONE = 0,
    UI_INVALIDATE_PAINT = 1 << 0,
    UI_INVALIDATE_LAYOUT = 1 << 1,
    UI_INVALIDATE_TREE = 1 << 2
} UIInvalidation;

typedef enum UIWidgetKind {
    UI_WIDGET_SCREEN_NODE,
    UI_WIDGET_BACKGROUND_NODE,
    UI_WIDGET_TEXT_NODE,
    UI_WIDGET_RECT_NODE,
    UI_WIDGET_CIRCLE_NODE,
    UI_WIDGET_RING_NODE,
    UI_WIDGET_LINE_NODE,
    UI_WIDGET_TRIANGLE_NODE,
    UI_WIDGET_BUTTON_NODE,
    UI_WIDGET_TEXT_FIELD_NODE,
    UI_WIDGET_TEXT_AREA_NODE,
    UI_WIDGET_DROPDOWN_NODE,
    UI_WIDGET_SLIDER_NODE,
    UI_WIDGET_TOGGLE_NODE,
    UI_WIDGET_CHECKBOX_NODE,
    UI_WIDGET_PARAGRAPH_NODE,
    UI_WIDGET_READONLY_TEXT_BOX_NODE,
    UI_WIDGET_NAVIGATION_BAR_NODE,
    UI_WIDGET_TAB_BAR_NODE,
    UI_WIDGET_PARAGRAPH_MODAL_NODE,
    UI_WIDGET_TITLE_BAR_NODE,
    UI_WIDGET_GROUP_NODE,
    UI_WIDGET_COLUMN_NODE,
    UI_WIDGET_ROW_NODE,
    UI_WIDGET_STACK_NODE,
    UI_WIDGET_GRID_NODE,
    UI_WIDGET_IMAGE_NODE,
    UI_WIDGET_CUSTOM_NODE,
    UI_WIDGET_DRAG_NODE,
    UI_WIDGET_TEXT_INPUT_PAINT_NODE,
    UI_WIDGET_ROUTER_NODE,
    UI_WIDGET_CARD_NODE
} UIWidgetKind;

/* Prepared painting only: no editing-state pointers survive submission. */
typedef struct UIWidgetTextInputPaint {
    TextInputStyle style;
    int cursor;
    int focused;
    int editable;
    int caret;
    int font;
    int font_token;
    int selection_start;
    int selection_end;
    int composition_start;
    int composition_end;
    int scroll_x;
} UIWidgetTextInputPaint;

typedef union UIWidgetData {
    UIWidgetTextInputPaint text_input_paint;
    struct {
        DragProps props;
        size_t format_offset;
    } drag;
    struct {
        SliderProps props;
        size_t format_offset;
    } slider;
    struct {
        int gap;
        int padding;
        int columns;
        int min_item_width;
        int max_columns;
    } layout;
    ParagraphSpec paragraph;
    ReadonlyTextBoxProps readonly_text_box;
    ParagraphModalMeasureProps paragraph_modal;
    ImageProps image;
    struct {
        int x1;
        int y1;
        int x2;
        int y2;
        int x3;
        int y3;
        int font;
        int font_token;
        int letter_spacing;
        int heading_level;
        int wrap;
        int align;
        int vertical_align;
        Color color;
        Color border;
        int styled;
        Style style;
    } primitive;
    ButtonSpec button;
    TextFieldProps text_field;
    TextAreaProps text_area;
    /* Retained interactive controls. Pointer fields follow the TextField
     * contract: callers keep them valid while the tree is not re-declared,
     * painting reads them live, and retained input routing writes through
     * them. */
    struct {
        int *value;
        const char *off_label;
        const char *on_label;
    } toggle;
    struct {
        int *value;
        const char *label;
    } checkbox;
} UIWidgetData;

typedef struct UIWidgetNode {
    int id;
    KeyID key;
    UIWidgetKind kind;
    Rectangle bounds;
    Rectangle declared_bounds;
    Rectangle input_clip;
    int has_input_clip;
    /* Internal frame-local paint snapshot index; zero means ordinary painting. */
    unsigned paint_capture;
    /* Internal declaration snapshot for deferred popup hit testing. */
    unsigned popup_input_capture;
    /* Internal declaration-time font snapshot for deferred text painting. */
    int font_token;
    int parent;
    int first_child;
    int next_sibling;
    const void *props;
    void *state;
    UIWidgetData data;
    unsigned flags;
    unsigned generation;
    char *owned_text;
} UIWidgetNode;

typedef void (*UIAccessibilitySink)(const UIAccessibilityNode *nodes,
                                    int count, void *userdata);

/* BeginTree starts a declaration pass. EndTree atomically reconciles, lays out,
 * routes, updates, and paints it. Every container closes with End(). */
void BeginTree(KeyID screen_key);
void EndTree(void);
void End(void);
KeyID Key(const char *text);
void InvalidateTree(UIInvalidation invalidation);
int NextEvent(UIEvent *event);
int SetSelection(KeyID key, int anchor, int cursor);
void ReconcileTree(void);
void LayoutTree(void);
void RouteInput(void);
void UpdateTree(void);
void Overlays(void);
const UIWidgetNode *GetTreeNodes(int *count);
int GetNodeHeight(UIWidgetNode node);
int GetNodeHeightById(int id);
const UIWidgetNode *GetNode(NodeId id);
NodeId HitTestNode(Vector2 point);
int GetAccessibilitySnapshot(UIAccessibilityNode *nodes, int capacity);
void SetAccessibilitySink(UIAccessibilitySink sink, void *userdata);

UIWidgetNode NodeParagraph(ParagraphSpec paragraph, int x, int y);
UIWidgetNode NodeReadonlyTextBox(ReadonlyTextBoxProps box);
UIWidgetNode NodeNavigationBar(NavigationBarProps nav);
UIWidgetNode NodeTabBar(TabBarProps bar);
UIWidgetNode NodeParagraphModal(ParagraphModalMeasureProps measure);
UIWidgetNode NodeTitleBar(int height);

Style ResolveButtonStyle(ButtonProps button, ButtonState state);

void BeginDisabled(int disabled);
void EndDisabled(void);
Rectangle BeginScroll(Rectangle bounds, int content_height, int *scroll_offset);
void EndScroll(void);

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *selected;
    int disabled;
} SelectableProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int *value;
    int *flags;
    int flags_value;
    int disabled;
} CheckboxProps;

void Background(Color color);
void Surface(Rectangle bounds, Style style);
int Card(CardProps card);
NodeId BeginCard(CardProps card);
void Text(TextProps props);
void Paragraph(ParagraphSpec paragraph, int x, int *y);
#ifdef KRYON_BACKEND_LIBDRAW
void kry_ui_rect_shape(int x, int y, int w, int h, Color fill, Color border);
#define Rect kry_ui_rect_shape
#else
void Rect(int x, int y, int w, int h, Color fill, Color border);
#endif
void Box(Rectangle bounds, Color fill, Color border);
void Circle(int center_x, int center_y, int radius, Color color);
void Ring(int center_x, int center_y, int inner_radius, int outer_radius,
          Color color);
void Line(int x1, int y1, int x2, int y2, Color color);
void Triangle(int x1, int y1, int x2, int y2, int x3, int y3, Color color);
void Bevel(int x, int y, int w, int h, Color light, Color dark);
void Icon(int id, int x, int y, int size, UIIconType icon, Color tint);
void RenderImage(ImageProps image);
int ButtonNode(ButtonSpec button);
int TextField(TextFieldProps field);
int Dropdown(DropdownProps dropdown);
int Toggle(ToggleProps toggle);
int Checkbox(CheckboxProps checkbox);
void Separator(SeparatorProps separator);
int DragDropSource(DragDropSourceProps source);
int DragDropTarget(DragDropTargetProps target);
int MultiSelectList(MultiSelectListProps list);
MenuBarResult MenuBar(int id, Rectangle bounds, const Menu *menus,
                              int menu_count, int *open_index);
int PopupMenu(int id, int x, int y, const MenuItem *items,
                    int item_count);
int ContextMenu(ContextMenuProps menu);
int Radio(RadioProps radio);
void Progress(ProgressProps progress);
void Plot(PlotProps plot);
int Drag(DragProps drag);
int Input(InputProps input);
int Slider(SliderProps slider);
int Spinbox(SpinboxProps spinbox);
int BeginPopup(PopupProps popup);
void EndPopup(void);
void ClosePopup(void);
void Fieldset(FieldsetProps frame);
int ListBox(ListBoxProps list);
int TreeView(TreeViewProps tree);
int TableView(TableViewProps table);
Rectangle BeginTableCell(TableViewProps table, int row, int column);
void EndTableCell(void);
int TextArea(TextAreaProps area);
void CanvasGrid(Rectangle bounds, int step, Color color);
int PanedView(PanedViewProps panes);
int Collapsible(CollapsibleProps section);
int ColorPicker(ColorPickerProps picker);
void Focus(Rectangle bounds);
void FocusDebugOverlay(const UIAccessibilityNode *nodes, int count);
void TransitionFade(const struct UITransition *transition, int width,
                          int height, Color color);
NavigationBarResult NavigationBar(NavigationBarProps nav);
ToolbarResult Toolbar(ToolbarProps toolbar);
int TabBar(TabBarProps bar);
/* Render a canonical tab bar and begin its arbitrary-content scope. The
 * caller owns selected_index; a tab selected by pointer or keyboard is
 * written before BeginTabItem is evaluated in the same frame. */
int BeginTabBar(TabBarProps bar, int *selected_index);
int BeginTabItem(int index);
void EndTabItem(void);
void EndTabBar(void);
int Modal(ModalProps modal);
int TitleBar(TitleBarProps title_bar);

int Button(ButtonProps button);
NodeId BeginButton(ButtonProps button);
int Selectable(SelectableProps selectable);
int InvisibleButton(InvisibleButtonProps button);
void Bullet(Rectangle bounds);
/* Layout nodes: auto-position children like flexbox. */
typedef struct {
    Rectangle bounds;
    int gap;
    int padding;
    KeyID key;
} ColumnProps;

typedef struct {
    Rectangle bounds;
    int gap;
    int padding;
    KeyID key;
} RowProps;

enum {
    ROUTER_NO_ROUTE = -2147483647
};

typedef struct RouterRoute {
    int id;
    int parent;
    const char *path;
    const char *title;
    const char *group;
} RouterRoute;

typedef struct RouterState {
    int initialized;
    int current_route;
    int previous_route;
    int requested_route;
    int changed;
    int route_version;
    unsigned int generation;
} RouterState;

typedef struct RouterProps {
    Rectangle bounds;
    KeyID key;
    RouterState *state;
    const RouterRoute *routes;
    int route_count;
    int initial_route;
    int sync_url;
    int replace_on_init;
} RouterProps;

typedef struct RouterResult {
    int route;
    int previous_route;
    int requested_route;
    int changed;
    const RouterRoute *route_info;
} RouterResult;

GridMetrics MeasureGrid(GridProps props);
GridCursor BeginGridCursor(GridProps props);
GridCursor GridStep(GridCursor cursor, int32_t height, int32_t column_span);
int32_t GridCursorHeight(GridCursor cursor);

void RouterStateInit(RouterState *state, int initial_route);
void RouterNavigate(RouterState *state, int route_id);
int RouterSetRoute(RouterProps props, int route_id, int push);
const RouterRoute *RouterFindRoute(const RouterRoute *routes, int route_count,
                                   int route_id);
RouterResult Router(RouterProps props);
NodeId Column(ColumnProps props);
NodeId Row(RowProps props);
NodeId Grid(GridProps props);
NodeId Stack(ColumnProps props);
NodeId Screen(ColumnProps props);

#endif

#ifndef KRYON_TREE_H
#define KRYON_TREE_H

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

struct TransitionState;

typedef int NodeId;
typedef unsigned long long KeyID;

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

typedef enum WidgetKind {
    WIDGET_SCREEN,
    WIDGET_BACKGROUND,
    WIDGET_TEXT,
    WIDGET_RECT,
    WIDGET_CIRCLE,
    WIDGET_RING,
    WIDGET_LINE,
    WIDGET_TRIANGLE,
    WIDGET_BUTTON,
    WIDGET_TEXT_FIELD,
    WIDGET_TEXT_AREA,
    WIDGET_DROPDOWN,
    WIDGET_SLIDER,
    WIDGET_TOGGLE,
    WIDGET_CHECKBOX,
    WIDGET_PARAGRAPH,
    WIDGET_NAVIGATION_BAR,
    WIDGET_TAB_BAR,
    WIDGET_TITLE_BAR,
    WIDGET_GROUP,
    WIDGET_COLUMN,
    WIDGET_ROW,
    WIDGET_STACK,
    WIDGET_GRID,
    WIDGET_IMAGE,
    WIDGET_CUSTOM,
    WIDGET_DRAG,
    WIDGET_TEXT_INPUT_PAINT,
    WIDGET_ROUTER,
    WIDGET_CARD
} WidgetKind;

/* Prepared painting only: no editing-state pointers survive submission. */
typedef struct WidgetTextInputPaint {
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
} WidgetTextInputPaint;

typedef union WidgetData {
    WidgetTextInputPaint text_input_paint;
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
} WidgetData;

typedef struct WidgetNode {
    int id;
    KeyID key;
    WidgetKind kind;
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
    WidgetData data;
    unsigned flags;
    unsigned generation;
    char *owned_text;
} WidgetNode;

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
const WidgetNode *GetTreeNodes(int *count);
int GetNodeHeight(WidgetNode node);
int GetNodeHeightById(int id);
const WidgetNode *GetNode(NodeId id);
NodeId HitTestNode(Vector2 point);
int GetAccessibilitySnapshot(AccessibilityNode *nodes, int capacity);
void SetAccessibilitySink(AccessibilitySink sink, void *userdata);

WidgetNode NodeParagraph(ParagraphSpec paragraph, int x, int y);
WidgetNode NodeNavigationBar(NavigationBarProps nav);
WidgetNode NodeTabBar(TabBarProps bar);
WidgetNode NodeTitleBar(int height);

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
void Icon(int id, int x, int y, int size, IconType icon, Color tint);
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
void FocusDebugOverlay(const AccessibilityNode *nodes, int count);
void TransitionFade(const struct TransitionState *transition, int width,
                          int height, Color color);
NavigationBarResult NavigationBar(NavigationBarProps nav);
ToolbarResult Toolbar(ToolbarProps toolbar);
int TabBar(TabBarProps bar);
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
NodeId Group(ColumnProps props);
NodeId Stack(ColumnProps props);
NodeId Screen(ColumnProps props);

#endif

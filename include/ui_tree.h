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
#include "ui_picture.h"
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
    UI_WIDGET_LINE_NODE,
    UI_WIDGET_BUTTON_NODE,
    UI_WIDGET_TEXT_FIELD_NODE,
    UI_WIDGET_TEXT_AREA_NODE,
    UI_WIDGET_DROPDOWN_NODE,
    UI_WIDGET_SLIDER_NODE,
    UI_WIDGET_TOGGLE_NODE,
    UI_WIDGET_CHECKBOX_NODE,
    UI_WIDGET_THEME_SETTINGS_NODE,
    UI_WIDGET_PARAGRAPH_NODE,
    UI_WIDGET_READONLY_TEXT_BOX_NODE,
    UI_WIDGET_LABEL_TEXT_FIELD_NODE,
    UI_WIDGET_SECTION_LABEL_NODE,
    UI_WIDGET_CHECKBOX_ROW_NODE,
    UI_WIDGET_BUTTON_ROW_NODE,
    UI_WIDGET_BOTTOM_NAV_NODE,
    UI_WIDGET_TAB_BAR_NODE,
    UI_WIDGET_THEME_PICKER_NODE,
    UI_WIDGET_PARAGRAPH_MODAL_NODE,
    UI_WIDGET_TITLE_BAR_NODE,
    UI_WIDGET_GROUP_NODE,
    UI_WIDGET_COLUMN_NODE,
    UI_WIDGET_ROW_NODE,
    UI_WIDGET_STACK_NODE,
    UI_WIDGET_GRID_NODE,
    UI_WIDGET_PICTURE_NODE,
    UI_WIDGET_CUSTOM_NODE,
    UI_WIDGET_FLOAT_SLIDER_NODE,
    UI_WIDGET_INT_SLIDER_NODE,
    UI_WIDGET_ANGLE_SLIDER_NODE,
    UI_WIDGET_FLOAT_DRAG_NODE,
    UI_WIDGET_INT_DRAG_NODE,
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
        DragFloatProps props;
        size_t format_offset;
    } float_drag;
    struct {
        DragIntProps props;
        size_t format_offset;
    } int_drag;
    struct {
        SliderAngleProps props;
        size_t format_offset;
    } angle_slider;
    struct {
        SliderFloatProps props;
        int vertical;
        size_t format_offset;
    } float_slider;
    struct {
        SliderIntProps props;
        int vertical;
        size_t format_offset;
    } int_slider;
    struct {
        int gap;
        int padding;
        int columns;
        int min_item_width;
        int max_columns;
    } layout;
    ParagraphSpec paragraph;
    ReadonlyTextBoxProps readonly_text_box;
    LabelTextFieldProps label_text_field;
    SectionLabelProps section_label;
    CheckboxRowProps checkbox_row;
    ButtonRowProps button_row;
    ThemeSettingsProps theme_settings;
    ParagraphModalMeasureProps paragraph_modal;
    PictureProps picture;
    struct {
        int x2;
        int y2;
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
    struct {
        int *value;
        const char *label;
        const char *suffix;
        const char *value_text_override;
        int min;
        int max;
        int vertical;
        UIVerticalSliderMarkCallback mark_callback;
        void *mark_callback_user_data;
    } slider;
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
UIWidgetNode NodeLabelTextField(LabelTextFieldProps row, int x, int y, int w);
UIWidgetNode NodeSectionLabel(SectionLabelProps label, int x, int y);
UIWidgetNode NodeCheckboxRow(CheckboxRowProps row, int x, int y);
UIWidgetNode NodeButtonRow(ButtonRowProps row);
UIWidgetNode NodeBottomNav(BottomNavProps nav);
UIWidgetNode NodeTopNav(TopNavProps nav);
UIWidgetNode NodeTabBar(TabBarProps bar);
UIWidgetNode NodeThemeSettings(ThemeSettingsProps settings);
UIWidgetNode NodeThemePicker(int x, int y, int w);
UIWidgetNode NodeParagraphModal(ParagraphModalMeasureProps measure);
UIWidgetNode NodeTitleBar(int height);

Style ResolveButtonStyle(ButtonProps button, ButtonState state);

typedef struct MenuButtonProps {
    ButtonProps button;
    int menu_id;
    const MenuItem *items;
    int item_count;
    int *open;
} MenuButtonProps;

typedef struct SplitButtonProps {
    ButtonProps button;
    int menu_id;
    const MenuItem *items;
    int item_count;
    int *open;
} SplitButtonProps;

typedef struct SplitButtonResult {
    int clicked;
    int activated_id;
} SplitButtonResult;

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
    int *flags;
    int flags_value;
    int disabled;
} CheckboxFlagsProps;

typedef struct {
    PictureProps picture;
    Color background;
} ImageWithBgProps;

typedef struct {
    PictureProps picture;
    Color background;
    int id;
    int disabled;
} ImageButtonProps;

typedef struct {
    Rectangle bounds;
    int id;
    const char *label;
    int font;
    int disabled;
} TabItemButtonProps;

typedef struct {
    Rectangle bounds;
    const Tab *tabs;
    int count;
    int *selected_index;
    int font;
    int *closed_index;
    int id;
    int disabled;
} ClosableTabBarProps;

void Background(Color color);
void Surface(Rectangle bounds, Style style);
int Card(CardProps card);
NodeId BeginCard(CardProps card);
void Text(TextProps props);
void LabelText(const char *label, const char *value, Rectangle bounds,
               int font_size, Color color);
void BulletText(const char *text, Rectangle bounds, int font_size,
                Color color);
void ValueBool(const char *prefix, int value, Rectangle bounds,
               int font_size, Color color);
void ValueInt(const char *prefix, int value, Rectangle bounds,
              int font_size, Color color);
void ValueUInt(const char *prefix, unsigned int value, Rectangle bounds,
               int font_size, Color color);
void ValueFloat(const char *prefix, float value, const char *format,
                Rectangle bounds, int font_size, Color color);
void Paragraph(ParagraphSpec paragraph, int x, int *y);
void TextLines(const char **lines, int count, int x, int *y,
                     int font, int line_h, Color color);
#ifdef KRYON_BACKEND_LIBDRAW
void kry_ui_rect_shape(int x, int y, int w, int h, Color fill, Color border);
#define Rect kry_ui_rect_shape
#else
void Rect(int x, int y, int w, int h, Color fill, Color border);
#endif
void Line(int x1, int y1, int x2, int y2, Color color);
void Bevel(int x, int y, int w, int h, Color light, Color dark);
void Icon(int id, int x, int y, int size, UIIconType icon, Color tint);
void Picture(PictureProps picture);
int ButtonNode(ButtonSpec button);
int Href(HrefProps link);
int TextField(TextFieldProps field);
int InfoButton(int id, int center_x, int center_y, int diameter);
void IconLink(int id, int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int Dropdown(DropdownProps dropdown);
int DropdownLegacy(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int DropdownOptions(int id, int x, int y, int w, int h,
                    const DropdownOption *options, int option_count,
                    int *selected_index);
int Slider(int id, int x, int y, int w, const char *label,
                 int min, int max, int *value, const char *suffix,
                 const char *value_text_override);
int Toggle(int id, int x, int y, int w, int h, int *value,
                 const char *off_label, const char *on_label);
int Checkbox(int id, int x, int y, const char *label, int *value);
int ThemeSettings(ThemeSettingsProps settings, ThemeSettingsState *state,
                        ThemeSettingsResult *result);
void Separator(Rectangle bounds, int vertical);
void SeparatorText(SeparatorTextProps separator);
int DragDropSource(DragDropSourceProps source);
int DragDropTarget(DragDropTargetProps target);
int MultiSelectList(MultiSelectListProps list);
MenuBarResult MenuBar(int id, Rectangle bounds, const Menu *menus,
                              int menu_count, int *open_index);
int PopupMenu(int id, int x, int y, const MenuItem *items,
                    int item_count);
int ContextMenu(ContextMenuProps menu);
int Radio(RadioButtonProps radio);
void Progress(ProgressBarProps progress);
void PlotLines(PlotProps plot);
void PlotHistogram(PlotProps plot);
int DragFloat(DragFloatProps drag);
int DragInt(DragIntProps drag);
int DragFloatRange2(DragFloatRange2Props drag);
int DragIntRange2(DragIntRange2Props drag);
int SliderFloat(SliderFloatProps slider);
int SliderInt(SliderIntProps slider);
int VSliderFloat(SliderFloatProps slider);
int VSliderInt(SliderIntProps slider);
int SliderAngle(SliderAngleProps slider);
int InputFloat(InputFloatProps input);
int InputInt(InputIntProps input);
int InputDouble(InputDoubleProps input);
int Spinbox(SpinboxProps spinbox);
int Combobox(ComboboxProps combo);
int BeginCombo(ComboProps combo);
void EndCombo(void);
void CloseCombo(void);
int BeginPopup(PopupProps popup);
void EndPopup(void);
void ClosePopup(void);
void LabelFrame(LabelFrameProps frame);
void ImageBox(ImageBoxProps image);
int ListBox(ListBoxProps list);
Rectangle BeginListBox(ListBoxProps list);
void EndListBox(void);
int TreeView(TreeViewProps tree);
int CascadingTreeView(CascadingTreeViewProps tree);
int SourceView(SourceViewProps source);
int TableView(TableViewProps table);
Rectangle BeginTableCell(TableViewProps table, int row, int column);
void EndTableCell(void);
int TextArea(TextAreaProps area);
int RichTextEditor(RichTextEditorProps editor);
void CanvasGrid(Rectangle bounds, int step, Color color);
int Notebook(NotebookProps notebook);
int PanedView(PanedViewProps panes);
int Collapsible(CollapsibleProps section);
int ColorPicker(Rectangle bounds, Color *color);
int ActionModal(ModalProps modal);
int MessageDialog(MessageDialogProps dialog);
int ConfirmDialog(ConfirmDialogProps dialog);
int PromptDialog(PromptDialogProps dialog);
int TextPopover(TextPopoverProps popover);
int PickerDialog(PickerDialogProps picker);
void Focus(Rectangle bounds);
void FocusDebugOverlay(const UIAccessibilityNode *nodes, int count);
UIGuideResult GuideOverlay(GuideOverlayProps guide);
int ThemeSwitcher(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int ThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
void TutorialImagePlaceholder(const char *label, int x, int y,
                                    int w, int h);
void TutorialImage(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
void TransitionFade(const struct UITransition *transition, int width,
                          int height, Color color);
void InfoRows(InfoRowsProps rows);
int LabelTextField(LabelTextFieldProps row, int x, int y, int w);
int SectionLabel(SectionLabelProps label, int x, int y);
int CheckboxRow(CheckboxRowProps row, int x, int y);
int OverlayButton(OverlayButtonProps button);
int ButtonRow(ButtonRowProps row);
int IconSliderPopup(IconSliderPopupProps popup);
IconRowResult BottomIconRow(BottomIconRowProps row);
BottomNavResult BottomNav(BottomNavProps nav);
BottomNavConfigResult BottomNavConfig(BottomNavConfigProps modal);
TopNavResult TopNav(TopNavProps nav);
ToolbarResult Toolbar(ToolbarProps toolbar);
ToolbarHeaderResult ToolbarHeader(ToolbarHeaderProps header);
int SubtabBar(SubtabBarProps bar);
int TabBar(TabBarProps bar);
/* Render a canonical tab bar and begin its arbitrary-content scope. The
 * caller owns selected_index; a tab selected by pointer or keyboard is
 * written before BeginTabItem is evaluated in the same frame. */
int BeginTabBar(TabBarProps bar, int *selected_index);
int BeginTabItem(int index);
void EndTabItem(void);
void EndTabBar(void);
PaneTabBarResult PaneTabs(PaneTabBar bar);
void PaneDropPreview(Rectangle bounds, PaneDropZone zone);
SidebarAccountHeaderResult SidebarAccountHeader(SidebarAccountHeaderProps header);
ProfilePicturePickerResult ProfilePicturePicker(ProfilePicturePickerProps modal);
void ReorderHandle(int id, int x, int y, int w, int h, int active);
void ReorderPlaceholder(Rectangle bounds);
int Modal(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int Modal3Button(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
void TitleBar(const char *title, int height);
int ReturnTitleBar(Texture2D return_icon, const char *title,
                         int height);
int ReturnDropdownTitleBar(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame ModalFrame(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);

int Button(ButtonProps button);
NodeId BeginButton(ButtonProps button);
int MenuButton(MenuButtonProps button);
SplitButtonResult SplitButton(SplitButtonProps button);
int Selectable(SelectableProps selectable);
int CheckboxFlags(CheckboxFlagsProps checkbox);
void ImageWithBg(ImageWithBgProps image);
int ImageButton(ImageButtonProps image);
int TabItemButton(TabItemButtonProps button);
int ClosableTabBar(ClosableTabBarProps bar);
int InvisibleButton(InvisibleButtonProps button);
int ArrowButton(ArrowButtonProps button);
void Bullet(Rectangle bounds);
int ColorEdit3(ColorEditProps edit);
int ColorEdit4(ColorEditProps edit);
int ColorPicker3(ColorEditProps picker);
int ColorPicker4(ColorEditProps picker);
int ColorButton(ColorButtonProps button);

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

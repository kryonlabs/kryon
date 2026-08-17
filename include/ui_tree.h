#ifndef UI_TREE_H
#define UI_TREE_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_overlay.h"
#include "ui_profile.h"
#include "ui_rows.h"
#include "ui_picture.h"
#include "ui_tk.h"

struct UITransition;

typedef int UINodeId;
typedef unsigned long long UIKey;

typedef enum UIEventKind {
    UI_EVENT_NONE = 0,
    UI_EVENT_CLICK,
    UI_EVENT_VALUE_CHANGED,
    UI_EVENT_TEXT_CHANGED,
    UI_EVENT_TEXT_COMMIT,
    UI_EVENT_SELECTION_CHANGED,
    UI_EVENT_FOCUS,
    UI_EVENT_BLUR
} UIEventKind;

typedef struct UIEvent {
    UIKey key;
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
    UI_WIDGET_PICTURE_NODE,
    UI_WIDGET_CUSTOM_NODE
} UIWidgetKind;

typedef union UIWidgetData {
    struct {
        int gap;
        int padding;
    } layout;
    UIParagraphSpec paragraph;
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
        Color color;
        Color border;
    } primitive;
    struct {
        UIButtonSpec spec;
        UIButtonStyle style;
    } button;
    TextFieldProps text_field;
} UIWidgetData;

typedef struct UIWidgetNode {
    int id;
    UIKey key;
    UIWidgetKind kind;
    Rectangle bounds;
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

/* BeginUI starts a declaration pass. EndUI atomically reconciles, lays out,
 * routes, updates, and paints it. Every container closes with End(). */
void BeginUI(UIKey screen_key);
void EndUI(void);
void End(void);
UIKey Key(const char *text);
void InvalidateUI(UIInvalidation invalidation);
int NextUIEvent(UIEvent *event);
int SetSelection(UIKey key, int anchor, int cursor);
void UIReconcileTree(void);
void UILayoutTree(void);
void UIRouteInput(void);
void UIUpdateTree(void);
void DrawUITree(void);
void DrawUIOverlays(void);
const UIWidgetNode *UIGetTreeNodes(int *count);
int UIGetNodeHeight(UIWidgetNode node);
int UIGetNodeHeightById(int id);
const UIWidgetNode *UIGetNode(UINodeId id);
UINodeId UIHitTestNode(Vector2 point);

UIWidgetNode UINodeParagraph(UIParagraphSpec paragraph, int x, int y);
UIWidgetNode UINodeReadonlyTextBox(ReadonlyTextBoxProps box);
UIWidgetNode UINodeLabelTextField(LabelTextFieldProps row, int x, int y, int w);
UIWidgetNode UINodeSectionLabel(SectionLabelProps label, int x, int y);
UIWidgetNode UINodeCheckboxRow(CheckboxRowProps row, int x, int y);
UIWidgetNode UINodeButtonRow(ButtonRowProps row);
UIWidgetNode UINodeBottomNav(BottomNavProps nav);
UIWidgetNode UINodeTopNav(TopNavProps nav);
UIWidgetNode UINodeTabBar(TabBarProps bar);
UIWidgetNode UINodeThemeSettings(ThemeSettingsProps settings);
UIWidgetNode UINodeThemePicker(int x, int y, int w);
UIWidgetNode UINodeParagraphModal(ParagraphModalMeasureProps measure);
UIWidgetNode UINodeTitleBar(int height);

typedef struct ButtonProps {
    Rectangle bounds;
    const char *label;
    UIButtonStyle style;
    int font;
    int id;
    int disabled;
} ButtonProps;

void Background(Color color);
void Text(const char *text, int x, int y, int font_size, Color color);
void TextInRect(const char *text, Rectangle rect, int font_size,
                      Color color);
void Paragraph(UIParagraphSpec paragraph, int x, int *y);
void TextLines(const char **lines, int count, int x, int *y,
                     int font, int line_h, Color color);
void Rect(int x, int y, int w, int h, Color fill, Color border);
void Line(int x1, int y1, int x2, int y2, Color color);
void Bevel(int x, int y, int w, int h, Color light, Color dark);
void IconTexture(int id, int x, int y, int size, Texture2D icon,
                       Color tint);
void Picture(PictureProps picture);
int UIButtonNode(UIButtonSpec button);
int IconButton(IconButtonProps button);
int Href(HrefProps link);
int TextInputControl(TextInputProps input);
int GenericButton(int id, int x, int y, int w, int h,
                        const char *label, UIButtonStyle style,
                        int disabled, int *hover);
int TextField(TextFieldProps field);
int ReadonlyTextBox(ReadonlyTextBoxProps box);
int IconBtn(int id, int x, int y, UIIconSize size, Texture2D icon,
                  int *hover);
int PaddedIconBtn(int id, int x, int y, int size, int padding,
                        Texture2D icon, int *hover);
int InfoButton(int id, int center_x, int center_y, int diameter);
int TextButton(int id, int x, int y, const char *label, int *hover);
void IconLink(int id, int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int Dropdown(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int DropdownEx(int id, int x, int y, int w, int h,
                     const UIDropdownOption *options, int option_count,
                     int *selected_index);
int LocaleDropdown(int id, int x, int y, int w, int h,
                         int *selected_index);
int Slider(int id, int x, int y, int w, const char *label,
                 int min, int max, int *value, const char *suffix,
                 const char *value_text_override);
int VerticalSlider(int id, int x, int y, int h, int min, int max,
                         int *value);
int VerticalSliderWithMarks(int id, int x, int y, int h, int min,
                                  int max, int *value,
                                  UIVerticalSliderMarkCallback callback,
                                  void *callback_user_data);
int Toggle(int id, int x, int y, int w, int h, int *value,
                 const char *off_label, const char *on_label);
int Checkbox(int id, int x, int y, const char *label, int *value);
int ThemeSettings(ThemeSettingsProps settings, UIThemeSettingsState *state,
                        UIThemeSettingsResult *result);
void Separator(Rectangle bounds, int vertical);
UIMenuBarResult MenuBar(int id, Rectangle bounds, const UIMenu *menus,
                              int menu_count, int *open_index);
int PopupMenu(int id, int x, int y, const UIMenuItem *items,
                    int item_count);
int Radio(RadioButtonProps radio);
void Progress(ProgressBarProps progress);
int Spinbox(SpinboxProps spinbox);
int Combobox(ComboboxProps combo);
void LabelFrame(LabelFrameProps frame);
void ImageBox(ImageBoxProps image);
int ListBox(ListBoxProps list);
int TreeView(TreeViewProps tree);
int CascadingTreeView(CascadingTreeViewProps tree);
int SourceView(SourceViewProps source);
int TableView(TableViewProps table);
int TextArea(TextAreaProps area);
void CanvasGrid(Rectangle bounds, int step, Color color);
int Notebook(NotebookProps notebook);
int PanedView(PanedViewProps panes);
int Collapsible(CollapsibleProps section);
int ColorPicker(Rectangle bounds, Color *color);
int ActionModal(ModalProps modal);
int MessageDialog(MessageDialogProps dialog);
int ConfirmDialog(ConfirmDialogProps dialog);
int PromptDialog(PromptDialogProps dialog);
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
UIIconRowResult BottomIconRow(BottomIconRowProps row);
UIBottomNavResult BottomNav(BottomNavProps nav);
UIBottomNavConfigResult BottomNavConfig(BottomNavConfigProps modal);
UITopNavResult TopNav(TopNavProps nav);
UIToolbarResult Toolbar(ToolbarProps toolbar);
UIToolbarHeaderResult ToolbarHeader(ToolbarHeaderProps header);
int SubtabBar(SubtabBarProps bar);
int TabBar(TabBarProps bar);
UISidebarAccountHeaderResult SidebarAccountHeader(UISidebarAccountHeaderSpec header);
UIProfilePicturePickerResult ProfilePicturePicker(UIProfilePicturePickerModal modal);
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

/* Layout nodes: auto-position children like flexbox. */
typedef struct {
    Rectangle bounds;
    int gap;
    int padding;
    UIKey key;
} ColumnProps;

typedef struct {
    Rectangle bounds;
    int gap;
    int padding;
    UIKey key;
} RowProps;

UINodeId Column(ColumnProps props);
UINodeId Row(RowProps props);
UINodeId Stack(ColumnProps props);

#endif

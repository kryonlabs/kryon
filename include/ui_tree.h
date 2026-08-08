#ifndef UI_TREE_H
#define UI_TREE_H

#include "kryon_compat.generated.h"
#include "ui_controls.h"
#include "ui_modal.h"
#include "ui_nav.h"
#include "ui_overlay.h"
#include "ui_profile.h"
#include "ui_rows.h"
#include "ui_tk.h"

struct UITransition;

typedef int UINodeId;

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
    UI_WIDGET_CUSTOM_NODE
} UIWidgetKind;

typedef union UIWidgetData {
    UIParagraph paragraph;
    UIReadonlyTextBox readonly_text_box;
    UILabelTextField label_text_field;
    UISectionLabel section_label;
    UICheckboxRow checkbox_row;
    UIButtonRow button_row;
    UIThemeSettings theme_settings;
    UIParagraphModalMeasure paragraph_modal;
} UIWidgetData;

typedef struct UIWidgetNode {
    int id;
    UIWidgetKind kind;
    Rectangle bounds;
    int parent;
    int first_child;
    int next_sibling;
    const void *props;
    void *state;
    UIWidgetData data;
    unsigned flags;
} UIWidgetNode;

void UIBeginTree(int screen_id);
void UIEndTree(void);
void UIReconcileTree(void);
void UILayoutTree(void);
void UIRouteInput(void);
void UIUpdateTree(void);
void UIRenderTree(void);
void UIRenderOverlays(void);
const UIWidgetNode *UIGetTreeNodes(int *count);
int UIGetNodeHeight(UIWidgetNode node);
int UIGetNodeHeightById(int id);

UIWidgetNode UINodeParagraph(UIParagraph paragraph, int x, int y);
UIWidgetNode UINodeReadonlyTextBox(UIReadonlyTextBox box);
UIWidgetNode UINodeLabelTextField(UILabelTextField row, int x, int y, int w);
UIWidgetNode UINodeSectionLabel(UISectionLabel label, int x, int y);
UIWidgetNode UINodeCheckboxRow(UICheckboxRow row, int x, int y);
UIWidgetNode UINodeButtonRow(UIButtonRow row);
UIWidgetNode UINodeBottomNav(UIBottomNav nav);
UIWidgetNode UINodeTopNav(UITopNav nav);
UIWidgetNode UINodeTabBar(UITabBar bar);
UIWidgetNode UINodeThemeSettings(UIThemeSettings settings);
UIWidgetNode UINodeThemePicker(int x, int y, int w);
UIWidgetNode UINodeParagraphModal(UIParagraphModalMeasure measure);
UIWidgetNode UINodeTitleBar(int height);

void UIBackground(Color color);
void UITextNode(const char *text, int x, int y, int font_size, Color color);
void UITextInRectNode(const char *text, Rectangle rect, int font_size,
                      Color color);
void UIParagraphNode(UIParagraph paragraph, int x, int *y);
void UITextLinesNode(const char **lines, int count, int x, int *y,
                     int font, int line_h, Color color);
void UIRectNode(int x, int y, int w, int h, Color fill, Color border);
void UILineNode(int x1, int y1, int x2, int y2, Color color);
void UIBevelNode(int x, int y, int w, int h, Color light, Color dark);
void UIIconTextureNode(int id, int x, int y, int size, Texture2D icon,
                       Color tint);
int UIButtonNode(UIButton button);
int UIIconButtonNode(UIIconButton button);
int UIHrefNode(UIHref link);
int UITextInputControlNode(UITextInput input);
int UIGenericButtonNode(int id, int x, int y, int w, int h,
                        const char *label, UIButtonStyle style,
                        int disabled, int *hover);
int UITextFieldNode(UITextField field);
int UIReadonlyTextBoxNode(UIReadonlyTextBox box);
int UIIconBtnNode(int id, int x, int y, UIIconSize size, Texture2D icon,
                  int *hover);
int UIPaddedIconBtnNode(int id, int x, int y, int size, int padding,
                        Texture2D icon, int *hover);
int UIInfoButtonNode(int id, int center_x, int center_y, int diameter);
int UITextButtonNode(int id, int x, int y, const char *label, int *hover);
void UIIconLinkNode(int id, int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int UIDropdownNode(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int UIDropdownNodeEx(int id, int x, int y, int w, int h,
                     const UIDropdownOption *options, int option_count,
                     int *selected_index);
int UILocaleDropdownNode(int id, int x, int y, int w, int h,
                         int *selected_index);
int UISliderNode(int id, int x, int y, int w, const char *label,
                 int min, int max, int *value, const char *suffix);
int UIVerticalSliderNode(int id, int x, int y, int h, int min, int max,
                         int *value);
int UIVerticalSliderWithMarksNode(int id, int x, int y, int h, int min,
                                  int max, int *value,
                                  UIVerticalSliderMarkCallback callback,
                                  void *callback_user_data);
int UIToggleNode(int id, int x, int y, int w, int h, int *value,
                 const char *off_label, const char *on_label);
int UICheckboxNode(int id, int x, int y, const char *label, int *value);
int UIThemeSettingsNode(UIThemeSettings settings, UIThemeSettingsState *state,
                        UIThemeSettingsResult *result);
void UISeparatorNode(Rectangle bounds, int vertical);
UIMenuBarResult UIMenuBarNode(int id, Rectangle bounds, const UIMenu *menus,
                              int menu_count, int *open_index);
int UIPopupMenuNode(int id, int x, int y, const UIMenuItem *items,
                    int item_count);
int UIRadioNode(UIRadioButton radio);
void UIProgressNode(UIProgressBar progress);
int UISpinboxNode(UISpinbox spinbox);
int UIComboboxNode(UICombobox combo);
void UILabelFrameNode(UILabelFrame frame);
void UIImageBoxNode(UIImageBox image);
int UIListBoxNode(UIListBox list);
int UITreeViewNode(UITreeView tree);
int UICascadingTreeViewNode(UICascadingTreeView tree);
int UISourceViewNode(UISourceView source);
int UITableViewNode(UITableView table);
int UITextAreaNode(UITextArea area);
void UICanvasGridNode(Rectangle bounds, int step, Color color);
int UINotebookNode(UINotebook notebook);
int UIPanedViewNode(UIPanedView panes);
int UICollapsibleNode(UICollapsible section);
int UIColorPickerNode(Rectangle bounds, Color *color);
int UIActionModalNode(UIModalSpec modal);
int UIMessageDialogNode(UIMessageDialog dialog);
int UIConfirmDialogNode(UIConfirmDialog dialog);
int UIPromptDialogNode(UIPromptDialog dialog);
void UIFocusNode(Rectangle bounds);
void UIFocusDebugOverlayNode(const UIAccessibilityNode *nodes, int count);
UIGuideResult UIGuideOverlayNode(UIGuideOverlay guide);
int UIThemeSwitcherNode(int x, int y, int w, const char *label,
                        const char *light_label, const char *dark_label,
                        int *theme_id, int *dark_mode);
int UIThemePickerNode(int x, int y, int w, int dark_mode, int *theme_id);
void UITutorialImagePlaceholderNode(const char *label, int x, int y,
                                    int w, int h);
void UITutorialImageNode(Texture2D texture, const char *fallback,
                         int x, int y, int w, int h);
void UITransitionFadeNode(const struct UITransition *transition, int width,
                          int height, Color color);
void UIInfoRowsNode(UIInfoRows rows);
int UILabelTextFieldNode(UILabelTextField row, int x, int y, int w);
int UISectionLabelNode(UISectionLabel label, int x, int y);
int UICheckboxRowNode(UICheckboxRow row, int x, int y);
int UIOverlayButtonNode(UIOverlayButton button);
int UIButtonRowNode(UIButtonRow row);
int UIIconSliderPopupNode(UIIconSliderPopup popup);
UIIconRowResult UIBottomIconRowNode(UIBottomIconRow row);
UIBottomNavResult UIBottomNavNode(UIBottomNav nav);
UIBottomNavConfigResult UIBottomNavConfigNode(UIBottomNavConfigModal modal);
UITopNavResult UITopNavNode(UITopNav nav);
UIToolbarResult UIToolbarNode(UIToolbar toolbar);
UIToolbarHeaderResult UIToolbarHeaderNode(UIToolbarHeader header);
int UISubtabBarNode(UISubtabBar bar);
int UITabBarNode(UITabBar bar);
UISidebarAccountHeaderResult UISidebarAccountHeaderNode(UISidebarAccountHeader header);
UIProfilePicturePickerResult UIProfilePicturePickerNode(UIProfilePicturePickerModal modal);
void UIReorderHandleNode(int id, int x, int y, int w, int h, int active);
void UIReorderPlaceholderNode(Rectangle bounds);
int UIModalNode(const char *title, const char *message,
                const char *cancel_btn, const char *confirm_btn);
int UIModal3ButtonNode(const char *title, const char *message,
                       const char *left_btn, const char *middle_btn,
                       const char *right_btn);
void UITitleBarNode(const char *title, int height);
int UIReturnTitleBarNode(Texture2D return_icon, const char *title,
                         int height);
int UIReturnDropdownTitleBarNode(Texture2D return_icon,
                                 UITitleBarDropdown dropdown, int height);
UIPanelFrame UIModalFrameNode(int width, int height, const char *title,
                              Texture2D left_icon, Texture2D right_icon);

#endif

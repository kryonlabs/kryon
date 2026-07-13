#ifndef UI_H
#define UI_H

#include "flint.h"
#include "ui_text.h"
#include "ui_icon_types.h"
#include <stddef.h>

typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;

typedef enum {
    UI_BUTTON_STYLE_PRIMARY,
    UI_BUTTON_STYLE_SECONDARY,
    UI_BUTTON_STYLE_DANGER,
    UI_BUTTON_STYLE_TAB,
    UI_BUTTON_STYLE_TAB_SELECTED
} UIButtonStyle;

typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
} UITextInputStyle;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color text;
    Color border;
    float radius;
} UIButton;

typedef struct {
    Rectangle bounds;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int icon_padding;
    int focus_id;
    int disabled;
    Color background;
    Color hover_background;
    Color icon_color;
    Color border;
    float radius;
} UIIconButton;

typedef struct {
    Rectangle bounds;
    const char *text;
    const char *href;
    int font;
    int focus_id;
    int disabled;
    Color color;
    Color hover_color;
} UIHref;

typedef struct {
    int id;
    int x;
    int y;
    int icon_size;
    int icon_padding;
    Texture2D icon;
    int *open;
    int *value;
    int min;
    int max;
    int popup_width;
    int popup_height;
} UIIconSliderPopup;

typedef struct {
    Texture2D icon;
    int disabled;
} UIIconRowItem;

typedef struct {
    int center_x;
    int view_width;
    int view_height;
    int count;
    const UIIconRowItem *items;
    int icon_size;
    int icon_padding;
    int gap;
    int side_margin;
    int bottom_margin;
    int max_button_width;
    int min_icon_size;
    int min_icon_padding;
    int min_gap;
} UIBottomIconRow;

typedef struct {
    int clicked_index;
    int y;
    int button_width;
} UIIconRowResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
    int active;
    int disabled;
} UIBottomNavItem;

typedef struct {
    int view_width;
    int view_height;
    int count;
    const UIBottomNavItem *items;
    int height;
    int icon_size;
    int icon_padding;
    int side_margin;
    int bottom_margin;
    int max_button_width;
} UIBottomNav;

typedef struct {
    int clicked_index;
    int clicked_route;
    int y;
    int height;
} UIBottomNavResult;

typedef struct {
    int id;
    Rectangle bounds;
    int disabled;
} UIReorderItem;

typedef struct {
    int id;
    Rectangle bounds;
    const UIReorderItem *items;
    int item_count;
    int handle_width;
    int drag_threshold;
    int *scroll_offset;
    int max_scroll;
    int viewport_top;
    int viewport_bottom;
    int auto_scroll_margin;
    int auto_scroll_step;
} UIReorderList;

typedef struct {
    int active;
    int dragging;
    int committed;
    int from_index;
    int to_index;
    int active_index;
    int target_index;
    int active_id;
    int pointer_y;
    int drag_delta_y;
} UIReorderListResult;

typedef struct {
    int route;
    const char *label;
    Texture2D icon;
} UIBottomNavOption;

typedef struct {
    int id;
    const char *title;
    int *routes;
    int *route_count;
    int max_route_count;
    const char **slot_labels;
    const UIBottomNavOption *options;
    int option_count;
    const char *add_label;
    const char *cancel_label;
    const char *save_label;
    const char *reset_label;
    Texture2D close_icon;
} UIBottomNavConfigModal;

typedef struct {
    int action;
    int changed;
} UIBottomNavConfigResult;

typedef struct {
    const char *message;
    int width;
    int header_h;
    int button_h;
    int line_gap;
    int extra_lines;
    int min_height;
    int font;
} UIParagraphModalMeasure;

typedef struct {
    Texture2D icon;
    int disabled;
} UIToolbarAction;

typedef struct {
    int id;
    int x;
    int y;
    int width;
    int height;
    int draw_menu;
    const char **options;
    int option_count;
    int *selected_index;
    int dropdown_min_width;
    int dropdown_max_width;
    int dropdown_height;
    const UIToolbarAction *actions;
    int action_count;
    int action_icon_size;
    int action_icon_padding;
    int action_gap;
    int side_padding;
} UIToolbar;

typedef struct {
    int selected_menu_item;
    int clicked_action;
} UIToolbarResult;

typedef struct {
    UIToolbar toolbar;
    Texture2D leading_icon;
    int leading_width;
    int leading_icon_size;
    int leading_icon_padding;
} UIToolbarHeader;

typedef struct {
    UIToolbarResult toolbar;
    int leading_clicked;
} UIToolbarHeaderResult;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    const char *username;
    const char *subtitle;
    const char *friends_text;
    Texture2D pfp_icon;
    const Texture2D *icons;
    UIIconType pfp_icon_type;
    int content_padding_x;
    int current_frame;
    int block_click_frame;
} UISidebarAccountHeader;

typedef struct {
    int pfp_clicked;
    int username_clicked;
    int friends_clicked;
    int height;
} UISidebarAccountHeaderResult;

typedef struct {
    const char *title;
    const Texture2D *icons;
    UIIconType *selected_icon_type;
    Texture2D close_icon;
    int max_width;
    int *scroll_offset;
} UIProfilePicturePickerModal;

typedef struct {
    int closed;
    int changed;
    int selected_index;
    UIIconType selected_icon_type;
} UIProfilePicturePickerResult;

typedef enum {
    UI_LYRA_PROFILE_ICON_NONE = 0,
    UI_LYRA_PROFILE_ICON_BIRD = 1,
    UI_LYRA_PROFILE_ICON_BOWL = 2,
    UI_LYRA_PROFILE_ICON_CACTUS = 3,
    UI_LYRA_PROFILE_ICON_HEART = 4,
    UI_LYRA_PROFILE_ICON_INCENSE = 5,
    UI_LYRA_PROFILE_ICON_LOTUS = 6,
    UI_LYRA_PROFILE_ICON_TREE1 = 7,
    UI_LYRA_PROFILE_ICON_TREE2 = 8,
    UI_LYRA_PROFILE_ICON_TREE3 = 9,
    UI_LYRA_PROFILE_ICON_TREE4 = 10,
    UI_LYRA_PROFILE_ICON_TREE5 = 11,
    UI_LYRA_PROFILE_ICON_BAMBUS = 12,
    UI_LYRA_PROFILE_ICON_BUSH = 13,
    UI_LYRA_PROFILE_ICON_COFFEE = 14,
    UI_LYRA_PROFILE_ICON_FLOWER1 = 15,
    UI_LYRA_PROFILE_ICON_FLOWER2 = 16,
    UI_LYRA_PROFILE_ICON_MOUNTAIN = 17,
    UI_LYRA_PROFILE_ICON_MUSHROOM = 18,
    UI_LYRA_PROFILE_ICON_PERSON1 = 19,
    UI_LYRA_PROFILE_ICON_RAINBOW = 20,
    UI_LYRA_PROFILE_ICON_TENT = 21,
    UI_LYRA_PROFILE_ICON_BUTTERFLY = 22,
    UI_LYRA_PROFILE_ICON_DRAGONFLY = 23,
    UI_LYRA_PROFILE_ICON_FIREPLACE = 24,
    UI_LYRA_PROFILE_ICON_FOX = 25,
    UI_LYRA_PROFILE_ICON_PALM = 26
} UILyraProfileIcon;

typedef struct {
    int id;
    const char **options;
    int option_count;
    int *selected_index;
    int disabled;
    int min_width;
    int height;
} UITitleBarDropdown;

typedef struct {
    const char *text;
    int font;
    Color color;
} UIInfoRow;

typedef struct {
    int x;
    int y;
    int width;
    int row_height;
    int padding_x;
    const UIInfoRow *rows;
    int row_count;
    Color background;
    Color separator;
    Color default_text;
} UIInfoRows;

typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIButtonRowItem;

typedef struct {
    const char *label;
    UIButtonStyle style;
    int disabled;
} UIModalAction;

typedef struct {
    const char *title;
    const char *message;
    const UIModalAction *actions;
    int action_count;
    Texture2D close_icon;
    int max_width;
} UIModalSpec;

typedef struct {
    int x;
    int y;
    int width;
    int height;
    int gap;
    const UIButtonRowItem *items;
    int count;
} UIButtonRow;

typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    UITextInputStyle style;
} UITextInput;

typedef int (*UITextInputFilter)(int codepoint, void *user_data);

typedef struct {
    char *text;
    size_t text_size;
    int *cursor_position;
    int max_codepoints;
    UITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} UITextEdit;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int font;
    int focus_id;
    UITextInputStyle style;
    UITextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} UITextField;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int *scroll_y;
    int max_codepoints;
    int font;
    int line_gap;
    int focus_id;
    const char *placeholder;
    UITextInputStyle style;
    UITextInputFilter filter;
    void *filter_user_data;
} UITextArea;

typedef struct {
    const char *label;
    UITextField field;
    int label_font;
    int label_h;
    int field_h;
    int gap;
    int bottom_gap;
    Color label_color;
} UILabelTextField;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    UITextInputStyle style;
    int line_gap;
} UIReadonlyTextBox;

typedef struct {
    const char *label;
    int font;
    int info_button;
    int icon_diameter;
    int height;
    Color color;
} UISectionLabel;

typedef struct {
    const char *label;
    int *value;
    int height;
    int disabled;
} UICheckboxRow;

typedef struct {
    Rectangle bounds;
    const char *label;
    int font;
    int disabled;
    Color background;
    Color hover_background;
    Color border;
    Color hover_border;
    Color text;
} UIOverlayButton;

typedef struct {
    const char *text;
    Texture2D icon;
    UIIconType icon_type;
    int icon_size;
    int width;
    int font;
    int line_gap;
    Color color;
} UIParagraph;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} UISubtab;

typedef struct {
    Rectangle bounds;
    const UISubtab *tabs;
    int count;
    int selected_index;
    int font;
} UISubtabBar;

typedef struct {
    const char *label;
    Texture2D icon;
    int icon_size;
    int disabled;
    Color accent;
} UITab;

typedef struct {
    Rectangle bounds;
    const UITab *tabs;
    int count;
    int selected_index;
    int font;
    int min_tab_width;
    int max_tab_width;
    int *scroll_offset;
    int focus_selected;
} UITabBar;

typedef struct {
    Rectangle anchor;
    const char *text;
} UIGuideStep;

typedef struct {
    const UIGuideStep *steps;
    int count;
    int *step;
    int view_width;
    int view_height;
    int reserved_top;
    int reserved_bottom;
    int max_width;
    int line_gap;
    int paragraph_font;
    Texture2D close_icon;
    Texture2D back_icon;
    Texture2D next_icon;
    Texture2D done_icon;
} UIGuideOverlay;

typedef struct {
    int closed;
    int finished;
    int changed;
    int step;
} UIGuideResult;

typedef struct {
    Rectangle bounds;
    int content_height;
    int content_x;
    int content_width;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
} UIScrollArea;

typedef struct {
    int content_x;
    int content_y;
    int content_w;
    int viewport_h;
    int content_h;
    int max_scroll;
} UIScrollView;

typedef struct {
    int x;
    int y;
    int w;
    int h;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
    int left_clicked;
    int right_clicked;
} UIPanelFrame;

typedef int (*UIScrollPageHeightFn)(int content_width, void *user_data);

typedef struct {
    int y;
    int height;
    int max_content_width;
    int min_content_width;
    int side_padding;
    int *scroll_offset;
    int wheel_step;
    int scrollbar_x;
    int measure_passes;
    UIScrollPageHeightFn content_height;
    void *user_data;
} UIScrollPageSpec;

typedef struct {
    UIScrollArea area;
    UIScrollView view;
    int content_x;
    int content_y;
    int content_w;
    int content_h;
} UIScrollPage;

typedef void (*UITextInputPlatformCallback)(int active);

void InitUI(int width, int height, float dpi);
void SetUIColors(Color text, Color bg, Color surface, Color circle, Color button, Color button_hover, Color icon);
void SetUILinkColor(Color link);
void ApplyCurrentUITheme(void);
int IsUIDesktopMode(void);
Camera2D GetUIDefaultCamera(void);
void BeginUIFrame(int width, int height, float dpi);
void SetUIFrame(Camera2D camera);
void ClearUIInputCaptures(void);
void PushUIInputCapture(Rectangle bounds, int allow_inside);
void BeginUIModalLayer(void);
void PushUIInputClip(Rectangle bounds);
void PopUIInputClip(void);
void SetUIModalCapture(Rectangle bounds);
void SetUITextInputPlatformCallback(UITextInputPlatformCallback callback);
void SetUICursorClickable(int *cursor_clickable);
void SetUICursorDisabled(int *cursor_disabled);
void MarkUIClickable(void);
void MarkUIDisabled(void);
void SetUIIcons(Texture2D gear_icon, Texture2D x_icon);
int UIInputCapturesClick(Vector2 point);
int UIReleaseConsumed(void);
void UIConsumeRelease(void);
int UIHoverEffectsEnabled(void);
void SetUITransitionCuesEnabled(int enabled);
int UITransitionCuesEnabled(void);
/* DPI scaling, color, and layout functions now from Flint: ScaleUIPx, ClampUIPx, LightenUIColor, DarkenUIColor, GetUICenteredColumn, GetUIPageSidePadding */
int GetUIFontSize(void);
int GetUISmallFontSize(void);
int GetUITitleFontSize(const char *title, int max_width);
int GetUIControlTextY(const char *text, int box_y, int box_h, int font);
void DrawCenteredUIControlText(const char *text, int center_x, int center_y, int font, Color color);
void DrawLeftUIControlTextInRect(const char *text, Rectangle rect, int font_size, Color color);
void DrawFittedUITextInRect(const char *text, Rectangle rect, int preferred_size, int min_size, Color color);
void DrawUITextInput(Rectangle bounds, const char *text, int cursor_position,
                              int focused, int cursor_visible, int font,
                              UITextInputStyle style);
int EditUIText(UITextEdit edit);
void QueueUITextInputCodepoint(int codepoint);
void QueueUITextInputBackspace(void);
void QueueUITextInputEnter(void);
int DrawUIButton(UIButton button);
int DrawUIIconButton(UIIconButton button);
int DrawUIHref(UIHref link);
int DrawUITextInputControl(UITextInput input);
int DrawUITextField(UITextField field);
int DrawUITextArea(UITextArea area);
int GetUIReadonlyTextBoxHeight(const char *text, int font, int width, UITextInputStyle style, int line_gap);
int DrawUIReadonlyTextBox(UIReadonlyTextBox box);
int GetUIParagraphHeight(UIParagraph paragraph);
void DrawUIParagraph(UIParagraph paragraph, int x, int *y);
UIGuideResult DrawUIGuideOverlay(UIGuideOverlay guide);
void DrawUIBevel(int x, int y, int w, int h, Color light, Color dark);
void DrawUITextLines(const char **lines, int count, int x, int *y, int font, int line_h, Color color);
/* Icon fallback drawing now from Flint: DrawUIIconFallback */
int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);
void DrawUIIconTexture(int x, int y, int size, Texture2D icon, Color tint);
int DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon, int *hover);
int DrawUIInfoButton(int center_x, int center_y, int diameter);
int DrawUIIconSliderPopup(UIIconSliderPopup popup);
UIIconRowResult DrawUIBottomIconRow(UIBottomIconRow row);
int GetUIBottomNavHeight(void);
UIBottomNavResult DrawUIBottomNav(UIBottomNav nav);
UIBottomNavConfigResult DrawUIBottomNavConfigModal(UIBottomNavConfigModal modal);
UIReorderListResult UpdateUIReorderList(UIReorderList list);
void DrawUIReorderHandle(int x, int y, int w, int h, int active);
void DrawUIReorderPlaceholder(Rectangle bounds);
UIToolbarResult DrawUIToolbar(UIToolbar toolbar);
UIToolbarHeaderResult DrawUIToolbarHeader(UIToolbarHeader header);
int GetUIProfilePictureIconCount(void);
UIIconType GetUIProfilePictureIconType(int index);
const char *GetUIProfilePictureIconName(int index);
UIIconType GetUIProfilePictureIconTypeForLyraID(int lyra_id);
int GetUILyraIDForProfilePictureIconType(UIIconType type);
UISidebarAccountHeaderResult DrawUISidebarAccountHeader(UISidebarAccountHeader header);
UIProfilePicturePickerResult DrawUIProfilePicturePickerModal(UIProfilePicturePickerModal modal);
void DrawUIInfoRows(UIInfoRows rows);
int GetUILabelTextFieldHeight(UILabelTextField row);
int DrawUILabelTextField(UILabelTextField row, int x, int y, int w);
int GetUISectionLabelHeight(UISectionLabel label);
int DrawUISectionLabel(UISectionLabel label, int x, int y);
int GetUICheckboxRowHeight(UICheckboxRow row);
int DrawUICheckboxRow(UICheckboxRow row, int x, int y);
int DrawUIOverlayButton(UIOverlayButton button);
int GetUIButtonRowHeight(UIButtonRow row);
int DrawUIButtonRow(UIButtonRow row);
int DrawUITextButton(int x, int y, const char *label, int *hover);

/* Generic button component with unified styling */
int DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                           UIButtonStyle style, int disabled, int *hover);
int DrawUISubtabBar(UISubtabBar bar);
int DrawUITabBar(UITabBar bar);
int GetUITabBarHeight(void);

void DrawUIIconLink(int x, int y, int icon_size, Texture2D icon, const char *url);
int DrawUISlider(int id, int x, int y, int w, const char *label, int min, int max, int *value, const char *suffix);
int DrawUIVerticalSlider(int id, int x, int y, int h,
                             int min, int max, int *value);
typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y, int h, int min, int max, int value);
int DrawUIVerticalSliderWithMarks(int id, int x, int y, int h,
                                       int min, int max, int *value, UIVerticalSliderMarkCallback callback,
                                       void *callback_user_data);
int DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
int DrawUICheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                     int *value, int disabled);
int DrawUIDropdownButton(int id, int x, int y, int w, int h, const char **options, int option_count, int *selected_index);
int DrawUIDropdownMenu(int id);
int UIDropdownCapturesClick(Vector2 point);
void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);
int DrawUIThemeSwitcher(int x, int y, int w, const char *label,
                           const char *light_label, const char *dark_label,
                           int *theme_id, int *dark_mode);
int DrawUIThemePicker(int x, int y, int w, int dark_mode, int *theme_id);
int GetUIThemePickerHeight(int w);
void DrawUITutorialImagePlaceholder(const char *label, int x, int y, int w, int h);
void DrawUITutorialImage(Texture2D texture, const char *fallback, int x, int y, int w, int h);
int DrawUIActionModal(UIModalSpec modal);
int DrawUIModal(const char *title, const char *message, const char *cancel_btn, const char *confirm_btn);
int DrawUIModal3Button(const char *title, const char *message, const char *left_btn, const char *middle_btn, const char *right_btn);
int GetUIParagraphModalHeight(UIParagraphModalMeasure measure);
int GetUITitleBarHeight(void);
void DrawUITitleBar(const char *title, int height);
int DrawUIReturnTitleBar(Texture2D return_icon, const char *title,
                              int height);
int DrawUIReturnDropdownTitleBar(Texture2D return_icon,
                                       UITitleBarDropdown dropdown,
                                       int height);
UIPanelFrame DrawUIModalFrame(int width, int height, const char *title,
                                      Texture2D left_icon,
                                      Texture2D right_icon);
int GetUIScrollbarReservedWidth(int max_scroll);
int GetUIScrollbarContentWidth(int content_width, int max_scroll);
int GetUIScrollbarSafeContentWidth(int content_x, int content_width,
                                    int scrollbar_x, int max_scroll);
UIScrollView MeasureUIScrollContainer(UIScrollArea area);
UIScrollView BeginUIScrollContainer(UIScrollArea area);
void EndUIScrollContainer(UIScrollArea area, UIScrollView view);
UIScrollPage BeginUIScrollPage(UIScrollPageSpec spec);
void EndUIScrollPage(UIScrollPage page);
int DrawUIScrollbar(int x, int y, int viewport_h, int content_h, int *scroll_offset, int max_scroll);

void BeginUIFocus(void);
void EndUIFocus(void);
int RegisterUIFocus(int id, Rectangle bounds);
int IsUIFocusActive(int id);
int IsUIFocusActivatePressed(int id);
void SetUIFocus(int id);
void ClearUIFocus(void);
void SetUIFocusTextInputActive(int active);
void DrawUIFocus(Rectangle bounds);

extern int ui_view_height;
extern int ui_view_width;

#endif

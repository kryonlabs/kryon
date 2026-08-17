#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "kryon_compat.generated.h"
#include "theme_style.h"
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

typedef enum {
    UI_SYNTAX_NONE,
    UI_SYNTAX_KRY,
    UI_SYNTAX_C,
    UI_SYNTAX_MAKE
} UISyntaxMode;

typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
    int padding_y;
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
} UIButtonSpec;

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
} IconButtonProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    const char *href;
    int font;
    int focus_id;
    int disabled;
    Color color;
    Color hover_color;
} HrefProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int cursor_position;
    int focused;
    int cursor_visible;
    int font;
    int focus_id;
    UITextInputStyle style;
} TextInputProps;

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
    int secure;
} TextFieldProps;

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
    UISyntaxMode syntax;
    UITextInputStyle style;
    UITextInputFilter filter;
    void *filter_user_data;
} TextAreaProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    UITextInputStyle style;
    int line_gap;
} ReadonlyTextBoxProps;

/* Public control style ABI. Apps can select a named ThemeStyle or override
 * these tokens directly when they need full control. */
typedef struct UIStyleTokens {
    /* Corner radii in unscaled UI pixels (DPI-scaled at draw time), NOT the
     * 0..0.5 normalized fraction DrawRectangleRounded takes. A fixed radius
     * keeps tall controls from rounding into pills. */
    float control_radius;
    float panel_radius;
    unsigned char control_alpha;
    unsigned char panel_alpha;
    unsigned char border_alpha;
    unsigned char shadow_alpha;
    unsigned char shine_alpha;
    int bevel_enabled;
    int touch_target_min;
    int shadow_offset_y;
} UIStyleTokens;

typedef struct UIMaterialScheme {
    Color primary;
    Color on_primary;
    Color secondary;
    Color on_secondary;
    Color surface;
    Color on_surface;
    Color surface_container;
    Color surface_variant;
    Color on_surface_variant;
    Color outline;
    Color error;
    Color on_error;
    Color disabled_container;
    Color disabled_content;
} UIMaterialScheme;

typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y,
                                             int h, int min, int max, int value);

typedef struct {
    const char *label;
    const char *font_name;
} UIDropdownOption;

UIStyleTokens GetUIStyleTokens(void);
UIStyleTokens GetUIStyleTokensForThemeStyle(ThemeStyle style);
UIMaterialScheme GetUIMaterialScheme(void);
void SetUIStyleTokens(UIStyleTokens tokens);
void ClearUIStyleTokensOverride(void);

int EditUIText(UITextEdit edit);
void QueueUITextInputCodepoint(int codepoint);
void QueueUITextInputBackspace(void);
void QueueUITextInputEnter(void);
int GetUITextAreaSelection(int focus_id, int *start, int *end);
void SetUITextAreaSelection(int focus_id, int anchor, int cursor);

int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);

int DrawUIButton(UIButtonSpec button);
int DrawUIIconButton(IconButtonProps button);
int DrawUIHref(HrefProps link);
int DrawUITextInputControl(TextInputProps input);
int DrawUITextField(TextFieldProps field);
int DrawUITextArea(TextAreaProps area);
int DrawUIReadonlyTextBox(ReadonlyTextBoxProps box);
int DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon,
                        int *hover);
int DrawUIInfoButton(int center_x, int center_y, int diameter);
int DrawUITextButton(int x, int y, const char *label, int *hover);
int DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                          UIButtonStyle style, int disabled, int *hover);
int DrawUIDropdown(int id, int x, int y, int w, int h,
                   const char **options, int option_count,
                   int *selected_index);
int DrawUIDropdownEx(int id, int x, int y, int w, int h,
                     const UIDropdownOption *options, int option_count,
                     int *selected_index);
int DrawUILocaleDropdown(int id, int x, int y, int w, int h,
                         int *selected_index);
int DrawUISlider(int id, int x, int y, int w, const char *label, int min,
                   int max, int *value, const char *suffix,
                   const char *value_text_override);
int DrawUIVerticalSlider(int id, int x, int y, int h, int min, int max,
                         int *value);
int DrawUIVerticalSliderWithMarks(int id, int x, int y, int h, int min,
                                  int max, int *value,
                                  UIVerticalSliderMarkCallback callback,
                                  void *callback_user_data);
int DrawUIToggleSwitch(int x, int y, int w, int h, int *value,
                         const char *off_label, const char *on_label);
int DrawUICheckboxToggle(int x, int y, const char *label, int *value);
int DrawDisabledUICheckboxToggle(int x, int y, const char *label,
                                 int *value, int disabled);

void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);

#endif

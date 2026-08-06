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
    UISyntaxMode syntax;
    UITextInputStyle style;
    UITextInputFilter filter;
    void *filter_user_data;
} UITextArea;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    UITextInputStyle style;
    int line_gap;
} UIReadonlyTextBox;

/* Public control style ABI. Apps can select a named ThemeStyle or override
 * these tokens directly when they need full control. */
typedef struct UIStyleTokens {
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

typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y,
                                             int h, int min, int max, int value);

UIStyleTokens GetUIStyleTokens(void);
UIStyleTokens GetUIStyleTokensForThemeStyle(ThemeStyle style);
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

typedef struct {
    const char *label;
    const char *font_name;
} UIDropdownOption;

void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);

#endif

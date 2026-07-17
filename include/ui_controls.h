#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "flint_compat.generated.h"
#include "ui_icon_types.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

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
    Rectangle bounds;
    const char *text;
    int font;
    UITextInputStyle style;
    int line_gap;
} UIReadonlyTextBox;

typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y,
                                             int h, int min, int max, int value);

int DrawUIButton(UIButton button);
int DrawUIIconButton(UIIconButton button);
int DrawUIHref(UIHref link);
int DrawUITextInputControl(UITextInput input);
void DrawUITextInput(Rectangle bounds, const char *text, int cursor_position,
                     int focused, int cursor_visible, int font,
                     UITextInputStyle style);
int EditUIText(UITextEdit edit);
void QueueUITextInputCodepoint(int codepoint);
void QueueUITextInputBackspace(void);
void QueueUITextInputEnter(void);
int DrawUITextField(UITextField field);
int DrawUITextArea(UITextArea area);
int GetUIReadonlyTextBoxHeight(const char *text, int font, int width,
                               UITextInputStyle style, int line_gap);
int DrawUIReadonlyTextBox(UIReadonlyTextBox box);

int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);
void DrawUIIconTexture(int x, int y, int size, Texture2D icon, Color tint);
int DrawUIIconBtn(int x, int y, UIIconSize size, Texture2D icon, int *hover);
int DrawUIPaddedIconBtn(int x, int y, int size, int padding, Texture2D icon,
                        int *hover);
int DrawUIInfoButton(int center_x, int center_y, int diameter);
int DrawUITextButton(int x, int y, const char *label, int *hover);
int DrawUIGenericButton(int x, int y, int w, int h, const char *label,
                        UIButtonStyle style, int disabled, int *hover);

void DrawUIIconLink(int x, int y, int icon_size, Texture2D icon,
                    const char *url);
int DrawUISlider(int id, int x, int y, int w, const char *label, int min,
                 int max, int *value, const char *suffix);
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
int DrawUIDropdownButton(int id, int x, int y, int w, int h,
                         const char **options, int option_count,
                         int *selected_index);
int DrawUIDropdownMenu(int id);
int UIDropdownCapturesClick(Vector2 point);
void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);

#ifdef __cplusplus
}
#endif

#endif

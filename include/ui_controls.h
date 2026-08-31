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
    ButtonStylePrimary,
    ButtonStyleSecondary,
    ButtonStyleDanger,
    ButtonStyleTab,
    ButtonStyleTabSelected
} ButtonStyle;

typedef enum {
    SyntaxNone,
    SyntaxKry,
    SyntaxC,
    SyntaxMake
} SyntaxMode;

typedef struct {
    Color background;
    Color border;
    Color focus_border;
    Color text;
    Color cursor;
    float radius;
    int padding_x;
    int padding_y;
} TextInputStyle;

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
} ButtonSpec;

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
    TextInputStyle style;
} TextInputProps;

typedef int (*TextInputFilter)(int codepoint, void *user_data);

typedef struct {
    char *text;
    size_t text_size;
    int *cursor_position;
    int max_codepoints;
    TextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
} TextEdit;

typedef struct {
    Rectangle bounds;
    char *text;
    size_t text_size;
    int *cursor_position;
    int *focused;
    int max_codepoints;
    int font;
    int focus_id;
    TextInputStyle style;
    TextInputFilter filter;
    void *filter_user_data;
    int *commit_pressed;
    int secure;
    int read_only;
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
    SyntaxMode syntax;
    TextInputStyle style;
    TextInputFilter filter;
    void *filter_user_data;
    int content_version;
    int read_only;
    int wrap;
} TextAreaProps;

typedef enum {
    RichTextToolBold = 1 << 0,
    RichTextToolItalic = 1 << 1,
    RichTextToolUnderline = 1 << 2,
    RichTextToolHeading = 1 << 3,
    RichTextToolBulletList = 1 << 4,
    RichTextToolNumberedList = 1 << 5,
    RichTextToolQuote = 1 << 6,
    RichTextToolCode = 1 << 7,
    RichTextToolLink = 1 << 8
} RichTextTool;

#define RICH_TEXT_TOOLS_DEFAULT \
    (RichTextToolBold | RichTextToolItalic | RichTextToolUnderline | \
     RichTextToolHeading | RichTextToolBulletList | RichTextToolNumberedList | \
     RichTextToolQuote | RichTextToolCode | RichTextToolLink)

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
    TextInputStyle style;
    TextInputStyle toolbar_style;
    unsigned int tools;
    int content_version;
    int read_only;
    int wrap;
} RichTextEditorProps;

typedef struct {
    Rectangle bounds;
    const char *text;
    int font;
    TextInputStyle style;
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

int EditText(TextEdit edit);
void QueueTextInputCodepoint(int codepoint);
void QueueTextInputBackspace(void);
void QueueTextInputEnter(void);
int GetTextAreaSelection(int focus_id, int *start, int *end);
void SetTextAreaSelection(int focus_id, int anchor, int cursor);

int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);

int RenderButton(ButtonSpec button);
int RenderStyledButton(int x, int y, int w, int h, const char *label,
                       ButtonStyle style, int disabled, int *hover);

void SetUIDropdownClipTop(int top);
void SetUIDropdownClipBottom(int bottom);

#endif

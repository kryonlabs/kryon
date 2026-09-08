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

typedef enum ButtonTone {
    ButtonToneNeutral,
    ButtonToneAccent,
    ButtonToneDanger,
    ButtonToneSuccess,
    ButtonToneWarning
} ButtonTone;

typedef enum ButtonEmphasis {
    ButtonEmphasisFilled,
    ButtonEmphasisSoft,
    ButtonEmphasisOutline,
    ButtonEmphasisGhost,
    ButtonEmphasisLink
} ButtonEmphasis;

typedef enum ControlSize {
    ControlSizeSmall,
    ControlSizeMedium,
    ControlSizeLarge
} ControlSize;

typedef enum IconPlacement {
    IconPlacementLeading,
    IconPlacementTrailing
} IconPlacement;

typedef enum ButtonState {
    ButtonStateAuto,
    ButtonStateNormal,
    ButtonStateHover,
    ButtonStatePressed,
    ButtonStateFocus,
    ButtonStateDisabled,
    ButtonStateLoading,
    ButtonStateSelected
} ButtonState;

typedef enum StyleField {
    StyleBackground    = 1u << 0,
    StyleForeground    = 1u << 1,
    StyleBorder        = 1u << 2,
    StyleFocus         = 1u << 3,
    StyleRadius        = 1u << 4,
    StyleBorderWidth   = 1u << 5,
    StyleOpacity       = 1u << 6,
    StylePaddingX      = 1u << 7,
    StylePaddingY      = 1u << 8,
    StyleGap           = 1u << 9,
    StyleFontSize      = 1u << 10,
    StyleIconSize      = 1u << 11,
    StyleContentOffset = 1u << 12
} StyleField;

/* A typed, renderer-independent set of visual properties. `fields` says
 * which values are present, so zero and transparent remain valid overrides. */
typedef struct Style {
    unsigned int fields;
    Color background;
    Color foreground;
    Color border;
    Color focus;
    float radius;
    float border_width;
    float opacity;
    float padding_x;
    float padding_y;
    float gap;
    float font_size;
    float icon_size;
    Vector2 content_offset;
} Style;

/* State entries are partial Style values layered over `normal`. This type is
 * shared by buttons and future interactive nodes. */
typedef struct ControlStyle {
    Style normal;
    Style hover;
    Style pressed;
    Style focused;
    Style disabled;
    Style loading;
    Style selected;
} ControlStyle;

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
    Color focus;
    float radius;
    float border_width;
    float opacity;
    float gap;
    float icon_size;
    Vector2 content_offset;
    ButtonState state;
    int loading;
    int selected;
    int style_resolved;
    ButtonTone tone;
    ButtonEmphasis emphasis;
    ControlStyle style;
    Texture2D icon;
    UIIconType icon_type;
    IconPlacement icon_placement;
    int icon_only;
} ButtonSpec;

Style MergeStyle(Style base, Style overrides);
Style ResolveControlStyle(Style base, ControlStyle control,
                          ButtonState state);

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
typedef struct ThemeMetrics {
    float radius_small;
    float radius_medium;
    float radius_large;
    float radius_pill;
    float border_width;
    float focus_width;
    float focus_gap;
    float space_1;
    float space_2;
    float space_3;
    float space_4;
    float space_5;
    float space_6;
    float control_height_small;
    float control_height_medium;
    float control_height_large;
    float control_padding_small;
    float control_padding_medium;
    float control_padding_large;
    float control_gap;
    float font_size_small;
    float font_size_medium;
    float font_size_large;
    float icon_size_small;
    float icon_size_medium;
    float icon_size_large;
    float shadow_blur;
    float disabled_opacity;
    float transition_fast_ms;
    float transition_normal_ms;
    /* Corner radii in unscaled UI pixels (DPI-scaled at draw time), NOT the
     * 0..0.5 normalized fraction DrawRectangleRounded takes. A fixed radius
     * keeps tall controls from rounding into pills. */
    float control_radius;
    float panel_radius;
    unsigned char control_alpha;
    unsigned char panel_alpha;
    unsigned char title_bar_alpha;
    unsigned char border_alpha;
    unsigned char shadow_alpha;
    unsigned char shine_alpha;
    int bevel_enabled;
    int touch_target_min;
    int shadow_offset_y;
} ThemeMetrics;

typedef struct ThemeScheme {
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
} ThemeScheme;

typedef void (*UIVerticalSliderMarkCallback)(void *user_data, int x, int y,
                                             int h, int min, int max, int value);

typedef struct {
    const char *label;
    const char *font_name;
} DropdownOption;

typedef struct {
    const char *label;
    int disabled;
} SegmentOption;

typedef struct {
    Rectangle bounds;
    int id;
    const SegmentOption *options;
    int option_count;
    int *selected_index;
    int font;
    int gap;
    int height;
    int min_item_width;
    int max_item_width;
    int wrap;
} SegmentedControlProps;

typedef struct {
    int selected_index;
    int clicked_index;
    int changed;
    int height;
} SegmentedControlResult;

typedef struct {
    Rectangle bounds;
    int id;
    int min_value;
    int max_value;
    int *value;
    int font;
    int gap;
    int height;
    int min_item_width;
    int wrap;
} ScoreControlProps;

typedef struct {
    int value;
    int clicked;
    int clicked_value;
    int changed;
    int height;
} ScoreControlResult;

ThemeMetrics GetThemeMetrics(void);
ThemeMetrics GetThemeMetricsForThemeStyle(ThemeStyle style);
ThemeScheme GetThemeScheme(void);
void SetThemeMetrics(ThemeMetrics tokens);
void ClearThemeMetricsOverride(void);

int EditText(TextEdit edit);
void QueueTextInputCodepoint(int codepoint);
void QueueTextInputBackspace(void);
void QueueTextInputEnter(void);
int TextBufferLineAtCursor(const char *text, int cursor);
int TextBufferColumnAtCursor(const char *text, int cursor);
int TextBufferLineCount(const char *text);
int TextBufferToggleLineComment(char *text, int text_size, int *cursor);
int TextBufferIndentLine(char *text, int text_size, int *cursor, int outdent);
int TextBufferBracketMatch(const char *text, int cursor);
Rectangle TextAreaGutter(TextAreaProps area, int gutter_width);
int GetTextAreaSelection(int focus_id, int *start, int *end);
void SetTextAreaSelection(int focus_id, int anchor, int cursor);

int GetUIIconButtonSize(UIIconSize size);
int GetUIIconButtonPadding(UIIconSize size);

int RenderButton(ButtonSpec button);
int GetSegmentedControlHeight(SegmentedControlProps control);
SegmentedControlResult SegmentedControl(SegmentedControlProps control);
int GetScoreControlHeight(ScoreControlProps control);
ScoreControlResult ScoreControl(ScoreControlProps control);


#endif

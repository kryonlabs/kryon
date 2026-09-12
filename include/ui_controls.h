#ifndef UI_CONTROLS_H
#define UI_CONTROLS_H

#include "kryon_compat.generated.h"
#include "theme_style.h"
#include "ui_control_props.generated.h"
#include "ui_button_props.generated.h"
#include "ui_icon_types.h"
#include <stddef.h>

typedef enum {
    UI_ICON_SIZE_TINY,
    UI_ICON_SIZE_SMALL,
    UI_ICON_SIZE_MEDIUM,
    UI_ICON_SIZE_LARGE
} UIIconSize;



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
    ButtonProps props;
    Style paint;
    Color hover_background;
    int style_resolved;
    Rectangle surface_bounds;
    int disclosure;
} ButtonSpec;

Style MergeStyle(Style base, Style overrides);
Style ResolveControlStyle(Style base, ControlStyle control,
                          ButtonState state);

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
    UIIconType icon_type;
    int disabled;
    int separator_before;
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

ThemeMetrics GetThemeMetrics(void);
ThemeMetrics GetThemeMetricsForThemeStyle(ThemeStyle style);
ThemeScheme GetThemeScheme(void);
void SetFancyEffectsEnabled(int enabled);
int FancyEffectsEnabled(void);
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

int GetSegmentedControlHeight(SegmentedControlProps control);
SegmentedControlResult SegmentedControl(SegmentedControlProps control);


#endif

#ifndef KRYON_CONTROLS_H
#define KRYON_CONTROLS_H

#include "kryon_compat.generated.h"
#include "ui_control_props.generated.h"
#include "ui_button_props.generated.h"
#include "ui_segmented_control_props.generated.h"
#include "ui_text_input_props.generated.h"
#include "ui_icon_types.h"
#include <stddef.h>

typedef enum {
    ICON_SIZE_TINY,
    ICON_SIZE_SMALL,
    ICON_SIZE_MEDIUM,
    ICON_SIZE_LARGE
} IconSize;



Style MergeStyle(Style base, Style overrides);

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

/* Public control metric tokens. Visual style selection belongs to KSS packs;
 * apps override these metrics only when they need full control. */
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

typedef void (*SliderMarkCallback)(void *user_data, int x, int y, int h,
                                   int min, int max, int value);

ThemeMetrics GetThemeMetrics(void);
ThemeMetrics GetDefaultThemeMetrics(void);
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

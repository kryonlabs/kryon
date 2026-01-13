#ifndef IR_PROPERTIES_H
#define IR_PROPERTIES_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include "ir_layout_types.h"
#include "ir_style_types.h"

// Forward declarations
typedef struct IRGradient IRGradient;
typedef struct IRAnimation IRAnimation;
typedef struct IRTransition IRTransition;

// ============================================================================
// Direction and BiDi Support
// ============================================================================

// CSS Direction Property (separate from flexbox direction)
typedef enum {
    IR_DIRECTION_AUTO = 0,      // Auto-detect from content (default)
    IR_DIRECTION_LTR = 1,       // Left-to-right
    IR_DIRECTION_RTL = 2,       // Right-to-left
    IR_DIRECTION_INHERIT = 3    // Inherit from parent
} IRDirection;

// CSS unicode-bidi Property
typedef enum {
    IR_UNICODE_BIDI_NORMAL = 0,    // Default behavior
    IR_UNICODE_BIDI_EMBED = 1,     // Creates additional level of embedding
    IR_UNICODE_BIDI_ISOLATE = 2,   // Isolates from surrounding text (recommended)
    IR_UNICODE_BIDI_PLAINTEXT = 3  // Each paragraph is independent
} IRUnicodeBidi;

// ============================================================================
// Flexbox Properties
// ============================================================================

typedef struct {
    bool wrap;
    uint32_t gap;
    IRAlignment cross_axis;
    IRAlignment justify_content;
    uint8_t grow;      // Flex grow factor (0-255)
    uint8_t shrink;    // Flex shrink factor (0-255)
    float basis;       // Flex basis in pixels (0 = auto/content)
    uint8_t direction; // Flexbox direction: 0=column, 1=row
    uint8_t base_direction;   // CSS direction property (IRDirection)
    uint8_t unicode_bidi;     // CSS unicode-bidi property (IRUnicodeBidi)
    uint8_t _padding;         // Alignment padding
} IRFlexbox;

// ============================================================================
// Layout Mode
// ============================================================================

typedef enum {
    IR_LAYOUT_MODE_FLEX,        // Flexbox layout (default)
    IR_LAYOUT_MODE_INLINE_FLEX, // Inline flexbox layout
    IR_LAYOUT_MODE_GRID,        // CSS Grid layout
    IR_LAYOUT_MODE_INLINE_GRID, // Inline CSS Grid layout
    IR_LAYOUT_MODE_BLOCK,       // Block layout (stacked)
    IR_LAYOUT_MODE_INLINE,      // Inline layout
    IR_LAYOUT_MODE_INLINE_BLOCK,// Inline-block layout
    IR_LAYOUT_MODE_NONE         // display: none
} IRLayoutMode;

// ============================================================================
// Grid Properties
// ============================================================================

typedef enum {
    IR_GRID_TRACK_AUTO,     // auto
    IR_GRID_TRACK_FR,       // fractional unit (fr)
    IR_GRID_TRACK_PX,       // pixels
    IR_GRID_TRACK_PERCENT,  // percentage
    IR_GRID_TRACK_MIN_CONTENT,  // min-content
    IR_GRID_TRACK_MAX_CONTENT   // max-content
} IRGridTrackType;

typedef struct {
    IRGridTrackType type;
    float value;  // Value for PX, PERCENT, FR
} IRGridTrack;

typedef struct {
    IRGridTrackType min_type;
    float min_value;
    IRGridTrackType max_type;
    float max_value;
} IRGridMinMax;

typedef enum {
    IR_GRID_REPEAT_NONE,       // No repeat (explicit tracks)
    IR_GRID_REPEAT_COUNT,      // repeat(3, 1fr)
    IR_GRID_REPEAT_AUTO_FIT,   // repeat(auto-fit, ...)
    IR_GRID_REPEAT_AUTO_FILL   // repeat(auto-fill, ...)
} IRGridRepeatMode;

typedef struct {
    IRGridRepeatMode mode;
    uint8_t count;
    IRGridTrack track;
    IRGridMinMax minmax;
    bool has_minmax;
} IRGridRepeat;

#define IR_MAX_GRID_TRACKS 12

typedef struct {
    IRGridTrack rows[IR_MAX_GRID_TRACKS];
    IRGridTrack columns[IR_MAX_GRID_TRACKS];
    uint8_t row_count;
    uint8_t column_count;
    IRGridRepeat column_repeat;
    IRGridRepeat row_repeat;
    bool use_column_repeat;
    bool use_row_repeat;
    float row_gap;
    float column_gap;
    IRAlignment justify_items;
    IRAlignment align_items;
    IRAlignment justify_content;
    IRAlignment align_content;
    bool auto_flow_row;
    bool auto_flow_dense;
} IRGrid;

typedef struct {
    int16_t row_start;
    int16_t row_end;
    int16_t column_start;
    int16_t column_end;
    IRAlignment justify_self;
    IRAlignment align_self;
} IRGridItem;

// ============================================================================
// Spacing
// ============================================================================

#define IR_SPACING_AUTO (-999999.0f)

#define IR_SPACING_SET_TOP     (1 << 0)
#define IR_SPACING_SET_RIGHT   (1 << 1)
#define IR_SPACING_SET_BOTTOM  (1 << 2)
#define IR_SPACING_SET_LEFT    (1 << 3)
#define IR_SPACING_SET_ALL     (IR_SPACING_SET_TOP | IR_SPACING_SET_RIGHT | IR_SPACING_SET_BOTTOM | IR_SPACING_SET_LEFT)

typedef struct {
    float top, right, bottom, left;
    uint8_t set_flags;
} IRSpacing;

// ============================================================================
// Border
// ============================================================================

typedef struct {
    float width;
    float width_top;
    float width_right;
    float width_bottom;
    float width_left;
    IRColor color;
    uint8_t radius;              // Legacy single radius value (for backward compatibility)
    uint8_t radius_top_left;     // Individual corner radius (TL, TR, BR, BL)
    uint8_t radius_top_right;
    uint8_t radius_bottom_right;
    uint8_t radius_bottom_left;
    uint8_t radius_flags;        // Bitmask for which corners are explicitly set
} IRBorder;

// Border radius flags for radius_flags bitmask
#define IR_RADIUS_SET_TOP_LEFT     (1 << 0)
#define IR_RADIUS_SET_TOP_RIGHT    (1 << 1)
#define IR_RADIUS_SET_BOTTOM_RIGHT (1 << 2)
#define IR_RADIUS_SET_BOTTOM_LEFT  (1 << 3)
#define IR_RADIUS_SET_ALL          (IR_RADIUS_SET_TOP_LEFT | IR_RADIUS_SET_TOP_RIGHT | IR_RADIUS_SET_BOTTOM_RIGHT | IR_RADIUS_SET_BOTTOM_LEFT)

// ============================================================================
// Typography
// ============================================================================

typedef enum {
    IR_TEXT_ALIGN_LEFT,
    IR_TEXT_ALIGN_RIGHT,
    IR_TEXT_ALIGN_CENTER,
    IR_TEXT_ALIGN_JUSTIFY
} IRTextAlign;

#define IR_TEXT_DECORATION_NONE        0x00
#define IR_TEXT_DECORATION_UNDERLINE   0x01
#define IR_TEXT_DECORATION_OVERLINE    0x02
#define IR_TEXT_DECORATION_LINE_THROUGH 0x04

typedef struct {
    float size;
    IRColor color;
    bool bold;
    bool italic;
    char* family;
    uint16_t weight;
    float line_height;
    float letter_spacing;
    float word_spacing;
    IRTextAlign align;
    uint8_t decoration;
} IRTypography;

// ============================================================================
// Transform
// ============================================================================

typedef struct {
    float translate_x, translate_y;
    float scale_x, scale_y;
    float rotate;
} IRTransform;

// ============================================================================
// Animation
// ============================================================================

typedef enum {
    IR_ANIM_PROP_OPACITY,
    IR_ANIM_PROP_TRANSLATE_X,
    IR_ANIM_PROP_TRANSLATE_Y,
    IR_ANIM_PROP_SCALE_X,
    IR_ANIM_PROP_SCALE_Y,
    IR_ANIM_PROP_ROTATE,
    IR_ANIM_PROP_WIDTH,
    IR_ANIM_PROP_HEIGHT,
    IR_ANIM_PROP_BACKGROUND_COLOR,
    IR_ANIM_PROP_CUSTOM
} IRAnimationProperty;

typedef enum {
    IR_EASING_LINEAR,
    IR_EASING_EASE_IN,
    IR_EASING_EASE_OUT,
    IR_EASING_EASE_IN_OUT,
    IR_EASING_EASE_IN_QUAD,
    IR_EASING_EASE_OUT_QUAD,
    IR_EASING_EASE_IN_OUT_QUAD,
    IR_EASING_EASE_IN_CUBIC,
    IR_EASING_EASE_OUT_CUBIC,
    IR_EASING_EASE_IN_OUT_CUBIC,
    IR_EASING_EASE_IN_BOUNCE,
    IR_EASING_EASE_OUT_BOUNCE
} IREasingType;

#define IR_MAX_KEYFRAME_PROPERTIES 8
#define IR_MAX_KEYFRAMES 16

typedef struct IRKeyframe {
    float offset;
    IREasingType easing;
    struct {
        IRAnimationProperty property;
        float value;
        IRColor color_value;
        bool is_set;
    } properties[IR_MAX_KEYFRAME_PROPERTIES];
    uint8_t property_count;
} IRKeyframe;

struct IRAnimation {
    char* name;
    float duration;
    float delay;
    int32_t iteration_count;
    bool alternate;
    IREasingType default_easing;
    IRKeyframe keyframes[IR_MAX_KEYFRAMES];
    uint8_t keyframe_count;
    float current_time;
    int32_t current_iteration;
    bool is_paused;
};

typedef struct IRTransition {
    IRAnimationProperty property;
    float duration;
    float delay;
    IREasingType easing;
    uint32_t trigger_state;
} IRTransition;

// ============================================================================
// Text Effects
// ============================================================================

typedef enum {
    IR_TEXT_DIR_LTR,
    IR_TEXT_DIR_RTL,
    IR_TEXT_DIR_AUTO
} IRTextDirection;

typedef enum {
    IR_TEXT_OVERFLOW_VISIBLE,
    IR_TEXT_OVERFLOW_CLIP,
    IR_TEXT_OVERFLOW_ELLIPSIS,
    IR_TEXT_OVERFLOW_FADE
} IRTextOverflowType;

typedef enum {
    IR_WHITE_SPACE_NORMAL,      // Default: sequences of whitespace collapse into one
    IR_WHITE_SPACE_NOWRAP,      // Sequences of whitespace collapse, text does not wrap to next line
    IR_WHITE_SPACE_PRE,         // Whitespace is preserved, text only wraps on line breaks
    IR_WHITE_SPACE_PRE_WRAP,    // Whitespace is preserved, text will wrap when necessary
    IR_WHITE_SPACE_PRE_LINE     // Sequences of whitespace collapse, text wraps when necessary
} IRWhiteSpaceType;

typedef enum {
    IR_TEXT_FADE_NONE,
    IR_TEXT_FADE_HORIZONTAL,
    IR_TEXT_FADE_VERTICAL,
    IR_TEXT_FADE_RADIAL
} IRTextFadeType;

typedef struct IRTextShadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    IRColor color;
    bool enabled;
} IRTextShadow;

typedef struct IRTextEffect {
    IRTextOverflowType overflow;
    IRTextFadeType fade_type;
    float fade_length;
    IRTextShadow shadow;
    IRDimension max_width;
    IRTextDirection text_direction;
    IRWhiteSpaceType white_space;   // CSS white-space property
    char* language;
} IRTextEffect;

// Text Layout Result (multi-line text)
typedef struct IRTextLayout {
    uint32_t line_count;                // Number of lines
    float* line_widths;                 // Width of each line
    float* line_heights;                // Height of each line
    uint32_t* line_start_indices;       // Character offset where each line starts
    uint32_t* line_end_indices;         // Character offset where each line ends (exclusive)
    float total_height;                 // Total height of all lines
    float max_line_width;               // Width of widest line
    bool needs_reshape;                 // True if text shaping needed (complex scripts)
    IRTextDirection direction;          // Text direction (LTR/RTL/AUTO)
} IRTextLayout;

// ============================================================================
// Box Shadow and Filters
// ============================================================================

typedef struct IRBoxShadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    float spread_radius;
    IRColor color;
    bool inset;
    bool enabled;
} IRBoxShadow;

typedef enum {
    IR_FILTER_BLUR,
    IR_FILTER_BRIGHTNESS,
    IR_FILTER_CONTRAST,
    IR_FILTER_GRAYSCALE,
    IR_FILTER_HUE_ROTATE,
    IR_FILTER_INVERT,
    IR_FILTER_OPACITY,
    IR_FILTER_SATURATE,
    IR_FILTER_SEPIA
} IRFilterType;

#define IR_MAX_FILTERS 8

typedef struct IRFilter {
    IRFilterType type;
    float value;
} IRFilter;

// ============================================================================
// Position and Overflow
// ============================================================================

typedef enum {
    IR_POSITION_STATIC = 0,     // Default positioning (in flow)
    IR_POSITION_RELATIVE,       // Relative to normal position
    IR_POSITION_ABSOLUTE,       // Relative to nearest positioned ancestor
    IR_POSITION_FIXED,          // Relative to viewport
    IR_POSITION_STICKY          // Sticky positioning
} IRPositionMode;

typedef enum {
    IR_OVERFLOW_VISIBLE,
    IR_OVERFLOW_HIDDEN,
    IR_OVERFLOW_SCROLL,
    IR_OVERFLOW_AUTO
} IROverflowMode;

typedef enum {
    IR_OBJECT_FIT_FILL = 0,
    IR_OBJECT_FIT_CONTAIN,
    IR_OBJECT_FIT_COVER,
    IR_OBJECT_FIT_NONE,
    IR_OBJECT_FIT_SCALE_DOWN
} IRObjectFit;

// ============================================================================
// Responsive Design
// ============================================================================

typedef enum {
    IR_QUERY_MIN_WIDTH,
    IR_QUERY_MAX_WIDTH,
    IR_QUERY_MIN_HEIGHT,
    IR_QUERY_MAX_HEIGHT
} IRQueryType;

typedef struct {
    IRQueryType type;
    float value;
} IRQueryCondition;

#define IR_MAX_BREAKPOINTS 4
#define IR_MAX_QUERY_CONDITIONS 2

typedef struct IRBreakpoint {
    IRQueryCondition conditions[IR_MAX_QUERY_CONDITIONS];
    uint8_t condition_count;
    IRDimension width;
    IRDimension height;
    bool visible;
    float opacity;
    IRLayoutMode layout_mode;
    bool has_layout_mode;
} IRBreakpoint;

typedef enum {
    IR_CONTAINER_TYPE_NORMAL,
    IR_CONTAINER_TYPE_SIZE,
    IR_CONTAINER_TYPE_INLINE_SIZE
} IRContainerType;

// ============================================================================
// Pseudo-class States
// ============================================================================

typedef enum {
    IR_PSEUDO_NONE = 0,
    IR_PSEUDO_HOVER = 1 << 0,
    IR_PSEUDO_ACTIVE = 1 << 1,
    IR_PSEUDO_FOCUS = 1 << 2,
    IR_PSEUDO_DISABLED = 1 << 3,
    IR_PSEUDO_CHECKED = 1 << 4,
    IR_PSEUDO_FIRST_CHILD = 1 << 5,
    IR_PSEUDO_LAST_CHILD = 1 << 6,
    IR_PSEUDO_VISITED = 1 << 7
} IRPseudoState;

#define IR_MAX_PSEUDO_STYLES 8

typedef enum {
    IR_BACKGROUND_CLIP_BORDER_BOX = 0,
    IR_BACKGROUND_CLIP_PADDING_BOX,
    IR_BACKGROUND_CLIP_CONTENT_BOX,
    IR_BACKGROUND_CLIP_TEXT
} IRBackgroundClip;

typedef struct {
    IRPseudoState state;
    IRColor background;
    IRColor text_color;
    IRColor border_color;
    float opacity;
    IRTransform transform;
    bool has_background;
    bool has_text_color;
    bool has_border_color;
    bool has_opacity;
    bool has_transform;
} IRPseudoStyle;

// ============================================================================
// Complete Style Structure
// ============================================================================

typedef struct IRStyle {
    IRDimension width, height;
    IRColor background;
    char* background_image;
    IRBackgroundClip background_clip;
    IRColor text_fill_color;
    IRColor border_color;
    IRBorder border;
    IRSpacing margin, padding;
    IRTypography font;
    IRTransform transform;
    IRAnimation** animations;
    uint32_t animation_count;
    IRTransition** transitions;
    uint32_t transition_count;
    uint32_t z_index;
    bool visible;
    float opacity;
    IRPositionMode position_mode;
    float absolute_x;
    float absolute_y;
    IROverflowMode overflow_x;
    IROverflowMode overflow_y;
    IRObjectFit object_fit;
    IRGridItem grid_item;
    IRTextEffect text_effect;
    IRBoxShadow box_shadow;
    IRFilter filters[IR_MAX_FILTERS];
    uint8_t filter_count;
    IRBreakpoint breakpoints[IR_MAX_BREAKPOINTS];
    uint8_t breakpoint_count;
    IRContainerType container_type;
    char* container_name;
    IRPseudoStyle pseudo_styles[IR_MAX_PSEUDO_STYLES];
    uint8_t pseudo_style_count;
    uint32_t current_pseudo_states;
} IRStyle;

// ============================================================================
// Layout Structure (combines layout mode with spacing)
// ============================================================================

typedef struct {
    IRLayoutMode mode;
    bool display_explicit;
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRFlexbox flex;
    IRGrid grid;
    IRSpacing margin;
    IRSpacing padding;
    float aspect_ratio;
} IRLayout;

#endif // IR_PROPERTIES_H

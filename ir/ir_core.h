#ifndef IR_CORE_H
#define IR_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// IR Component Types
typedef enum {
    IR_COMPONENT_CONTAINER,
    IR_COMPONENT_TEXT,
    IR_COMPONENT_BUTTON,
    IR_COMPONENT_INPUT,
    IR_COMPONENT_CHECKBOX,
    IR_COMPONENT_DROPDOWN,
    IR_COMPONENT_ROW,
    IR_COMPONENT_COLUMN,
    IR_COMPONENT_CENTER,
    IR_COMPONENT_IMAGE,
    IR_COMPONENT_CANVAS,
    IR_COMPONENT_NATIVE_CANVAS,
    IR_COMPONENT_MARKDOWN,
    IR_COMPONENT_SPRITE,
    IR_COMPONENT_TAB_GROUP,
    IR_COMPONENT_TAB_BAR,
    IR_COMPONENT_TAB,
    IR_COMPONENT_TAB_CONTENT,
    IR_COMPONENT_TAB_PANEL,
    // Modal/Overlay components
    IR_COMPONENT_MODAL,
    // Table components
    IR_COMPONENT_TABLE,
    IR_COMPONENT_TABLE_HEAD,
    IR_COMPONENT_TABLE_BODY,
    IR_COMPONENT_TABLE_FOOT,
    IR_COMPONENT_TABLE_ROW,
    IR_COMPONENT_TABLE_CELL,
    IR_COMPONENT_TABLE_HEADER_CELL,
    // Markdown document components
    IR_COMPONENT_HEADING,             // H1-H6 with semantic level
    IR_COMPONENT_PARAGRAPH,           // Text paragraph with wrapping
    IR_COMPONENT_BLOCKQUOTE,          // Quoted text block
    IR_COMPONENT_CODE_BLOCK,          // Fenced code with language tag
    IR_COMPONENT_HORIZONTAL_RULE,     // Thematic break (---, ***, ___)
    IR_COMPONENT_LIST,                // Ordered/unordered list container
    IR_COMPONENT_LIST_ITEM,           // Individual list item
    IR_COMPONENT_LINK,                // Hyperlink <a href="...">
    // Inline semantic components (for rich text)
    IR_COMPONENT_SPAN,                // <span> - inline container
    IR_COMPONENT_STRONG,              // <strong> - bold/important text
    IR_COMPONENT_EM,                  // <em> - emphasized/italic text
    IR_COMPONENT_CODE_INLINE,         // <code> - inline code
    IR_COMPONENT_SMALL,               // <small> - smaller text
    IR_COMPONENT_MARK,                // <mark> - highlighted text
    IR_COMPONENT_CUSTOM,
    // Source structure types (for round-trip codegen)
    IR_COMPONENT_STATIC_BLOCK,        // Static block (compile-time code execution)
    IR_COMPONENT_FOR_LOOP,            // For loop template (compile-time iteration)
    IR_COMPONENT_VAR_DECL,            // Variable declaration (const/let/var)
    // Template placeholder (for docs layout templates)
    IR_COMPONENT_PLACEHOLDER          // Template placeholder ({{name}})
} IRComponentType;

// Dimension Types
// IMPORTANT: IR_DIMENSION_AUTO must be 0 so that memset(0) correctly
// initializes dimension types to AUTO (no constraint)
typedef enum {
    IR_DIMENSION_AUTO = 0,  // Must be 0 for memset compatibility
    IR_DIMENSION_PX,
    IR_DIMENSION_PERCENT,
    IR_DIMENSION_FLEX,
    IR_DIMENSION_VW,        // Viewport width (1vw = 1% of viewport width)
    IR_DIMENSION_VH,        // Viewport height (1vh = 1% of viewport height)
    IR_DIMENSION_VMIN,      // Minimum of vw and vh
    IR_DIMENSION_VMAX,      // Maximum of vw and vh
    IR_DIMENSION_REM,       // Root em (relative to root font size)
    IR_DIMENSION_EM         // Em (relative to parent font size)
} IRDimensionType;

typedef struct {
    IRDimensionType type;
    float value;
} IRDimension;

// ============================================================================
// Layout System - Single-Pass Recursive Architecture
// ============================================================================

// Layout constraints from parent
typedef struct {
    float max_width;   // Available width from parent
    float max_height;  // Available height from parent
    float min_width;   // Minimum allowed width
    float min_height;  // Minimum allowed height
} IRLayoutConstraints;

// Final computed layout (absolute positioning)
typedef struct {
    float x;           // Absolute X position
    float y;           // Absolute Y position
    float width;       // Final width
    float height;      // Final height
    bool valid;        // Layout is current (not dirty)
} IRComputedLayout;

// Complete layout state per component
typedef struct {
    IRComputedLayout computed;          // Final layout (single-pass)
    uint32_t cache_generation;          // For cache invalidation
    bool dirty;                         // Needs recomputation
    bool layout_valid;                  // Layout complete
} IRLayoutState;

// Color Types
typedef enum {
    IR_COLOR_SOLID,
    IR_COLOR_TRANSPARENT,
    IR_COLOR_GRADIENT,
    IR_COLOR_VAR_REF    // Reference to style variable
} IRColorType;

// Style variable ID type (for IR_COLOR_VAR_REF)
typedef uint16_t IRStyleVarId;

// Forward declarations
typedef struct IRGradient IRGradient;
typedef struct IRPropertyBinding IRPropertyBinding;
typedef struct IRSourceStructures IRSourceStructures;
typedef struct IRReactiveManifest IRReactiveManifest;

// Union for color data - either RGBA, style variable reference, or gradient pointer
typedef union IRColorData {
    struct { uint8_t r, g, b, a; };  // Direct RGBA (anonymous for convenience)
    IRStyleVarId var_id;              // Reference to style variable
    IRGradient* gradient;             // Pointer to gradient definition
} IRColorData;

typedef struct IRColor {
    IRColorType type;
    IRColorData data;
    char* var_name;  // CSS variable name (e.g., "--bg") for roundtrip preservation
} IRColor;

// Helper macros for creating IRColor values
#define IR_COLOR_RGBA(R, G, B, A) ((IRColor){ .type = IR_COLOR_SOLID, .data = { .r = (R), .g = (G), .b = (B), .a = (A) } })
#define IR_COLOR_VAR(ID) ((IRColor){ .type = IR_COLOR_VAR_REF, .data = { .var_id = (ID) } })
#define IR_COLOR_GRADIENT(GRAD_PTR) ((IRColor){ .type = IR_COLOR_GRADIENT, .data = { .gradient = (GRAD_PTR) } })

// Gradient Types
typedef enum {
    IR_GRADIENT_LINEAR,
    IR_GRADIENT_RADIAL,
    IR_GRADIENT_CONIC
} IRGradientType;

// Gradient Color Stop
typedef struct {
    float position;  // 0.0 to 1.0
    uint8_t r, g, b, a;
} IRGradientStop;

// Gradient Definition
struct IRGradient {
    IRGradientType type;
    IRGradientStop stops[8];  // Maximum 8 color stops
    uint8_t stop_count;
    float angle;  // For linear gradients (degrees)
    float center_x, center_y;  // For radial/conic (0.0 to 1.0)
};

// Alignment Types
typedef enum {
    IR_ALIGNMENT_START,
    IR_ALIGNMENT_CENTER,
    IR_ALIGNMENT_END,
    IR_ALIGNMENT_STRETCH,
    IR_ALIGNMENT_SPACE_BETWEEN,
    IR_ALIGNMENT_SPACE_AROUND,
    IR_ALIGNMENT_SPACE_EVENLY
} IRAlignment;

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

// Flexbox Properties
typedef struct {
    bool wrap;
    uint32_t gap;
    // NOTE: main_axis was removed - use justify_content instead
    IRAlignment cross_axis;
    IRAlignment justify_content;  // Main-axis alignment (was main_axis)
    uint8_t grow;      // Flex grow factor (0-255)
    uint8_t shrink;    // Flex shrink factor (0-255)
    uint8_t direction; // Flexbox direction: 0=column, 1=row
    uint8_t base_direction;   // CSS direction property (IRDirection)
    uint8_t unicode_bidi;     // CSS unicode-bidi property (IRUnicodeBidi)
    uint8_t _padding;         // Alignment padding
} IRFlexbox;

// Layout Mode
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

// Grid Track Size
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

// Minmax constraint for grid tracks: minmax(min, max)
typedef struct {
    IRGridTrackType min_type;
    float min_value;
    IRGridTrackType max_type;
    float max_value;
} IRGridMinMax;

// Repeat mode for grid templates
typedef enum {
    IR_GRID_REPEAT_NONE,       // No repeat (explicit tracks)
    IR_GRID_REPEAT_COUNT,      // repeat(3, 1fr)
    IR_GRID_REPEAT_AUTO_FIT,   // repeat(auto-fit, ...)
    IR_GRID_REPEAT_AUTO_FILL   // repeat(auto-fill, ...)
} IRGridRepeatMode;

// Grid repeat definition: repeat(mode, track)
typedef struct {
    IRGridRepeatMode mode;
    uint8_t count;             // For REPEAT_COUNT mode
    IRGridTrack track;         // Track to repeat (simple case)
    IRGridMinMax minmax;       // For minmax() inside repeat
    bool has_minmax;           // Using minmax vs simple track
} IRGridRepeat;

// Grid Properties
#define IR_MAX_GRID_TRACKS 12  // Maximum rows/columns

typedef struct {
    // Grid template (explicit tracks)
    IRGridTrack rows[IR_MAX_GRID_TRACKS];
    IRGridTrack columns[IR_MAX_GRID_TRACKS];
    uint8_t row_count;
    uint8_t column_count;

    // Grid template (repeat syntax)
    IRGridRepeat column_repeat;  // For repeat() syntax on columns
    IRGridRepeat row_repeat;     // For repeat() syntax on rows
    bool use_column_repeat;      // Use column_repeat instead of explicit columns
    bool use_row_repeat;         // Use row_repeat instead of explicit rows

    // Gaps
    float row_gap;
    float column_gap;

    // Alignment
    IRAlignment justify_items;   // Horizontal alignment of items within cells
    IRAlignment align_items;     // Vertical alignment of items within cells
    IRAlignment justify_content; // Horizontal alignment of grid within container
    IRAlignment align_content;   // Vertical alignment of grid within container

    // Auto flow
    bool auto_flow_row;  // true = row (default), false = column
    bool auto_flow_dense; // Dense packing algorithm
} IRGrid;

// Grid Item Placement (for child components)
typedef struct {
    int16_t row_start;     // -1 = auto
    int16_t row_end;       // -1 = auto (span 1)
    int16_t column_start;  // -1 = auto
    int16_t column_end;    // -1 = auto (span 1)

    // Alignment overrides for this specific item
    IRAlignment justify_self;  // IR_ALIGNMENT_START = use parent's justify_items
    IRAlignment align_self;    // IR_ALIGNMENT_START = use parent's align_items
} IRGridItem;

// Spacing
// Special value for "auto" margin (e.g., margin: 0 auto for centering)
#define IR_SPACING_AUTO (-999999.0f)

// Flags to track which spacing values were explicitly set
#define IR_SPACING_SET_TOP     (1 << 0)
#define IR_SPACING_SET_RIGHT   (1 << 1)
#define IR_SPACING_SET_BOTTOM  (1 << 2)
#define IR_SPACING_SET_LEFT    (1 << 3)
#define IR_SPACING_SET_ALL     (IR_SPACING_SET_TOP | IR_SPACING_SET_RIGHT | IR_SPACING_SET_BOTTOM | IR_SPACING_SET_LEFT)

typedef struct {
    float top, right, bottom, left;
    uint8_t set_flags;  // Track which values were explicitly set (IR_SPACING_SET_*)
} IRSpacing;

// Border
typedef struct {
    float width;          // Uniform border width (if > 0, applies to all sides)
    float width_top;      // Per-side widths (0 = not set)
    float width_right;
    float width_bottom;
    float width_left;
    IRColor color;
    uint8_t radius;       // Border radius
} IRBorder;

// Text Alignment
typedef enum {
    IR_TEXT_ALIGN_LEFT,
    IR_TEXT_ALIGN_RIGHT,
    IR_TEXT_ALIGN_CENTER,
    IR_TEXT_ALIGN_JUSTIFY
} IRTextAlign;

// Text Decoration Flags (can be combined with bitwise OR)
#define IR_TEXT_DECORATION_NONE        0x00
#define IR_TEXT_DECORATION_UNDERLINE   0x01
#define IR_TEXT_DECORATION_OVERLINE    0x02
#define IR_TEXT_DECORATION_LINE_THROUGH 0x04

// Typography
typedef struct {
    float size;
    IRColor color;
    bool bold;
    bool italic;
    char* family;  // Font family name

    // Extended typography (Phase 3)
    uint16_t weight;           // 100-900 (400=normal, 700=bold), 0=use bold flag
    float line_height;         // Line height multiplier (0=auto, 1.5=1.5x font size)
    float letter_spacing;      // Letter spacing in pixels (0=normal)
    float word_spacing;        // Word spacing in pixels (0=normal)
    IRTextAlign align;         // Text alignment
    uint8_t decoration;        // Text decoration flags (bitfield)
} IRTypography;

// Transform
typedef struct {
    float translate_x, translate_y;
    float scale_x, scale_y;
    float rotate;
} IRTransform;

// Animation Property Types
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

// Easing Functions
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

// Animation Keyframe - property value at specific time
#define IR_MAX_KEYFRAME_PROPERTIES 8

typedef struct IRKeyframe {
    float offset;  // 0.0 to 1.0 (percentage of animation duration)
    IREasingType easing;  // Easing to use FROM this keyframe to next

    // Property values at this keyframe
    struct {
        IRAnimationProperty property;
        float value;  // For numeric properties
        IRColor color_value;  // For color properties
        bool is_set;
    } properties[IR_MAX_KEYFRAME_PROPERTIES];
    uint8_t property_count;
} IRKeyframe;

// Animation Definition (keyframe-based)
#define IR_MAX_KEYFRAMES 16

typedef struct IRAnimation {
    char* name;  // Animation name (for CSS @keyframes)
    float duration;  // Total duration in seconds
    float delay;  // Delay before starting
    int32_t iteration_count;  // -1 = infinite, 0 = none, >0 = count
    bool alternate;  // Alternate direction on each iteration
    IREasingType default_easing;  // Default easing if keyframe doesn't specify

    IRKeyframe keyframes[IR_MAX_KEYFRAMES];
    uint8_t keyframe_count;

    // Runtime state (not serialized)
    float current_time;
    int32_t current_iteration;
    bool is_paused;
} IRAnimation;

// Transition Definition (animate property changes)
typedef struct IRTransition {
    IRAnimationProperty property;  // Which property to transition
    float duration;  // Transition duration
    float delay;  // Delay before transition starts
    IREasingType easing;  // Easing function

    // Transition trigger - which state change triggers this
    uint32_t trigger_state;  // Bitmask of IR_PSEUDO_* states
} IRTransition;

// Text Direction for BiDi support
typedef enum {
    IR_TEXT_DIR_LTR,    // Left-to-right (English, Spanish, etc.)
    IR_TEXT_DIR_RTL,    // Right-to-left (Arabic, Hebrew, etc.)
    IR_TEXT_DIR_AUTO    // Auto-detect from first strong character
} IRTextDirection;

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

// Text Overflow Behavior
typedef enum {
    IR_TEXT_OVERFLOW_VISIBLE,   // Show all text (may overflow bounds)
    IR_TEXT_OVERFLOW_CLIP,      // Hard clip at bounds
    IR_TEXT_OVERFLOW_ELLIPSIS,  // Show "..." when truncated
    IR_TEXT_OVERFLOW_FADE       // Gradient fade at edge (uses text_effect settings)
} IRTextOverflowType;

// Text Fade Direction
typedef enum {
    IR_TEXT_FADE_NONE,
    IR_TEXT_FADE_HORIZONTAL,    // Fade left/right edges
    IR_TEXT_FADE_VERTICAL,      // Fade top/bottom edges
    IR_TEXT_FADE_RADIAL         // Fade from center outward
} IRTextFadeType;

// Text Shadow
typedef struct IRTextShadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    IRColor color;
    bool enabled;
} IRTextShadow;

// Text Effect Properties
typedef struct IRTextEffect {
    IRTextOverflowType overflow;
    IRTextFadeType fade_type;
    float fade_length;          // Pixels to fade (0 = no fade)
    IRTextShadow shadow;
    // Text layout properties
    IRDimension max_width;      // Maximum width for text wrapping (0 = no limit)
    IRTextDirection text_direction;  // Text direction (LTR/RTL/AUTO)
    char* language;             // Language code for text shaping (e.g., "ar", "hi", "th")
} IRTextEffect;

// Box Shadow (for containers)
typedef struct IRBoxShadow {
    float offset_x;
    float offset_y;
    float blur_radius;
    float spread_radius;        // Positive = expand, negative = contract
    IRColor color;
    bool inset;                 // True = inset shadow, false = drop shadow
    bool enabled;
} IRBoxShadow;

// CSS Filter Types
typedef enum {
    IR_FILTER_BLUR,             // Gaussian blur (value in px)
    IR_FILTER_BRIGHTNESS,       // 0 = black, 1 = normal, >1 = brighter
    IR_FILTER_CONTRAST,         // 0 = gray, 1 = normal, >1 = more contrast
    IR_FILTER_GRAYSCALE,        // 0 = color, 1 = fully grayscale
    IR_FILTER_HUE_ROTATE,       // Rotate hue (value in degrees)
    IR_FILTER_INVERT,           // 0 = normal, 1 = inverted
    IR_FILTER_OPACITY,          // 0 = transparent, 1 = opaque
    IR_FILTER_SATURATE,         // 0 = desaturated, 1 = normal, >1 = more saturated
    IR_FILTER_SEPIA             // 0 = normal, 1 = fully sepia
} IRFilterType;

#define IR_MAX_FILTERS 8        // Maximum filters per component

typedef struct IRFilter {
    IRFilterType type;
    float value;                // Meaning depends on filter type
} IRFilter;

// Layout Constraints
typedef struct {
    IRLayoutMode mode;  // Layout algorithm to use
    bool display_explicit;  // Was display: property explicitly set in CSS?
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRFlexbox flex;     // Used when mode = IR_LAYOUT_MODE_FLEX
    IRGrid grid;        // Used when mode = IR_LAYOUT_MODE_GRID
    IRSpacing margin;
    IRSpacing padding;
    float aspect_ratio;  // Width/height ratio (0 = no constraint, 1.0 = square, 1.777 = 16:9)
} IRLayout;

// Position Mode
typedef enum {
    IR_POSITION_RELATIVE,   // Default: positioned via flexbox/flow layout
    IR_POSITION_ABSOLUTE,   // Positioned at explicit x/y coordinates
    IR_POSITION_FIXED       // Fixed positioning (relative to viewport)
} IRPositionMode;

// Overflow Handling
typedef enum {
    IR_OVERFLOW_VISIBLE,    // Content not clipped (default)
    IR_OVERFLOW_HIDDEN,     // Content clipped, no scrolling
    IR_OVERFLOW_SCROLL,     // Scrollbars always shown
    IR_OVERFLOW_AUTO        // Scrollbars shown only when needed
} IROverflowMode;

// Object-fit mode (for images and videos)
typedef enum {
    IR_OBJECT_FIT_FILL = 0,      // Default: stretch to fill
    IR_OBJECT_FIT_CONTAIN,       // Scale to fit, preserve aspect ratio
    IR_OBJECT_FIT_COVER,         // Scale to cover, preserve aspect ratio
    IR_OBJECT_FIT_NONE,          // No resizing
    IR_OBJECT_FIT_SCALE_DOWN     // Like none or contain, whichever is smaller
} IRObjectFit;

// Media Query / Container Query Types
typedef enum {
    IR_QUERY_MIN_WIDTH,
    IR_QUERY_MAX_WIDTH,
    IR_QUERY_MIN_HEIGHT,
    IR_QUERY_MAX_HEIGHT
} IRQueryType;

typedef struct {
    IRQueryType type;
    float value;  // Breakpoint value in pixels
} IRQueryCondition;

// Breakpoint-specific styles
#define IR_MAX_BREAKPOINTS 4
#define IR_MAX_QUERY_CONDITIONS 2  // e.g., min-width AND max-width

typedef struct IRBreakpoint {
    IRQueryCondition conditions[IR_MAX_QUERY_CONDITIONS];
    uint8_t condition_count;

    // Style overrides for this breakpoint
    IRDimension width;
    IRDimension height;
    bool visible;  // Can hide/show at breakpoints
    float opacity;

    // Layout overrides
    IRLayoutMode layout_mode;
    bool has_layout_mode;  // Whether to override parent layout mode
} IRBreakpoint;

// Container Query Context
typedef enum {
    IR_CONTAINER_TYPE_NORMAL,      // Not a container query context
    IR_CONTAINER_TYPE_SIZE,        // Container query on size
    IR_CONTAINER_TYPE_INLINE_SIZE  // Container query on inline size only
} IRContainerType;

// Pseudo-class States
typedef enum {
    IR_PSEUDO_NONE = 0,
    IR_PSEUDO_HOVER = 1 << 0,      // Mouse hover
    IR_PSEUDO_ACTIVE = 1 << 1,     // Being clicked/pressed
    IR_PSEUDO_FOCUS = 1 << 2,      // Has keyboard focus
    IR_PSEUDO_DISABLED = 1 << 3,   // Disabled state
    IR_PSEUDO_CHECKED = 1 << 4,    // Checked (for checkboxes)
    IR_PSEUDO_FIRST_CHILD = 1 << 5,  // First child of parent
    IR_PSEUDO_LAST_CHILD = 1 << 6,   // Last child of parent
    IR_PSEUDO_VISITED = 1 << 7     // Link visited state
} IRPseudoState;

// Pseudo-class specific styles
typedef struct {
    IRPseudoState state;           // Which pseudo-class this applies to

    // Style overrides when in this state
    IRColor background;
    IRColor text_color;
    IRColor border_color;
    float opacity;
    IRTransform transform;         // For hover effects like scale/translate

    bool has_background;
    bool has_text_color;
    bool has_border_color;
    bool has_opacity;
    bool has_transform;
} IRPseudoStyle;

#define IR_MAX_PSEUDO_STYLES 8  // Support multiple pseudo-class styles per component

// Background clipping mode for gradient text effects
typedef enum {
    IR_BACKGROUND_CLIP_BORDER_BOX = 0,
    IR_BACKGROUND_CLIP_PADDING_BOX,
    IR_BACKGROUND_CLIP_CONTENT_BOX,
    IR_BACKGROUND_CLIP_TEXT           // For gradient text (background-clip: text)
} IRBackgroundClip;

// Style Properties
typedef struct IRStyle {
    IRDimension width, height;
    IRColor background;
    char* background_image;           // For gradient strings like "linear-gradient(...)"
    IRBackgroundClip background_clip; // Background clipping mode
    IRColor text_fill_color;          // For -webkit-text-fill-color
    IRColor border_color;
    IRBorder border;
    IRSpacing margin, padding;
    IRTypography font;
    IRTransform transform;
    IRAnimation** animations;  // Array of pointers to animations
    uint32_t animation_count;
    IRTransition** transitions;  // Array of pointers to transitions
    uint32_t transition_count;
    uint32_t z_index;
    bool visible;
    float opacity;
    // Positioning
    IRPositionMode position_mode;
    float absolute_x;
    float absolute_y;
    // Overflow handling
    IROverflowMode overflow_x;
    IROverflowMode overflow_y;
    // Object-fit (for images/videos)
    IRObjectFit object_fit;
    // Grid item placement (when parent uses grid layout)
    IRGridItem grid_item;
    // Text effects
    IRTextEffect text_effect;
    // Box shadow
    IRBoxShadow box_shadow;
    // CSS Filters
    IRFilter filters[IR_MAX_FILTERS];
    uint8_t filter_count;
    // Responsive design
    IRBreakpoint breakpoints[IR_MAX_BREAKPOINTS];
    uint8_t breakpoint_count;
    // Container query context (if this component is a container)
    IRContainerType container_type;
    char* container_name;  // Optional name for nested container queries
    // Pseudo-class styles
    IRPseudoStyle pseudo_styles[IR_MAX_PSEUDO_STYLES];
    uint8_t pseudo_style_count;
    // Current active pseudo-states (bitfield)
    uint32_t current_pseudo_states;
} IRStyle;

// Event Types
typedef enum {
    // Core events (0-99) - hardcoded for performance
    IR_EVENT_CLICK = 0,
    IR_EVENT_HOVER = 1,
    IR_EVENT_FOCUS = 2,
    IR_EVENT_BLUR = 3,
    IR_EVENT_TEXT_CHANGE = 4,  // Text input change event (for Input components)
    IR_EVENT_KEY = 5,
    IR_EVENT_SCROLL = 6,
    IR_EVENT_TIMER = 7,
    IR_EVENT_CUSTOM = 8,

    // Plugin event range (100-255) - dynamically registered
    IR_EVENT_PLUGIN_START = 100,
    IR_EVENT_PLUGIN_END = 255
} IREventType;

// Logic Source Types
typedef enum {
    IR_LOGIC_NIM,
    IR_LOGIC_C,
    IR_LOGIC_LUA,
    IR_LOGIC_WASM,
    IR_LOGIC_NATIVE
} LogicSourceType;

// Handler Source - stores actual source code for language-specific handlers
// Used to preserve function source through KIR for roundtrip and web embedding
typedef struct IRHandlerSource {
    char* language;      // "lua", "nim", "javascript", etc.
    char* code;          // The actual function source code
    char* file;          // Source file name (for debugging/roundtrip)
    int line;            // Source line number
} IRHandlerSource;

// Event Handler
typedef struct IREvent {
    IREventType type;
    char* event_name;    // NEW: String name for plugin events (NULL for core events)
    char* logic_id;      // References IRLogic (legacy, for Nim/C callbacks)
    char* handler_data;  // Event-specific data
    uint32_t bytecode_function_id;  // References bytecode function in IRMetadata (0 = none)
    IRHandlerSource* handler_source; // NEW: Embedded handler source code for web/roundtrip
    struct IREvent* next;
} IREvent;

// Business Logic Container
typedef struct IRLogic {
    char* id;
    char* source_code;
    LogicSourceType type;
    struct IRLogic* next;
} IRLogic;

// Dirty flags for efficient incremental layout
typedef enum {
    IR_DIRTY_NONE = 0,
    IR_DIRTY_STYLE = 1 << 0,     // Style properties changed
    IR_DIRTY_LAYOUT = 1 << 1,    // Layout needs recalculation
    IR_DIRTY_CHILDREN = 1 << 2,  // Children added/removed
    IR_DIRTY_CONTENT = 1 << 3,   // Text content changed
    IR_DIRTY_SUBTREE = 1 << 4,   // Descendant needs layout
    IR_DIRTY_RENDER = 1 << 5     // Visual-only changes (no layout recalc needed)
} IRDirtyFlags;

// Layout cache for performance optimization
typedef struct {
    bool dirty;                          // Cache invalid flag
    float cached_intrinsic_width;        // Cached intrinsic width
    float cached_intrinsic_height;       // Cached intrinsic height
    uint32_t cache_generation;           // Generation counter for invalidation
} IRLayoutCache;

// Rendered bounds cache (filled during layout/render)
typedef struct IRRenderedBounds {
    float x, y, width, height;
    bool valid;  // true if bounds have been calculated
} IRRenderedBounds;

// Forward declaration for tab drag visuals
struct TabGroupState;

// Dropdown State (stored in IRComponent->custom_data)
typedef struct IRDropdownState {
    char* placeholder;      // Placeholder text when nothing selected
    char** options;         // Array of option strings
    uint32_t option_count;  // Number of options
    int32_t selected_index; // Currently selected index (-1 = none)
    bool is_open;           // Whether dropdown menu is open
    int32_t hovered_index;  // Currently hovered option index (-1 = none)
} IRDropdownState;

// Modal State (stored in IRComponent->custom_data)
typedef struct IRModalState {
    bool is_open;            // Whether modal is visible
    char* title;             // Optional title text (NULL if no title bar)
    uint32_t backdrop_color; // Backdrop color (RGBA, default semi-transparent black)
} IRModalState;

// ============================================================================
// Table Component Structures
// ============================================================================

// Table cell data (for colspan/rowspan and alignment)
typedef struct IRTableCellData {
    uint16_t colspan;           // Number of columns this cell spans (default 1)
    uint16_t rowspan;           // Number of rows this cell spans (default 1)
    uint8_t alignment;          // IRAlignment for cell content (horizontal)
    uint8_t vertical_alignment; // IRAlignment for vertical positioning
    bool is_spanned;            // True if this cell position is covered by another cell's span
    uint32_t spanned_by_id;     // ID of the cell that spans over this position (if is_spanned)
} IRTableCellData;

// Table column definition (for auto-sizing and column properties)
typedef struct IRTableColumnDef {
    IRDimension min_width;      // Minimum column width
    IRDimension max_width;      // Maximum column width
    IRDimension width;          // Explicit width (if set)
    IRAlignment alignment;      // Default horizontal alignment for column
    bool auto_size;             // True if width should be calculated from content
} IRTableColumnDef;

// Table styling options
typedef struct IRTableStyle {
    IRColor header_background;    // Background color for header cells
    IRColor even_row_background;  // Background for even rows (striped)
    IRColor odd_row_background;   // Background for odd rows
    IRColor border_color;         // Color for table borders
    float border_width;           // Width of cell borders in pixels
    float cell_padding;           // Padding inside cells in pixels
    bool show_borders;            // Whether to show cell borders
    bool striped_rows;            // Whether to use alternating row colors
    bool header_sticky;           // Whether header sticks to top on scroll
    bool collapse_borders;        // Border-collapse: collapse (true) vs separate (false)
} IRTableStyle;

// Maximum table dimensions
#define IR_MAX_TABLE_COLUMNS 64
#define IR_MAX_TABLE_ROWS 1024

// Runtime table state (stored in IRComponent->custom_data for Table components)
typedef struct IRTableState {
    // Column definitions
    IRTableColumnDef* columns;
    uint32_t column_count;

    // Calculated column widths (after layout)
    float* calculated_widths;

    // Calculated row heights (after layout)
    float* calculated_heights;

    // Row information
    uint32_t row_count;
    uint32_t header_row_count;   // Number of rows in TableHead
    uint32_t footer_row_count;   // Number of rows in TableFoot

    // Table styling
    IRTableStyle style;

    // Cell span tracking (for efficient lookup during layout)
    // 2D array: span_map[row * column_count + col] = IRTableCellData
    IRTableCellData* span_map;
    uint32_t span_map_rows;
    uint32_t span_map_cols;

    // Section component references (for quick access)
    struct IRComponent* head_section;
    struct IRComponent* body_section;
    struct IRComponent* foot_section;

    // Layout cache
    bool layout_valid;
    float cached_total_width;
    float cached_total_height;
    float cached_available_width;   // Input dimensions used for last layout
    float cached_available_height;
} IRTableState;

// ============================================================================
// Markdown Component Structures
// ============================================================================

// Heading data (H1-H6)
typedef struct {
    uint8_t level;          // 1-6 for H1-H6
    char* text;             // Heading text content
    char* id;               // Optional anchor ID for linking (NULL if none)
} IRHeadingData;

// List data
typedef enum {
    IR_LIST_UNORDERED = 0,  // Bullet list (-, *, +)
    IR_LIST_ORDERED = 1     // Numbered list (1., 2., ...)
} IRListType;

typedef struct {
    IRListType type;        // Ordered or unordered
    uint32_t start;         // Starting number (for ordered lists, default 1)
    bool tight;             // Tight vs loose spacing (CommonMark spec)
} IRListData;

// List item data
typedef struct {
    uint32_t number;        // Item number (for ordered lists)
    char* marker;           // Bullet character (-, *, +) or NULL
    bool is_task_item;      // GitHub-style task list item
    bool is_checked;        // Task item checked state
} IRListItemData;

// Blockquote data
typedef struct {
    uint8_t depth;          // Nesting level for nested quotes (1-based)
} IRBlockquoteData;

// Code block data
typedef struct {
    char* language;         // Language tag (e.g., "python", "rust", NULL for none)
    char* code;             // Code content (raw text)
    size_t length;          // Code length in bytes
    bool show_line_numbers; // Enable line numbers
    uint32_t start_line;    // Starting line number (default 1)
    // Syntax highlighting is handled by plugins via web renderer extension point
} IRCodeBlockData;

// Link data
typedef struct {
    char* url;              // Target URL (required)
    char* title;            // Optional title attribute (NULL if none)
    char* target;           // Target window (_blank, _self, _parent, _top)
    char* rel;              // Relationship (noopener, noreferrer, external, etc.)
} IRLinkData;

// Placeholder data (for template placeholders like {{content}}, {{sidebar}})
typedef struct {
    char* name;             // Placeholder name (e.g., "content", "sidebar")
    bool preserve;          // If true, output {{name}} in codegen; if false, must be substituted
} IRPlaceholderData;

// Main IR Component
// Tab-specific data
typedef struct {
    char* title;                   // Tab title text
    bool reorderable;              // Can tabs be reordered via drag-and-drop
    int32_t selected_index;        // Currently selected tab index (for TabGroup)
    uint32_t active_background;    // Active tab background color (RGBA)
    uint32_t text_color;           // Tab text color (RGBA)
    uint32_t active_text_color;    // Active tab text color (RGBA)
} IRTabData;

// CSS Selector Type (for HTML roundtrip fidelity)
typedef enum {
    IR_SELECTOR_NONE = 0,      // No CSS selector (component-only, no styling rule)
    IR_SELECTOR_ELEMENT,       // Element selector: header { }, nav { }
    IR_SELECTOR_CLASS,         // Class selector: .container { }, .hero { }
    IR_SELECTOR_ID             // ID selector: #main { }
} IRSelectorType;

typedef struct IRComponent {
    uint32_t id;
    IRComponentType type;
    IRSelectorType selector_type;  // How to generate CSS selector (element vs class)
    char* tag;           // HTML semantic tag (e.g., "section", "header", "nav")
    char* css_class;     // CSS class name for styling (e.g., "hero", "features")
    IRStyle* style;
    IREvent* events;
    IRLogic* logic;
    struct IRComponent** children;
    uint32_t child_count;
    uint32_t child_capacity;  // Allocated capacity for exponential growth
    IRLayout* layout;
    IRLayoutState* layout_state;  // NEW: Two-pass layout state (replaces dirty_flags)
    struct IRComponent* parent;
    char* text_content;      // For text components (evaluated value)
    char* text_expression;   // For reactive text (e.g., "{{value}}" or "$value")
    char* custom_data;       // For custom components
    char* component_ref;     // For component references (e.g., "Counter")
    char* component_props;   // JSON string of props passed to component instance
    uint32_t owner_instance_id;        // ID of owning component instance (for state isolation)
    char* scope;             // Scope string for variable lookups (e.g., "Counter#0", "Counter#1")
    IRRenderedBounds rendered_bounds;  // Cached layout bounds
    IRLayoutCache layout_cache;        // Performance cache for layout
    uint32_t dirty_flags;              // Dirty tracking for incremental updates
    bool has_active_animations;        // OPTIMIZATION: Track if this subtree has animations (80% reduction)
    IRTextLayout* text_layout;         // Cached multi-line text layout (for IR_COMPONENT_TEXT)
    IRTabData* tab_data;               // Tab-specific data (for tab components)

    // Source preservation fields (for round-trip codegen)
    IRPropertyBinding** property_bindings;  // Property binding metadata
    uint32_t property_binding_count;
    struct {
        char* generated_by;            // ID of source structure that generated this (e.g., "for_1", "static_1")
        uint32_t iteration_index;      // Iteration index if generated by for loop (0-based)
        bool is_template;              // True if this is a template component (not expanded)
    } source_metadata;

    // Conditional rendering support
    char* visible_condition;           // Variable name that controls visibility (e.g., "showMessage")
    bool visible_when_true;            // True if visible when condition is true, false if visible when condition is false

    // Interaction state
    bool is_disabled;                  // True if component is disabled (for buttons, inputs, etc.)
} IRComponent;

// IR Buffer for serialization
typedef struct IRBuffer {
    uint8_t* data;        // Current read/write position
    uint8_t* base;        // Original pointer (for free)
    size_t size;          // Remaining/used bytes
    size_t capacity;      // Total capacity
} IRBuffer;

// Forward declarations
typedef struct IRComponentPool IRComponentPool;
typedef struct IRComponentMap IRComponentMap;

// IR Metadata (application-wide information)
typedef struct IRMetadata {
    uint32_t version;                 // IR format version
    uint32_t component_count;         // Total number of components
    uint32_t max_depth;               // Maximum tree depth

    // Plugin requirements
    char** required_plugins;          // Array of plugin names (e.g., ["canvas", "markdown"])
    uint32_t plugin_count;            // Number of required plugins

    // Window properties (for App component)
    char* window_title;               // Window title (NULL for default)
    int window_width;                 // Window width (0 for default)
    int window_height;                // Window height (0 for default)

    char reserved[16];                // For future expansion
} IRMetadata;

// Source file metadata for round-trip serialization
typedef struct IRSourceMetadata {
    char* source_language;    // Original language: "tsx", "c", "nim", "lua", "kry", "html", "md"
    char* source_file;        // Path to original source file
    char* compiler_version;   // Kryon compiler version (e.g., "kryon-1.0.0")
    char* timestamp;          // ISO8601 timestamp when KIR was generated
} IRSourceMetadata;

// ============================================================================
// Source Preservation Structures (for Kry → KIR → Kry round-trip codegen)
// ============================================================================
// These structures preserve compile-time constructs (static blocks, const
// declarations, compile-time for loops) that are normally expanded during
// compilation. They enable perfect round-trip code generation.

// Property binding metadata (links property value to source expression)
typedef struct IRPropertyBinding {
    char* property_name;        // Property name (e.g., "justifyContent")
    char* source_expr;          // Source expression (e.g., "item.value")
    char* resolved_value;       // Resolved literal value (e.g., "center")
    char* binding_type;         // "static_template" | "const_ref" | "reactive"
} IRPropertyBinding;

// Variable declaration (const/let/var) - compile-time only
typedef struct IRVarDecl {
    char* id;                   // Unique ID (e.g., "const_1")
    char* name;                 // Variable name (e.g., "alignments")
    char* var_type;             // "const" | "let" | "var"
    char* value_type;           // Type hint (e.g., "array", "object", "number")
    char* value_json;           // Value as JSON string
    char* scope;                // "global" | "static:<static_block_id>" | "component:<name>"
} IRVarDecl;

// Compile-time for loop template (ONLY for loops inside static blocks)
// NOTE: Runtime reactive for loops are handled by IRReactiveForLoop (lines 1033-1042)
typedef struct IRForLoopData {
    char* id;                   // Unique ID (e.g., "for_1")
    char* parent_id;            // Parent static block ID
    char* iterator_name;        // Loop variable name (e.g., "item")
    char* collection_ref;       // Variable reference (e.g., "alignments")
    char* collection_expr;      // Full collection expression
    IRComponent* template_component;  // Template component (before expansion)
    uint32_t* expanded_component_ids; // IDs of expanded components in root tree
    uint32_t expanded_count;
} IRForLoopData;

// Static block metadata (compile-time code execution block)
typedef struct IRStaticBlockData {
    char* id;                   // Unique ID (e.g., "static_1")
    uint32_t parent_component_id;  // Parent component ID in tree
    uint32_t* children_ids;     // IDs of components generated by this block
    uint32_t children_count;
    char** var_declaration_ids; // IDs of const/let/var declarations in this block
    uint32_t var_decl_count;
    char** for_loop_ids;        // IDs of compile-time for loops in this block
    uint32_t for_loop_count;
} IRStaticBlockData;

// Container for all source preservation structures
typedef struct IRSourceStructures {
    // Static blocks (compile-time execution)
    IRStaticBlockData** static_blocks;
    uint32_t static_block_count;
    uint32_t static_block_capacity;

    // Variable declarations (const/let/var)
    IRVarDecl** var_decls;
    uint32_t var_decl_count;
    uint32_t var_decl_capacity;

    // Compile-time for loops (only inside static blocks)
    IRForLoopData** for_loops;
    uint32_t for_loop_count;
    uint32_t for_loop_capacity;

    // Next ID counters for generation
    uint32_t next_static_block_id;
    uint32_t next_var_decl_id;
    uint32_t next_for_loop_id;
} IRSourceStructures;

// Forward declaration for stylesheet (defined in ir_stylesheet.h)
typedef struct IRStylesheet IRStylesheet;

// Global IR Context
typedef struct IRContext {
    IRComponent* root;
    IRLogic* logic_list;
    uint32_t next_component_id;
    uint32_t next_logic_id;
    IRComponentPool* component_pool;  // Memory pool for components
    IRComponentMap* component_map;     // Hash map for fast ID lookups
    IRMetadata* metadata;              // Application metadata (including plugin requirements)
    IRReactiveManifest* reactive_manifest;  // Reactive state and bindings
    IRSourceStructures* source_structures;  // Source preservation (for round-trip codegen)
    IRSourceMetadata* source_metadata;      // Source file metadata (language, file path, etc.)
    IRStylesheet* stylesheet;               // Global stylesheet with CSS rules and variables
} IRContext;

// IR Type System Functions
const char* ir_component_type_to_string(IRComponentType type);
const char* ir_event_type_to_string(IREventType type);
const char* ir_logic_type_to_string(LogicSourceType type);
IRComponentType ir_get_component_type(IRComponent* component);
uint32_t ir_get_component_id(IRComponent* component);

// Component Tree Accessors (for Lua FFI)
uint32_t ir_get_child_count(IRComponent* component);
IRComponent* ir_get_child_at(IRComponent* component, uint32_t index);

// IR Layout API (defined in ir_layout.c)
void ir_layout_compute(IRComponent* root, float available_width, float available_height);
void ir_layout_compute_table(IRComponent* table, float available_width, float available_height);
void ir_layout_mark_dirty(IRComponent* component);
void ir_layout_mark_render_dirty(IRComponent* component);
void ir_layout_invalidate_subtree(IRComponent* component);
float ir_get_component_intrinsic_width(IRComponent* component);
float ir_get_component_intrinsic_height(IRComponent* component);

// Text measurement callback for two-pass layout system
// Backend-specific text measurement (SDL3 TTF, terminal, etc.)
typedef void (*IRTextMeasureCallback)(const char* text, float font_size,
                                       float max_width, float* out_width, float* out_height);
void ir_layout_set_text_measure_callback(IRTextMeasureCallback callback);

// Text width estimation (used by component modules)
float ir_get_text_width_estimate(const char* text, float font_size);

// Single-pass recursive layout system
void ir_layout_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                           float parent_x, float parent_y);
void ir_layout_compute_tree(IRComponent* root, float viewport_width, float viewport_height);
IRComputedLayout* ir_layout_get_bounds(IRComponent* component);

// Text Layout API (defined in ir_layout.c)
IRTextLayout* ir_text_layout_create(uint32_t max_lines);
void ir_text_layout_destroy(IRTextLayout* layout);
void ir_text_layout_compute(IRComponent* component, float max_width, float font_size);
float ir_text_layout_get_width(IRTextLayout* layout);
float ir_text_layout_get_height(IRTextLayout* layout);

// Font metrics callback (implemented by backends)
typedef struct {
    float (*get_text_width)(const char* text, uint32_t length, float font_size, const char* font_family);
    float (*get_font_height)(float font_size, const char* font_family);
    float (*get_word_width)(const char* word, uint32_t length, float font_size, const char* font_family);
} IRFontMetrics;

// Global font metrics (set by backend)
extern IRFontMetrics* g_ir_font_metrics;
void ir_set_font_metrics(IRFontMetrics* metrics);

// ============================================================================
// Reactive State Manifest System (for hot reload & state persistence)
// ============================================================================

// Reactive variable value types
typedef enum {
    IR_REACTIVE_TYPE_INT,
    IR_REACTIVE_TYPE_FLOAT,
    IR_REACTIVE_TYPE_STRING,
    IR_REACTIVE_TYPE_BOOL,
    IR_REACTIVE_TYPE_CUSTOM  // For complex types (serialized as JSON)
} IRReactiveVarType;

// Reactive variable value (union for different types)
typedef union {
    int64_t as_int;
    double as_float;
    char* as_string;
    bool as_bool;
} IRReactiveValue;

// Reactive variable descriptor
typedef struct {
    uint32_t id;                    // Unique ID for this reactive variable
    char* name;                     // Variable name (for debugging/reconnection)
    IRReactiveVarType type;         // Type of the value
    IRReactiveValue value;          // Current value
    uint32_t version;               // Version counter (for change tracking)

    // Metadata for reconnection
    char* source_location;          // Source file:line where variable was created
    uint32_t binding_count;         // Number of bindings to components

    // NEW: Declarative metadata for .kir v2.1 serialization
    char* type_string;              // Type as string ("int", "float", "string", "bool", "array<T>")
    char* initial_value_json;       // Initial value as JSON string
    char* scope;                    // Scope ("global", "component:123")
} IRReactiveVarDescriptor;

// Binding type (how the binding updates the component)
typedef enum {
    IR_BINDING_TEXT,           // Updates text content
    IR_BINDING_CONDITIONAL,    // Shows/hides component
    IR_BINDING_ATTRIBUTE,      // Updates a component attribute
    IR_BINDING_FOR_LOOP,       // Rebuilds for-loop items
    IR_BINDING_CUSTOM          // Custom update procedure (language-specific)
} IRBindingType;

// Reactive binding (links component to reactive variable)
typedef struct {
    uint32_t component_id;          // ID of the bound component
    uint32_t reactive_var_id;       // ID of the reactive variable
    IRBindingType binding_type;     // Type of binding
    char* expression;               // Expression string (e.g., "$value", "count > 5")
    char* update_code;              // Language-specific update code (optional)
} IRReactiveBinding;

// Reactive conditional (for if/else components)
typedef struct {
    uint32_t component_id;          // Component that is conditionally shown
    char* condition;                // Condition expression (e.g., "count > 5")
    uint32_t* dependent_var_ids;    // Array of reactive var IDs this depends on
    uint32_t dependent_var_count;
    bool last_eval_result;          // Last evaluation result (for caching)
    bool suspended;                 // If true, don't re-initialize (for tab switching)
    // Branch children IDs (for serialization/codegen round-trip)
    uint32_t* then_children_ids;    // Component IDs in then-branch
    uint32_t then_children_count;
    uint32_t* else_children_ids;    // Component IDs in else-branch
    uint32_t else_children_count;
} IRReactiveConditional;

// Reactive for-loop (for dynamic lists)
typedef struct {
    uint32_t parent_component_id;   // Parent container
    char* collection_expr;          // Collection expression (e.g., "items")
    uint32_t collection_var_id;     // Reactive var ID for the collection
    char* loop_variable_name;       // Loop variable name (e.g., "item", "todo")
    char* item_template;            // Template for each item (language-specific)
    uint32_t* child_component_ids;  // Current child components
    uint32_t child_count;
} IRReactiveForLoop;

// Component property definition (for custom components)
typedef struct {
    char* name;                     // Prop name (e.g., "initialValue")
    char* type;                     // Type as string (e.g., "int", "string")
    char* default_value;            // Default value as JSON string
} IRComponentProp;

// Component state variable definition
typedef struct {
    char* name;                     // State var name (e.g., "value")
    char* type;                     // Type as string
    char* initial_expr;             // Initial expression (e.g., "{{initialValue}}")
} IRComponentStateVar;

// Custom component definition
typedef struct {
    char* name;                     // Component name (e.g., "Counter")
    IRComponentProp* props;         // Array of prop definitions
    uint32_t prop_count;
    IRComponentStateVar* state_vars; // Array of state variable definitions
    uint32_t state_var_count;
    IRComponent* template_root;     // Template component tree
} IRComponentDefinition;

// Source code entry (for round-trip serialization)
typedef struct {
    char* lang;  // Language identifier (e.g., "nim", "lua", "js", "c")
    char* code;  // Source code content
} IRSourceEntry;

// Complete reactive manifest
struct IRReactiveManifest {
    // Reactive variables
    IRReactiveVarDescriptor* variables;
    uint32_t variable_count;
    uint32_t variable_capacity;

    // Bindings
    IRReactiveBinding* bindings;
    uint32_t binding_count;
    uint32_t binding_capacity;

    // Conditionals
    IRReactiveConditional* conditionals;
    uint32_t conditional_count;
    uint32_t conditional_capacity;

    // For-loops
    IRReactiveForLoop* for_loops;
    uint32_t for_loop_count;
    uint32_t for_loop_capacity;

    // Component definitions (custom components)
    IRComponentDefinition* component_defs;
    uint32_t component_def_count;
    uint32_t component_def_capacity;

    // Source code entries (for round-trip preservation)
    IRSourceEntry* sources;
    uint32_t source_count;
    uint32_t source_capacity;

    // Metadata
    uint32_t next_var_id;
};

// Reactive manifest management functions
IRReactiveManifest* ir_reactive_manifest_create(void);
void ir_reactive_manifest_destroy(IRReactiveManifest* manifest);

// Add reactive variable to manifest
uint32_t ir_reactive_manifest_add_var(IRReactiveManifest* manifest,
                                      const char* name,
                                      IRReactiveVarType type,
                                      IRReactiveValue value);

// Add binding to manifest
void ir_reactive_manifest_add_binding(IRReactiveManifest* manifest,
                                      uint32_t component_id,
                                      uint32_t reactive_var_id,
                                      IRBindingType binding_type,
                                      const char* expression);

// Add conditional to manifest
void ir_reactive_manifest_add_conditional(IRReactiveManifest* manifest,
                                         uint32_t component_id,
                                         const char* condition,
                                         const uint32_t* dependent_var_ids,
                                         uint32_t dependent_var_count);

// Set branch children IDs for a conditional (for serialization round-trip)
void ir_reactive_manifest_set_conditional_branches(IRReactiveManifest* manifest,
                                                   uint32_t component_id,
                                                   const uint32_t* then_children_ids,
                                                   uint32_t then_children_count,
                                                   const uint32_t* else_children_ids,
                                                   uint32_t else_children_count);

// Add for-loop to manifest
void ir_reactive_manifest_add_for_loop(IRReactiveManifest* manifest,
                                      uint32_t parent_component_id,
                                      const char* collection_expr,
                                      uint32_t collection_var_id);

// Find reactive variable by name
IRReactiveVarDescriptor* ir_reactive_manifest_find_var(IRReactiveManifest* manifest,
                                                       const char* name);

// Get reactive variable by ID
IRReactiveVarDescriptor* ir_reactive_manifest_get_var(IRReactiveManifest* manifest,
                                                      uint32_t var_id);

// Update reactive variable value
bool ir_reactive_manifest_update_var(IRReactiveManifest* manifest,
                                     uint32_t var_id,
                                     IRReactiveValue new_value);

// NEW: Set declarative metadata for .kir v2.1 serialization
void ir_reactive_manifest_set_var_metadata(IRReactiveManifest* manifest,
                                           uint32_t var_id,
                                           const char* type_string,
                                           const char* initial_value_json,
                                           const char* scope);

// Print manifest (for debugging)
void ir_reactive_manifest_print(IRReactiveManifest* manifest);

// Add component definition to manifest
void ir_reactive_manifest_add_component_def(IRReactiveManifest* manifest,
                                            const char* name,
                                            IRComponentProp* props,
                                            uint32_t prop_count,
                                            IRComponentStateVar* state_vars,
                                            uint32_t state_var_count,
                                            IRComponent* template_root);

// Find component definition by name
IRComponentDefinition* ir_reactive_manifest_find_component_def(IRReactiveManifest* manifest,
                                                               const char* name);

// Add source code to manifest (for round-trip preservation)
void ir_reactive_manifest_add_source(IRReactiveManifest* manifest,
                                     const char* lang,
                                     const char* code);

// Get source code by language
const char* ir_reactive_manifest_get_source(IRReactiveManifest* manifest,
                                            const char* lang);

// ============================================================================
// Source Structures API (for Kry → KIR → Kry round-trip codegen)
// ============================================================================

// Create/destroy source structures container
IRSourceStructures* ir_source_structures_create(void);
void ir_source_structures_destroy(IRSourceStructures* ss);

// Static block management
IRStaticBlockData* ir_source_structures_add_static_block(IRSourceStructures* ss,
                                                          uint32_t parent_component_id);
void ir_static_block_add_child(IRStaticBlockData* block, uint32_t child_id);
void ir_static_block_add_var_decl(IRStaticBlockData* block, const char* var_decl_id);
void ir_static_block_add_for_loop(IRStaticBlockData* block, const char* for_loop_id);

// Variable declaration management
IRVarDecl* ir_source_structures_add_var_decl(IRSourceStructures* ss,
                                              const char* name,
                                              const char* var_type,
                                              const char* value_json,
                                              const char* scope);

// Compile-time for loop management (static blocks only)
IRForLoopData* ir_source_structures_add_for_loop(IRSourceStructures* ss,
                                                  const char* parent_static_id,
                                                  const char* iterator_name,
                                                  const char* collection_ref,
                                                  IRComponent* template_component);
void ir_for_loop_add_expanded_component(IRForLoopData* loop, uint32_t component_id);

// Property binding management
IRPropertyBinding* ir_component_add_property_binding(IRComponent* component,
                                                      const char* property_name,
                                                      const char* source_expr,
                                                      const char* resolved_value,
                                                      const char* binding_type);

// Lookup functions
IRStaticBlockData* ir_source_structures_find_static_block(IRSourceStructures* ss, const char* id);
IRVarDecl* ir_source_structures_find_var_decl(IRSourceStructures* ss, const char* id);
IRForLoopData* ir_source_structures_find_for_loop(IRSourceStructures* ss, const char* id);

#endif // IR_CORE_H

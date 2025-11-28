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
    IR_COMPONENT_MARKDOWN,
    IR_COMPONENT_CUSTOM
} IRComponentType;

// Dimension Types
typedef enum {
    IR_DIMENSION_PX,
    IR_DIMENSION_PERCENT,
    IR_DIMENSION_AUTO,
    IR_DIMENSION_FLEX
} IRDimensionType;

typedef struct {
    IRDimensionType type;
    float value;
} IRDimension;

// Color Types
typedef enum {
    IR_COLOR_SOLID,
    IR_COLOR_TRANSPARENT,
    IR_COLOR_GRADIENT,
    IR_COLOR_VAR_REF    // Reference to style variable
} IRColorType;

// Style variable ID type (for IR_COLOR_VAR_REF)
typedef uint16_t IRStyleVarId;

// Union for color data - either RGBA or style variable reference
typedef union IRColorData {
    struct { uint8_t r, g, b, a; };  // Direct RGBA (anonymous for convenience)
    IRStyleVarId var_id;              // Reference to style variable
} IRColorData;

typedef struct IRColor {
    IRColorType type;
    IRColorData data;
} IRColor;

// Helper macros for creating IRColor values
#define IR_COLOR_RGBA(R, G, B, A) ((IRColor){ .type = IR_COLOR_SOLID, .data = { .r = (R), .g = (G), .b = (B), .a = (A) } })
#define IR_COLOR_VAR(ID) ((IRColor){ .type = IR_COLOR_VAR_REF, .data = { .var_id = (ID) } })

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
typedef struct {
    IRGradientType type;
    IRGradientStop stops[8];  // Maximum 8 color stops
    uint8_t stop_count;
    float angle;  // For linear gradients (degrees)
    float center_x, center_y;  // For radial/conic (0.0 to 1.0)
} IRGradient;

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

// Flexbox Properties
typedef struct {
    bool wrap;
    uint32_t gap;
    IRAlignment main_axis;
    IRAlignment cross_axis;
    IRAlignment justify_content;
    uint8_t grow;      // Flex grow factor (0-255)
    uint8_t shrink;    // Flex shrink factor (0-255)
    uint8_t direction; // Layout direction: 0=column, 1=row
} IRFlexbox;

// Spacing
typedef struct {
    float top, right, bottom, left;
} IRSpacing;

// Border
typedef struct {
    float width;
    IRColor color;
    uint8_t radius;  // Border radius
} IRBorder;

// Typography
typedef struct {
    float size;
    IRColor color;
    bool bold;
    bool italic;
    char* family;  // Font family name
} IRTypography;

// Transform
typedef struct {
    float translate_x, translate_y;
    float scale_x, scale_y;
    float rotate;
} IRTransform;

// Animation
typedef enum {
    IR_ANIMATION_FADE,
    IR_ANIMATION_SLIDE,
    IR_ANIMATION_SCALE,
    IR_ANIMATION_CUSTOM
} IRAnimationType;

typedef struct IRAnimation {
    IRAnimationType type;
    float duration;
    float delay;
    bool loop;
    char* custom_data;  // For custom animations
} IRAnimation;

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
} IRTextEffect;

// Layout Constraints
typedef struct {
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRFlexbox flex;
    IRSpacing margin;
    IRSpacing padding;
    float aspect_ratio;  // Width/height ratio (0 = no constraint, 1.0 = square, 1.777 = 16:9)
} IRLayout;

// Position Mode
typedef enum {
    IR_POSITION_RELATIVE,   // Default: positioned via flexbox/flow layout
    IR_POSITION_ABSOLUTE    // Positioned at explicit x/y coordinates
} IRPositionMode;

// Style Properties
typedef struct IRStyle {
    IRDimension width, height;
    IRColor background;
    IRColor border_color;
    IRBorder border;
    IRSpacing margin, padding;
    IRTypography font;
    IRTransform transform;
    IRAnimation* animations;
    uint32_t animation_count;
    uint32_t z_index;
    bool visible;
    float opacity;
    // Absolute positioning
    IRPositionMode position_mode;
    float absolute_x;
    float absolute_y;
    // Text effects
    IRTextEffect text_effect;
} IRStyle;

// Event Types
typedef enum {
    IR_EVENT_CLICK,
    IR_EVENT_HOVER,
    IR_EVENT_FOCUS,
    IR_EVENT_BLUR,
    IR_EVENT_KEY,
    IR_EVENT_SCROLL,
    IR_EVENT_TIMER,
    IR_EVENT_CUSTOM
} IREventType;

// Logic Source Types
typedef enum {
    IR_LOGIC_NIM,
    IR_LOGIC_C,
    IR_LOGIC_LUA,
    IR_LOGIC_WASM,
    IR_LOGIC_NATIVE
} LogicSourceType;

// Event Handler
typedef struct IREvent {
    IREventType type;
    char* logic_id;      // References IRLogic
    char* handler_data;  // Event-specific data
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
    IR_DIRTY_SUBTREE = 1 << 4    // Descendant needs layout
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

// Main IR Component
typedef struct IRComponent {
    uint32_t id;
    IRComponentType type;
    char* tag;           // HTML tag for web, custom identifier
    IRStyle* style;
    IREvent* events;
    IRLogic* logic;
    struct IRComponent** children;
    uint32_t child_count;
    IRLayout* layout;
    struct IRComponent* parent;
    char* text_content;  // For text components
    char* custom_data;   // For custom components
    IRRenderedBounds rendered_bounds;  // Cached layout bounds
    IRLayoutCache layout_cache;        // Performance cache for layout
    uint32_t dirty_flags;              // Dirty tracking for incremental updates
} IRComponent;

// IR Buffer for serialization
typedef struct IRBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
} IRBuffer;

// Forward declarations
typedef struct IRComponentPool IRComponentPool;
typedef struct IRComponentMap IRComponentMap;

// Global IR Context
typedef struct IRContext {
    IRComponent* root;
    IRLogic* logic_list;
    uint32_t next_component_id;
    uint32_t next_logic_id;
    IRComponentPool* component_pool;  // Memory pool for components
    IRComponentMap* component_map;     // Hash map for fast ID lookups
} IRContext;

// IR Type System Functions
const char* ir_component_type_to_string(IRComponentType type);
const char* ir_event_type_to_string(IREventType type);
const char* ir_logic_type_to_string(LogicSourceType type);

// IR Layout API (defined in ir_layout.c)
void ir_layout_compute(IRComponent* root, float available_width, float available_height);
void ir_layout_mark_dirty(IRComponent* component);
void ir_layout_invalidate_subtree(IRComponent* component);
void ir_layout_invalidate_cache(IRComponent* component);
float ir_get_component_intrinsic_width(IRComponent* component);
float ir_get_component_intrinsic_height(IRComponent* component);

#endif // IR_CORE_H

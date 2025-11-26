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
    IR_COLOR_GRADIENT
} IRColorType;

typedef struct {
    IRColorType type;
    uint8_t r, g, b, a;
} IRColor;

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

// Layout Constraints
typedef struct {
    IRDimension min_width, min_height;
    IRDimension max_width, max_height;
    IRFlexbox flex;
    IRSpacing margin;
    IRSpacing padding;
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
} IRComponent;

// IR Buffer for serialization
typedef struct IRBuffer {
    uint8_t* data;
    size_t size;
    size_t capacity;
} IRBuffer;

// Global IR Context
typedef struct IRContext {
    IRComponent* root;
    IRLogic* logic_list;
    uint32_t next_component_id;
    uint32_t next_logic_id;
} IRContext;

// IR Type System Functions
const char* ir_component_type_to_string(IRComponentType type);
const char* ir_event_type_to_string(IREventType type);
const char* ir_logic_type_to_string(LogicSourceType type);

#endif // IR_CORE_H

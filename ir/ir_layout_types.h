#ifndef IR_LAYOUT_TYPES_H
#define IR_LAYOUT_TYPES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Dimension Types
// ============================================================================

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
// Alignment Types
// ============================================================================

typedef enum {
    IR_ALIGNMENT_START,
    IR_ALIGNMENT_CENTER,
    IR_ALIGNMENT_END,
    IR_ALIGNMENT_STRETCH,
    IR_ALIGNMENT_SPACE_BETWEEN,
    IR_ALIGNMENT_SPACE_AROUND,
    IR_ALIGNMENT_SPACE_EVENLY,
    IR_ALIGNMENT_BASELINE,       // Baseline alignment (for flex/grid items)
    IR_ALIGNMENT_AUTO            // Automatic alignment (default)
} IRAlignment;

// ============================================================================
// Layout System
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

// Forward declaration
typedef struct IRComponent IRComponent;

// Dirty flags for efficient incremental layout
// (moved here from ir_core.h for consolidation)
typedef enum {
    IR_DIRTY_NONE = 0,
    IR_DIRTY_STYLE = 1 << 0,     // Style properties changed
    IR_DIRTY_LAYOUT = 1 << 1,    // Layout needs recalculation
    IR_DIRTY_CHILDREN = 1 << 2,  // Children added/removed
    IR_DIRTY_CONTENT = 1 << 3,   // Text content changed
    IR_DIRTY_SUBTREE = 1 << 4,   // Descendant needs layout
    IR_DIRTY_RENDER = 1 << 5     // Visual-only changes (no layout recalc needed)
} IRDirtyFlags;

// Complete layout state per component
typedef struct {
    IRComputedLayout computed;          // Final layout (single-pass)
    uint32_t cache_generation;          // For cache invalidation
    bool dirty;                         // Legacy: Needs recomputation
    bool layout_valid;                  // Layout complete
    IRDirtyFlags dirty_flags;           // Consolidated dirty tracking
} IRLayoutState;

// ============================================================================
// Dimension Utilities
// ============================================================================

IRDimension ir_dimension_px(float value);
IRDimension ir_dimension_percent(float value);
IRDimension ir_dimension_auto(void);
float ir_dimension_resolve(IRDimension dim, float parent_size);

#endif // IR_LAYOUT_TYPES_H

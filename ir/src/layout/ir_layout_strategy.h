#ifndef IR_LAYOUT_STRATEGY_H
#define IR_LAYOUT_STRATEGY_H

#include "../../include/ir_types.h"
#include "../../include/ir_layout.h"

// Forward declarations
typedef struct IRComponent IRComponent;

// ============================================================================
// Layout Strategy Interface
// ============================================================================

/**
 * IRLayoutStrategy - Component-specific layout behavior
 *
 * Each component type can register a strategy that provides:
 * - measure: Calculate the component's intrinsic size
 * - layout_children: Position children within the component
 * - intrinsic_size: Get content-driven size (optional)
 */
typedef struct IRLayoutStrategy {
    IRComponentType type;

    // Measure component size given constraints
    void (*measure)(IRComponent* comp, IRLayoutConstraints* constraints);

    // Layout children (position them within the component)
    void (*layout_children)(IRComponent* comp);

    // Get intrinsic size (for content-driven sizing)
    void (*intrinsic_size)(IRComponent* comp, float* width, float* height);

} IRLayoutStrategy;

// ============================================================================
// Strategy Registry
// ============================================================================

// Register a layout strategy
void ir_layout_register_strategy(IRLayoutStrategy* strategy);

// Get strategy for component type
IRLayoutStrategy* ir_layout_get_strategy(IRComponentType type);

// Check if component has custom layout
bool ir_layout_has_strategy(IRComponentType type);

// Initialize all built-in layout strategies
void ir_layout_strategies_init(void);

#endif // IR_LAYOUT_STRATEGY_H

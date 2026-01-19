#ifndef STYLE_ANALYZER_H
#define STYLE_ANALYZER_H

#include "ir_core.h"
#include "ir_logic.h"
#include <stdbool.h>

/**
 * Style Analysis Module
 *
 * Determines when IR components need CSS classes and IDs in generated HTML.
 *
 * Core principle: Only add markup when it provides value beyond browser defaults.
 * - No class if component uses browser default styling
 * - No ID unless component has event handlers
 */

/**
 * Style analysis result
 */
typedef struct {
    bool needs_css;           // Component has custom styling (non-default values)
    bool needs_id;            // Component has event handlers or reactive bindings
    char* suggested_class;    // Natural class name (caller must free)
} StyleAnalysis;

/**
 * Analyze a component's styling requirements
 *
 * @param component The IR component to analyze
 * @param logic_block Optional logic block to check for event handlers
 * @return StyleAnalysis struct (caller must free with style_analysis_free)
 */
StyleAnalysis* analyze_component_style(IRComponent* component, IRLogicBlock* logic_block);

/**
 * Check if a style has any non-default values
 *
 * Returns true if ANY property differs from browser/IR defaults:
 * - Colors: not transparent
 * - Dimensions: not auto
 * - Spacing: not zero
 * - Border width: > 0
 * - Typography: custom font/size/weight
 * - Transform: not identity (translate=0, scale=1, rotate=0)
 * - Effects: opacity < 1, filters, shadows, etc.
 *
 * @param style The IRStyle to check
 * @return true if style has custom values, false if all defaults
 */
bool ir_style_has_custom_values(IRStyle* style);

/**
 * Check if a component has event handlers
 *
 * Checks both:
 * - Legacy IREvent linked list (component->events)
 * - Logic block handlers (ir_logic_block_get_handler)
 *
 * @param component The component to check
 * @param logic_block Optional logic block to check for handlers
 * @return true if component has any event handlers
 */
bool has_event_handlers(IRComponent* component, IRLogicBlock* logic_block);

/**
 * Generate a natural class name for a component
 *
 * Priority:
 * 1. Use component->tag if set (user custom class)
 * 2. Use natural semantic names (.button, .row, .column, .center)
 * 3. Fallback to generic names
 *
 * @param component The component to generate a class name for
 * @return Allocated string (caller must free), or NULL if error
 */
char* generate_natural_class_name(IRComponent* component);

/**
 * Free a style analysis struct
 *
 * @param analysis The analysis to free
 */
void style_analysis_free(StyleAnalysis* analysis);

#endif // STYLE_ANALYZER_H

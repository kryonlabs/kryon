#ifndef IR_LAYOUT_DEBUG_H
#define IR_LAYOUT_DEBUG_H

#include "../../include/ir_core.h"
#include <stdbool.h>

// ============================================================================
// Debug Control
// ============================================================================

// Enable/disable layout debugging
void ir_layout_set_debug_enabled(bool enabled);
bool ir_layout_is_debug_enabled(void);

// ============================================================================
// Tree Printing
// ============================================================================

// Print layout tree to stderr or file
void ir_layout_debug_print_tree(IRComponent* root);
void ir_layout_debug_print_component(IRComponent* comp, int depth);

// ============================================================================
// File Logging
// ============================================================================

// Enable logging to file
void ir_layout_debug_enable_file_logging(const char* path);
void ir_layout_debug_close_file(void);

#endif // IR_LAYOUT_DEBUG_H

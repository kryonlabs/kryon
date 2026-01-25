/**
 * Kryon UI Framework - C DSL Implementation
 *
 * Internal implementation for the DSL parent stack management.
 */

#include "kryon_dsl.h"
#include <stddef.h>
#include <stdio.h>

// ============================================================================
// Parent Stack Management
// ============================================================================

#define MAX_PARENT_STACK_DEPTH 128

static IRComponent* _parent_stack[MAX_PARENT_STACK_DEPTH];
static int _parent_stack_top = -1;

/**
 * Push a component onto the parent stack
 */
void _kryon_push_parent(IRComponent* parent) {
    if (_parent_stack_top >= MAX_PARENT_STACK_DEPTH - 1) {
        // Stack overflow - this shouldn't happen in normal use
        return;
    }
    _parent_stack[++_parent_stack_top] = parent;
}

/**
 * Pop a component from the parent stack
 */
void _kryon_pop_parent(void) {
    if (_parent_stack_top < 0) {
        // Stack underflow - this shouldn't happen in normal use
        return;
    }
    _parent_stack_top--;
}

/**
 * Get the current parent component (top of stack)
 * Returns NULL if stack is empty
 */
IRComponent* _kryon_get_current_parent(void) {
    if (_parent_stack_top < 0) {
        return NULL;
    }
    return _parent_stack[_parent_stack_top];
}

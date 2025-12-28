/**
 * Kryon Markdown AST - Implementation
 *
 * Implementation of markdown AST node creation and memory management.
 * These are internal helper functions used by the markdown parser.
 */

#include "ir_markdown_ast.h"
#include "ir_core.h"
#include <stdlib.h>
#include <string.h>

/**
 * Create a new AST node
 */
MdNode* md_node_new(MdBlockType type) {
    MdNode* node = (MdNode*)malloc(sizeof(MdNode));
    if (!node) return NULL;

    memset(node, 0, sizeof(MdNode));
    node->type = type;
    return node;
}

/**
 * Create a new inline element
 */
MdInline* md_inline_new(MdInlineType type) {
    MdInline* inl = (MdInline*)malloc(sizeof(MdInline));
    if (!inl) return NULL;

    memset(inl, 0, sizeof(MdInline));
    inl->type = type;
    return inl;
}

/**
 * Append a child node to a parent
 */
void md_node_append_child(MdNode* parent, MdNode* child) {
    if (!parent || !child) return;

    child->parent = parent;
    child->next = NULL;
    child->prev = parent->last_child;

    if (parent->last_child) {
        parent->last_child->next = child;
    } else {
        parent->first_child = child;
    }

    parent->last_child = child;
}

/**
 * Free an inline element and all its siblings
 */
void md_inline_free(MdInline* inl) {
    // NO-OP: All inlines are allocated from inline_pool in parser_state_t.
    // They will be freed when parser_destroy() is called.
    // Attempting to free individual inlines causes "free(): invalid pointer" crashes.
    (void)inl;  // Suppress unused parameter warning
}

/**
 * Free an AST node and all its children
 */
void md_node_free(MdNode* node) {
    // NO-OP: All nodes are allocated from node_pool in parser_state_t.
    // They will be freed when parser_destroy() is called.
    // Attempting to free individual nodes causes "free(): invalid pointer" crashes.
    (void)node;  // Suppress unused parameter warning
}

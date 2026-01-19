/**
 * @file ir_layout_debug.c
 * @brief Layout debugging utilities
 */

#define _GNU_SOURCE
#include "ir_layout_debug.h"
#include "../include/ir_types.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Debug State
// ============================================================================

static bool g_debug_enabled = false;
static FILE* g_debug_file = NULL;
static int g_pass_count = 0;

// ============================================================================
// Helpers
// ============================================================================

static const char* get_debug_file_path(void) {
    const char* path = getenv("KRYON_LAYOUT_DEBUG_FILE");
    return path ? path : "/tmp/kryon_layout_debug.txt";
}

static void indent(FILE* f, int depth) {
    for (int i = 0; i < depth; i++) {
        fprintf(f, "  ");
    }
}

static void ir_layout_debug_print_component_recursive(IRComponent* comp, int depth);

// ============================================================================
// Debug Control
// ============================================================================

bool ir_layout_is_debug_enabled(void) {
    return g_debug_enabled || getenv("KRYON_DEBUG_LAYOUT");
}

void ir_layout_set_debug_enabled(bool enabled) {
    g_debug_enabled = enabled;
}

// ============================================================================
// File Logging
// ============================================================================

void ir_layout_debug_enable_file_logging(const char* path) {
    if (g_debug_file) {
        fclose(g_debug_file);
    }

    g_debug_file = fopen(path ? path : get_debug_file_path(), "w");
    if (g_debug_file) {
        fprintf(g_debug_file, "=== Kryon Layout Debug ===\n\n");
    }
}

void ir_layout_debug_close_file(void) {
    if (g_debug_file) {
        fclose(g_debug_file);
        g_debug_file = NULL;
    }
}

// ============================================================================
// Tree Printing
// ============================================================================

void ir_layout_debug_print_component(IRComponent* comp, int depth) {
    FILE* out = g_debug_file ? g_debug_file : stderr;

    indent(out, depth);

    if (!comp) {
        fprintf(out, "NULL\n");
        return;
    }

    const char* type_name = ir_component_type_to_string(comp->type);
    fprintf(out, "[%d] %s", comp->id, type_name);

    if (comp->tag) {
        fprintf(out, " tag='%s'", comp->tag);
    }

    if (comp->css_class) {
        fprintf(out, " class='%s'", comp->css_class);
    }

    if (comp->text_content) {
        fprintf(out, " text='%s'", comp->text_content);
    }

    if (comp->layout_state) {
        fprintf(out, " pos=(%.0f,%.0f) size=(%.0f,%.0f)",
            comp->layout_state->computed.x,
            comp->layout_state->computed.y,
            comp->layout_state->computed.width,
            comp->layout_state->computed.height);
    }

    fprintf(out, "\n");
}

static void ir_layout_debug_print_component_recursive(IRComponent* comp, int depth) {
    ir_layout_debug_print_component(comp, depth);

    for (uint32_t i = 0; i < comp->child_count; i++) {
        ir_layout_debug_print_component_recursive(comp->children[i], depth + 1);
    }
}

void ir_layout_debug_print_tree(IRComponent* root) {
    FILE* out = g_debug_file ? g_debug_file : stderr;

    g_pass_count++;
    fprintf(out, "\n=== Layout Pass #%d ===\n", g_pass_count);

    if (!root) {
        fprintf(out, "NULL root\n");
        return;
    }

    ir_layout_debug_print_component_recursive(root, 0);
}

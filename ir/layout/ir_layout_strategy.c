/**
 * @file ir_layout_strategy.c
 * @brief Layout strategy registry implementation
 */

#define _GNU_SOURCE
#include "ir_layout_strategy.h"
#include "../ir_log.h"
#include <stdlib.h>
#include <string.h>

#define MAX_STRATEGIES 32

static IRLayoutStrategy* g_strategies[MAX_STRATEGIES];
static int g_strategy_count = 0;
static bool g_registry_initialized = false;

void ir_layout_register_strategy(IRLayoutStrategy* strategy) {
    if (!strategy) {
        IR_LOG_ERROR("LAYOUT", "Cannot register NULL strategy");
        return;
    }

    if (g_strategy_count >= MAX_STRATEGIES) {
        IR_LOG_ERROR("LAYOUT", "Strategy registry full");
        return;
    }

    g_strategies[g_strategy_count++] = strategy;
    IR_LOG_DEBUG("LAYOUT", "Registered layout strategy for type %d",
        strategy->type);
}

IRLayoutStrategy* ir_layout_get_strategy(IRComponentType type) {
    for (int i = 0; i < g_strategy_count; i++) {
        if (g_strategies[i]->type == type) {
            return g_strategies[i];
        }
    }
    return NULL;  // Use default layout
}

bool ir_layout_has_strategy(IRComponentType type) {
    return ir_layout_get_strategy(type) != NULL;
}

// Forward declarations of strategy registration functions
extern void ir_register_container_layout_strategy(void);
extern void ir_register_text_layout_strategy(void);
extern void ir_register_row_layout_strategy(void);
extern void ir_register_column_layout_strategy(void);
extern void ir_register_image_layout_strategy(void);
extern void ir_register_canvas_layout_strategy(void);
extern void ir_register_foreach_layout_strategy(void);

// Initialize all built-in strategies
void ir_layout_strategies_init(void) {
    if (g_registry_initialized) return;

    ir_register_container_layout_strategy();
    ir_register_text_layout_strategy();
    ir_register_row_layout_strategy();
    ir_register_column_layout_strategy();
    ir_register_image_layout_strategy();
    ir_register_canvas_layout_strategy();
    ir_register_foreach_layout_strategy();

    g_registry_initialized = true;

    IR_LOG_DEBUG("LAYOUT", "Initialized %d layout strategies", g_strategy_count);
}

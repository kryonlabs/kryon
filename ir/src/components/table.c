#include "table.h"
#include "../include/ir_core.h"
#include "../layout/layout_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

void layout_table_single_pass(IRComponent* c, IRLayoutConstraints constraints,
                               float parent_x, float parent_y) {
    if (!c || c->type != IR_COMPONENT_TABLE) return;

    // Ensure layout state exists
    if (!layout_ensure_state(c)) return;

    // Get table state
    IRTableState* state = (IRTableState*)c->custom_data;
    if (!state) {
        // No table state - shouldn't happen if finalize was called
        layout_set_final_with_parent(c, parent_x, parent_y, constraints.max_width, 100.0f);
        return;
    }

    if (layout_is_debug_enabled("Table")) {
        fprintf(stderr, "[TABLE_LAYOUT] Starting layout for table id=%u constraints=%.1fx%.1f\n",
                c->id, constraints.max_width, constraints.max_height);
    }

    // Count rows and columns by traversing the tree
    uint32_t num_rows = 0;
    uint32_t num_cols = 0;

    for (uint32_t section_idx = 0; section_idx < c->child_count; section_idx++) {
        IRComponent* section = c->children[section_idx];
        if (!section) continue;

        // TableHead, TableBody, TableFoot
        for (uint32_t row_idx = 0; row_idx < section->child_count; row_idx++) {
            IRComponent* row = section->children[row_idx];
            if (!row || row->type != IR_COMPONENT_TABLE_ROW) continue;

            num_rows++;
            if (row->child_count > num_cols) {
                num_cols = row->child_count;
            }
        }
    }

    if (layout_is_debug_enabled("Table")) {
        fprintf(stderr, "[TABLE_LAYOUT] Detected %u rows x %u cols\n", num_rows, num_cols);
    }

    // If no rows, return default dimensions
    if (num_rows == 0 || num_cols == 0) {
        layout_set_final_with_parent(c, parent_x, parent_y, constraints.max_width, 50.0f);
        return;
    }

    // Allocate dimension arrays
    float* col_widths = (float*)calloc(num_cols, sizeof(float));
    float* row_heights = (float*)calloc(num_rows, sizeof(float));

    // Calculate cell padding
    float cell_padding = (state->style.cell_padding > 0) ? state->style.cell_padding : 8.0f;

    // Measure cells to determine column widths and row heights
    uint32_t current_row = 0;
    for (uint32_t section_idx = 0; section_idx < c->child_count; section_idx++) {
        IRComponent* section = c->children[section_idx];
        if (!section) continue;

        for (uint32_t row_idx = 0; row_idx < section->child_count; row_idx++) {
            IRComponent* row = section->children[row_idx];
            if (!row || row->type != IR_COMPONENT_TABLE_ROW) continue;

            float max_row_height = 0;

            for (uint32_t col_idx = 0; col_idx < row->child_count && col_idx < num_cols; col_idx++) {
                IRComponent* cell = row->children[col_idx];
                if (!cell) continue;

                // Measure cell content (text)
                float cell_width = 100.0f;  // Default minimum width
                float cell_height = 30.0f;  // Default height

                if (cell->text_content) {
                    float font_size = (cell->style && cell->style->font.size > 0) ? cell->style->font.size : 14.0f;
                    float text_width = ir_get_text_width_estimate(cell->text_content, font_size);
                    cell_width = text_width + cell_padding * 2;
                    cell_height = font_size + cell_padding * 2;
                }

                // Update column width (take maximum)
                if (cell_width > col_widths[col_idx]) {
                    col_widths[col_idx] = cell_width;
                }

                // Update row height (take maximum)
                if (cell_height > max_row_height) {
                    max_row_height = cell_height;
                }
            }

            row_heights[current_row] = max_row_height;
            current_row++;
        }
    }

    // Calculate total table dimensions
    float total_width = 0;
    for (uint32_t i = 0; i < num_cols; i++) {
        total_width += col_widths[i];
    }

    float total_height = 0;
    for (uint32_t i = 0; i < num_rows; i++) {
        total_height += row_heights[i];
    }

    // Add border spacing if borders are enabled
    if (state->style.show_borders) {
        total_width += (num_cols + 1) * state->style.border_width;
        total_height += (num_rows + 1) * state->style.border_width;
    }

    if (layout_is_debug_enabled("Table")) {
        fprintf(stderr, "[TABLE_LAYOUT] Total dimensions: %.1fx%.1f\n", total_width, total_height);
    }

    // Apply constraints and set table computed layout
    layout_apply_constraints(&total_width, &total_height, constraints);
    layout_set_final_with_parent(c, parent_x, parent_y, total_width, total_height);

    // Store calculated dimensions in table state
    state->calculated_widths = col_widths;
    state->calculated_heights = row_heights;
    state->row_count = num_rows;
    state->column_count = num_cols;

    // Now layout all child cells with calculated positions
    float current_y = parent_y;
    if (state->style.show_borders) {
        current_y += state->style.border_width;
    }

    current_row = 0;
    for (uint32_t section_idx = 0; section_idx < c->child_count; section_idx++) {
        IRComponent* section = c->children[section_idx];
        if (!section) continue;

        for (uint32_t row_idx = 0; row_idx < section->child_count; row_idx++) {
            IRComponent* row = section->children[row_idx];
            if (!row || row->type != IR_COMPONENT_TABLE_ROW) continue;

            float current_x = parent_x;
            if (state->style.show_borders) {
                current_x += state->style.border_width;
            }

            // Layout row
            if (!layout_ensure_state(row)) continue;
            layout_set_final_with_parent(row, parent_x, current_y, total_width, row_heights[current_row]);

            // Layout cells in this row
            for (uint32_t col_idx = 0; col_idx < row->child_count && col_idx < num_cols; col_idx++) {
                IRComponent* cell = row->children[col_idx];
                if (!cell) continue;

                if (!layout_ensure_state(cell)) continue;
                layout_set_final_with_parent(cell, current_x, current_y, col_widths[col_idx], row_heights[current_row]);

                current_x += col_widths[col_idx];
                if (state->style.show_borders) {
                    current_x += state->style.border_width;
                }
            }

            current_y += row_heights[current_row];
            if (state->style.show_borders) {
                current_y += state->style.border_width;
            }
            current_row++;
        }
    }

    // Layout table sections (TableHead, TableBody, TableFoot)
    for (uint32_t section_idx = 0; section_idx < c->child_count; section_idx++) {
        IRComponent* section = c->children[section_idx];
        if (!section) continue;

        if (!layout_ensure_state(section)) continue;
        layout_set_final_with_parent(section, parent_x, parent_y, total_width, total_height);
    }

    if (layout_is_debug_enabled("Table")) {
        fprintf(stderr, "[TABLE_LAYOUT] Completed layout for table id=%u: %.1fx%.1f at (%.1f,%.1f)\n",
                c->id, total_width, total_height, parent_x, parent_y);
    }
}

const IRLayoutTrait IR_TABLE_LAYOUT_TRAIT = {
    .layout_component = layout_table_single_pass,
    .name = "Table"
};

void ir_table_component_init(void) {
    ir_layout_register_trait(IR_COMPONENT_TABLE, &IR_TABLE_LAYOUT_TRAIT);

    if (getenv("KRYON_DEBUG_REGISTRY")) {
        fprintf(stderr, "[Registry] Table component initialized\n");
    }
}

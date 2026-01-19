// IR Table Module
// Table component implementation extracted from ir_builder.c

#define _GNU_SOURCE
#include "ir_table.h"
#include "../include/ir_builder.h"
#include "../utils/ir_color_utils.h"
#include "../include/ir_component_factory.h"
#include "../utils/ir_memory.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum number of table columns (defined in ir_core.h, but need locally)
#ifndef IR_MAX_TABLE_COLUMNS
#define IR_MAX_TABLE_COLUMNS 64
#endif

// ============================================================================
// Table State Management
// ============================================================================

IRTableState* ir_table_create_state(void) {
    IRTableState* state = (IRTableState*)calloc(1, sizeof(IRTableState));
    if (!state) return NULL;

    // Initialize with default styling
    state->style.border_color = ir_color_rgba(200, 200, 200, 255);  // Light gray
    state->style.header_background = ir_color_rgba(240, 240, 240, 255);  // Very light gray
    state->style.even_row_background = ir_color_rgba(255, 255, 255, 255);  // White
    state->style.odd_row_background = ir_color_rgba(249, 249, 249, 255);  // Off-white
    state->style.border_width = 1.0f;
    state->style.cell_padding = 8.0f;
    state->style.show_borders = true;
    state->style.striped_rows = false;
    state->style.header_sticky = false;
    state->style.collapse_borders = true;

    state->columns = NULL;
    state->column_count = 0;
    state->calculated_widths = NULL;
    state->calculated_heights = NULL;
    state->row_count = 0;
    state->header_row_count = 0;
    state->footer_row_count = 0;
    state->span_map = NULL;
    state->span_map_rows = 0;
    state->span_map_cols = 0;
    state->layout_valid = false;

    return state;
}

void ir_table_destroy_state(IRTableState* state) {
    if (!state) return;

    if (state->columns) free(state->columns);
    if (state->calculated_widths) free(state->calculated_widths);
    if (state->calculated_heights) free(state->calculated_heights);
    if (state->span_map) free(state->span_map);

    free(state);
}

IRTableState* ir_get_table_state(IRComponent* component) {
    if (!component || component->type != IR_COMPONENT_TABLE || !component->custom_data) {
        return NULL;
    }
    return (IRTableState*)component->custom_data;
}

// ============================================================================
// Table Column Definitions
// ============================================================================

void ir_table_add_column(IRTableState* state, IRTableColumnDef column) {
    if (!state) return;
    if (state->column_count >= IR_MAX_TABLE_COLUMNS) {
        fprintf(stderr, "[ir_table] Table column limit (%d) exceeded\n", IR_MAX_TABLE_COLUMNS);
        return;
    }

    // Reallocate columns array
    IRTableColumnDef* new_columns = (IRTableColumnDef*)realloc(
        state->columns,
        (state->column_count + 1) * sizeof(IRTableColumnDef)
    );
    if (!new_columns) return;

    state->columns = new_columns;
    state->columns[state->column_count] = column;
    state->column_count++;
    state->layout_valid = false;
}

IRTableColumnDef ir_table_column_auto(void) {
    IRTableColumnDef col = {0};
    col.auto_size = true;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_AUTO;
    col.min_width.type = IR_DIMENSION_PX;
    col.min_width.value = 50.0f;  // Reasonable minimum
    col.max_width.type = IR_DIMENSION_AUTO;
    return col;
}

IRTableColumnDef ir_table_column_px(float width) {
    IRTableColumnDef col = {0};
    col.auto_size = false;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_PX;
    col.width.value = width;
    col.min_width.type = IR_DIMENSION_PX;
    col.min_width.value = width;
    col.max_width.type = IR_DIMENSION_PX;
    col.max_width.value = width;
    return col;
}

IRTableColumnDef ir_table_column_percent(float percent) {
    IRTableColumnDef col = {0};
    col.auto_size = false;
    col.alignment = IR_ALIGNMENT_START;
    col.width.type = IR_DIMENSION_PERCENT;
    col.width.value = percent;
    col.min_width.type = IR_DIMENSION_AUTO;
    col.max_width.type = IR_DIMENSION_AUTO;
    return col;
}

IRTableColumnDef ir_table_column_with_alignment(IRTableColumnDef col, IRAlignment alignment) {
    col.alignment = alignment;
    return col;
}

// ============================================================================
// Table Styling
// ============================================================================

void ir_table_set_border_color(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!state) return;
    state->style.border_color = ir_color_rgba(r, g, b, a);
}

void ir_table_set_header_background(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
    if (!state) return;
    state->style.header_background = ir_color_rgba(r, g, b, a);
}

void ir_table_set_row_backgrounds(IRTableState* state,
                                   uint8_t even_r, uint8_t even_g, uint8_t even_b, uint8_t even_a,
                                   uint8_t odd_r, uint8_t odd_g, uint8_t odd_b, uint8_t odd_a) {
    if (!state) return;
    state->style.even_row_background = ir_color_rgba(even_r, even_g, even_b, even_a);
    state->style.odd_row_background = ir_color_rgba(odd_r, odd_g, odd_b, odd_a);
}

void ir_table_set_border_width(IRTableState* state, float width) {
    if (!state) return;
    state->style.border_width = width;
}

void ir_table_set_cell_padding(IRTableState* state, float padding) {
    if (!state) return;
    state->style.cell_padding = padding;
}

void ir_table_set_show_borders(IRTableState* state, bool show) {
    if (!state) return;
    state->style.show_borders = show;
}

void ir_table_set_striped(IRTableState* state, bool striped) {
    if (!state) return;
    state->style.striped_rows = striped;
}

// ============================================================================
// Table Cell Data
// ============================================================================

IRTableCellData* ir_table_cell_data_create(uint16_t colspan, uint16_t rowspan, IRAlignment alignment) {
    IRTableCellData* data = (IRTableCellData*)calloc(1, sizeof(IRTableCellData));
    if (!data) return NULL;

    data->colspan = colspan > 0 ? colspan : 1;
    data->rowspan = rowspan > 0 ? rowspan : 1;
    data->alignment = (uint8_t)alignment;
    data->vertical_alignment = (uint8_t)IR_ALIGNMENT_CENTER;
    data->is_spanned = false;
    data->spanned_by_id = 0;

    return data;
}

IRTableCellData* ir_get_table_cell_data(IRComponent* component) {
    if (!component) return NULL;
    if (component->type != IR_COMPONENT_TABLE_CELL &&
        component->type != IR_COMPONENT_TABLE_HEADER_CELL) {
        return NULL;
    }
    if (!component->custom_data) return NULL;
    return (IRTableCellData*)component->custom_data;
}

void ir_table_cell_set_colspan(IRComponent* cell, uint16_t colspan) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->colspan = colspan > 0 ? colspan : 1;
    }
}

void ir_table_cell_set_rowspan(IRComponent* cell, uint16_t rowspan) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->rowspan = rowspan > 0 ? rowspan : 1;
    }
}

void ir_table_cell_set_alignment(IRComponent* cell, IRAlignment alignment) {
    IRTableCellData* data = ir_get_table_cell_data(cell);
    if (data) {
        data->alignment = (uint8_t)alignment;
    }
}

// ============================================================================
// Table Component Creation
// ============================================================================

IRComponent* ir_table(void) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE);
    if (!component) return NULL;

    // Create and attach table state
    IRTableState* state = ir_table_create_state();
    if (state) {
        component->custom_data = (char*)state;
    }

    return component;
}

IRComponent* ir_table_head(void) {
    return ir_create_component(IR_COMPONENT_TABLE_HEAD);
}

IRComponent* ir_table_body(void) {
    return ir_create_component(IR_COMPONENT_TABLE_BODY);
}

IRComponent* ir_table_foot(void) {
    return ir_create_component(IR_COMPONENT_TABLE_FOOT);
}

IRComponent* ir_table_row(void) {
    return ir_create_component(IR_COMPONENT_TABLE_ROW);
}

IRComponent* ir_table_cell(uint16_t colspan, uint16_t rowspan) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE_CELL);
    if (!component) return NULL;

    // Create and attach cell data
    IRTableCellData* data = ir_table_cell_data_create(colspan, rowspan, IR_ALIGNMENT_START);
    if (data) {
        component->custom_data = (char*)data;
    }

    return component;
}

IRComponent* ir_table_header_cell(uint16_t colspan, uint16_t rowspan) {
    IRComponent* component = ir_create_component(IR_COMPONENT_TABLE_HEADER_CELL);
    if (!component) return NULL;

    // Create and attach cell data with center alignment for headers
    IRTableCellData* data = ir_table_cell_data_create(colspan, rowspan, IR_ALIGNMENT_CENTER);
    if (data) {
        component->custom_data = (char*)data;
    }

    return component;
}

// ============================================================================
// Table Internal Functions
// ============================================================================

// Build span map for efficient layout
static void ir_table_build_span_map(IRTableState* state, IRComponent* table) {
    if (!state || !table) return;

    // Count rows and determine column count
    uint32_t total_rows = 0;
    uint32_t max_cols = state->column_count;

    // Traverse sections to count rows
    for (uint32_t i = 0; i < table->child_count; i++) {
        IRComponent* section = table->children[i];
        if (!section) continue;

        if (section->type == IR_COMPONENT_TABLE_HEAD) {
            state->head_section = section;
            state->header_row_count = section->child_count;
        } else if (section->type == IR_COMPONENT_TABLE_BODY) {
            state->body_section = section;
        } else if (section->type == IR_COMPONENT_TABLE_FOOT) {
            state->foot_section = section;
            state->footer_row_count = section->child_count;
        }

        total_rows += section->child_count;

        // Count columns from first row if not explicitly set
        if (max_cols == 0 && section->child_count > 0) {
            IRComponent* first_row = section->children[0];
            if (first_row) {
                max_cols = first_row->child_count;
            }
        }
    }

    state->row_count = total_rows;
    if (max_cols == 0) max_cols = 1;

    // Allocate span map
    if (state->span_map) free(state->span_map);
    state->span_map = (IRTableCellData*)calloc(total_rows * max_cols, sizeof(IRTableCellData));
    state->span_map_rows = total_rows;
    state->span_map_cols = max_cols;

    // Initialize all cells with default span (1x1)
    for (uint32_t i = 0; i < total_rows * max_cols; i++) {
        state->span_map[i].colspan = 1;
        state->span_map[i].rowspan = 1;
        state->span_map[i].is_spanned = false;
    }

    // Populate span map from actual cells
    uint32_t row_offset = 0;
    for (uint32_t s = 0; s < table->child_count; s++) {
        IRComponent* section = table->children[s];
        if (!section) continue;

        for (uint32_t r = 0; r < section->child_count; r++) {
            IRComponent* row = section->children[r];
            if (!row || row->type != IR_COMPONENT_TABLE_ROW) continue;

            uint32_t col = 0;
            for (uint32_t c = 0; c < row->child_count && col < max_cols; c++) {
                IRComponent* cell = row->children[c];
                if (!cell) continue;

                // Skip spanned positions
                while (col < max_cols && state->span_map[(row_offset + r) * max_cols + col].is_spanned) {
                    col++;
                }
                if (col >= max_cols) break;

                IRTableCellData* cell_data = ir_get_table_cell_data(cell);
                uint16_t colspan = cell_data ? cell_data->colspan : 1;
                uint16_t rowspan = cell_data ? cell_data->rowspan : 1;

                // Mark this cell's position
                IRTableCellData* map_entry = &state->span_map[(row_offset + r) * max_cols + col];
                map_entry->colspan = colspan;
                map_entry->rowspan = rowspan;
                map_entry->alignment = cell_data ? cell_data->alignment : (uint8_t)IR_ALIGNMENT_START;

                // Mark spanned positions
                for (uint16_t rs = 0; rs < rowspan && (row_offset + r + rs) < total_rows; rs++) {
                    for (uint16_t cs = 0; cs < colspan && (col + cs) < max_cols; cs++) {
                        if (rs == 0 && cs == 0) continue;  // Skip the cell itself
                        IRTableCellData* spanned = &state->span_map[(row_offset + r + rs) * max_cols + (col + cs)];
                        spanned->is_spanned = true;
                        spanned->spanned_by_id = cell->id;
                    }
                }

                col += colspan;
            }
        }
        row_offset += section->child_count;
    }
}

// ============================================================================
// Table Finalization
// ============================================================================

// Call after all children are added to build span map and calculate layout
void ir_table_finalize(IRComponent* table) {
    if (!table || table->type != IR_COMPONENT_TABLE) return;

    IRTableState* state = ir_get_table_state(table);
    if (!state) return;

    // Build span map
    ir_table_build_span_map(state, table);

    // Allocate calculated widths array if needed
    if (state->column_count > 0 || state->span_map_cols > 0) {
        uint32_t cols = state->column_count > 0 ? state->column_count : state->span_map_cols;
        if (state->calculated_widths) free(state->calculated_widths);
        state->calculated_widths = (float*)calloc(cols, sizeof(float));
    }

    // Allocate calculated heights array
    if (state->row_count > 0) {
        if (state->calculated_heights) free(state->calculated_heights);
        state->calculated_heights = (float*)calloc(state->row_count, sizeof(float));
    }

    state->layout_valid = false;
}

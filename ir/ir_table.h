#ifndef IR_TABLE_H
#define IR_TABLE_H

#include <stdint.h>
#include <stdbool.h>
#include "ir_core.h"

// ============================================================================
// Table State Management
// ============================================================================

IRTableState* ir_table_create_state(void);
void ir_table_destroy_state(IRTableState* state);
IRTableState* ir_get_table_state(IRComponent* component);

// ============================================================================
// Table Column Definitions
// ============================================================================

void ir_table_add_column(IRTableState* state, IRTableColumnDef column);
IRTableColumnDef ir_table_column_auto(void);
IRTableColumnDef ir_table_column_px(float width);
IRTableColumnDef ir_table_column_percent(float percent);
IRTableColumnDef ir_table_column_with_alignment(IRTableColumnDef col, IRAlignment alignment);

// ============================================================================
// Table Styling
// ============================================================================

void ir_table_set_border_color(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_table_set_header_background(IRTableState* state, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
void ir_table_set_row_backgrounds(IRTableState* state,
                                   uint8_t even_r, uint8_t even_g, uint8_t even_b, uint8_t even_a,
                                   uint8_t odd_r, uint8_t odd_g, uint8_t odd_b, uint8_t odd_a);
void ir_table_set_border_width(IRTableState* state, float width);
void ir_table_set_cell_padding(IRTableState* state, float padding);
void ir_table_set_show_borders(IRTableState* state, bool show);
void ir_table_set_striped(IRTableState* state, bool striped);

// ============================================================================
// Table Cell Data
// ============================================================================

IRTableCellData* ir_table_cell_data_create(uint16_t colspan, uint16_t rowspan, IRAlignment alignment);
IRTableCellData* ir_get_table_cell_data(IRComponent* component);
void ir_table_cell_set_colspan(IRComponent* cell, uint16_t colspan);
void ir_table_cell_set_rowspan(IRComponent* cell, uint16_t rowspan);
void ir_table_cell_set_alignment(IRComponent* cell, IRAlignment alignment);

// ============================================================================
// Table Component Creation
// ============================================================================

IRComponent* ir_table(void);
IRComponent* ir_table_head(void);
IRComponent* ir_table_body(void);
IRComponent* ir_table_foot(void);
IRComponent* ir_table_row(void);
IRComponent* ir_table_cell(uint16_t colspan, uint16_t rowspan);
IRComponent* ir_table_header_cell(uint16_t colspan, uint16_t rowspan);

// ============================================================================
// Table Finalization
// ============================================================================

// Call after all children are added to build span map and calculate layout
void ir_table_finalize(IRComponent* table);

#endif // IR_TABLE_H

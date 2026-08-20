#include "terminal_pane.h"

#include <string.h>

static int
reflow_cell_is_blank(const TerminalPaneReflowSpec *spec, const void *cell)
{
    if(spec == NULL || cell == NULL)
        return 1;
    if(spec->is_blank != NULL)
        return spec->is_blank(cell, spec->userdata);
    if(spec->blank_cell != NULL)
        return memcmp(cell, spec->blank_cell, spec->cell_size) == 0;
    return 0;
}

static void
reflow_copy_cell(const TerminalPaneReflowSpec *spec, int row, int col,
                 const void *cell)
{
    unsigned char *output;

    if(spec == NULL || cell == NULL || spec->output_cells == NULL ||
       row < 0 || row >= spec->output_rows || col < 0 ||
       col >= spec->output_cols)
        return;
    output = spec->output_cells;
    memcpy(output + ((size_t)row * (size_t)spec->output_cols + (size_t)col) *
                        spec->cell_size,
           cell, spec->cell_size);
}

static const void *
reflow_input_cell(const TerminalPaneReflowSpec *spec, int row, int col)
{
    const unsigned char *input;

    if(spec == NULL || spec->input_cells == NULL || row < 0 ||
       row >= spec->input_rows || col < 0 || col >= spec->input_cols)
        return NULL;
    input = spec->input_cells;
    return input + ((size_t)row * (size_t)spec->input_cols + (size_t)col) *
                       spec->cell_size;
}

static void
reflow_fill_output(const TerminalPaneReflowSpec *spec)
{
    int row;
    int col;

    if(spec == NULL)
        return;
    if(spec->output_wrapped != NULL && spec->output_rows > 0)
        memset(spec->output_wrapped, 0, (size_t)spec->output_rows);
    if(spec->output_cells == NULL || spec->blank_cell == NULL)
        return;
    for(row = 0; row < spec->output_rows; row++) {
        for(col = 0; col < spec->output_cols; col++)
            reflow_copy_cell(spec, row, col, spec->blank_cell);
    }
}

static int
reflow_row_text_end(const TerminalPaneReflowSpec *spec, int row, int keep_full)
{
    int end;

    if(spec == NULL || row < 0 || row >= spec->input_rows ||
       spec->input_cols <= 0)
        return 0;
    if(keep_full)
        return spec->input_cols;
    end = spec->input_cols;
    while(end > 0 && reflow_cell_is_blank(spec,
                                          reflow_input_cell(spec, row, end - 1)))
        end--;
    return end;
}

static void
reflow_append_cell(const TerminalPaneReflowSpec *spec, int *out_row,
                   int *out_col, const void *cell)
{
    if(spec == NULL || out_row == NULL || out_col == NULL ||
       spec->output_cols <= 0)
        return;
    if(*out_col >= spec->output_cols) {
        if(spec->output_wrapped != NULL && *out_row >= 0 &&
           *out_row < spec->output_rows)
            spec->output_wrapped[*out_row] = 1;
        (*out_row)++;
        *out_col = 0;
    }
    reflow_copy_cell(spec, *out_row, *out_col, cell);
    (*out_col)++;
}

static void
reflow_finish_line(int *out_row, int *out_col)
{
    if(out_row == NULL || out_col == NULL)
        return;
    (*out_row)++;
    *out_col = 0;
}

int
TerminalPaneReflowRows(const TerminalPaneReflowSpec *spec)
{
    int input_rows;
    int out_row = 0;
    int out_col = 0;
    int cursor_row = -1;
    int cursor_col = -1;
    int row;

    if(spec == NULL || spec->input_cells == NULL ||
       spec->output_cells == NULL || spec->input_cols <= 0 ||
       spec->input_rows <= 0 || spec->output_cols <= 0 ||
       spec->output_rows <= 0 || spec->cell_size == 0)
        return 0;

    reflow_fill_output(spec);

    input_rows = spec->input_rows;
    if(spec->trim_blank_rows_after_cursor) {
        while(input_rows > spec->cursor_input_row + 1) {
            int soft =
                spec->input_wrapped != NULL && spec->input_wrapped[input_rows - 1];

            if(soft || reflow_row_text_end(spec, input_rows - 1, 0) > 0)
                break;
            input_rows--;
        }
    }

    for(row = 0; row < input_rows; row++) {
        int soft = spec->input_wrapped != NULL && spec->input_wrapped[row];
        int end = reflow_row_text_end(spec, row, soft);
        int col;

        for(col = 0; col < end; col++) {
            if(row == spec->cursor_input_row &&
               col == spec->cursor_input_col) {
                cursor_row = out_row;
                cursor_col = out_col;
            }
            reflow_append_cell(spec, &out_row, &out_col,
                               reflow_input_cell(spec, row, col));
        }
        if(row == spec->cursor_input_row && cursor_row < 0) {
            cursor_row = out_row;
            cursor_col = out_col +
                         (spec->cursor_input_col > end
                              ? spec->cursor_input_col - end
                              : 0);
            while(cursor_col >= spec->output_cols) {
                cursor_row++;
                cursor_col -= spec->output_cols;
            }
        }
        if(!soft)
            reflow_finish_line(&out_row, &out_col);
    }
    if(out_col > 0)
        reflow_finish_line(&out_row, &out_col);

    if(spec->cursor_output_row != NULL)
        *spec->cursor_output_row = cursor_row;
    if(spec->cursor_output_col != NULL)
        *spec->cursor_output_col = cursor_col;
    if(spec->output_row_count != NULL)
        *spec->output_row_count = out_row;
    return 1;
}

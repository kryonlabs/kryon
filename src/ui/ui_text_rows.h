#ifndef KRYON_TEXT_ROWS_INTERNAL_H
#define KRYON_TEXT_ROWS_INTERNAL_H

#include "runtime/text_rows.h"

typedef struct TextRowCursor {
    String source;
    TextLogicalLine line;
    int next, start, y, font, gap, width, headings, words, loaded;
} TextRowCursor;

typedef struct TextMeasuredRow {
    int start, end, line_end, font, y, height, last;
} TextMeasuredRow;

TextRowCursor ui_text_rows(const char *text, int font, int gap, int width,
                           int headings, int words);
int ui_text_row_next(TextRowCursor *cursor, TextMeasuredRow *row);
#endif

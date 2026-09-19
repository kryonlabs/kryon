#include "ui_internal.h"
#include "ui_text_rows.h"

TextRowCursor ui_text_rows(const char *text, int font, int gap, int width,
                           int headings, int words)
{
    if (!text) text = "";
    TextRowCursor cursor = {0};
    cursor.source = StringView(text, strlen(text));
    cursor.font = font;
    cursor.gap = gap;
    cursor.width = width;
    cursor.headings = headings;
    cursor.words = words;
    return cursor;
}

/* Copying a measured range is a font ABI adaptation: native font APIs accept
 * terminated strings. Reuse one buffer per row, including shaped prefixes. */
int ui_text_row_next(TextRowCursor *cursor, TextMeasuredRow *row)
{
    if (!cursor->loaded) {
        cursor->line = TextLogicalLineFor(cursor->source, cursor->next);
        if (!cursor->line.valid) return 0;
        cursor->start = cursor->line.start;
        cursor->loaded = 1;
    }
    TextLogicalLine line = cursor->line;
    int start = cursor->start;
    int font = TextRowFontFor(cursor->source, line.start, line.end,
                              cursor->font, cursor->headings);
    int length = line.end - start;
    char local[1024];
    char *buffer = length < (int)sizeof(local) ? local : malloc((size_t)length + 1);
    if (!buffer) return 0;
    const char *text = (const char *)cursor->source.data;
    memcpy(buffer, text + start, (size_t)length);
    buffer[length] = '\0';
    int measured = TextWidth(buffer, font);
    TextRowBreak state = {.end = start};
    if (cursor->width <= 0 || measured <= cursor->width || start == line.end) {
        state = TextRowAdvance(state, start, line.end, line.end, 0,
                               (float)measured, (float)cursor->width, cursor->words);
    } else {
        while (!state.done) {
            int next = ui_grapheme_next_boundary(text, line.end, state.end);
            int offset = next - start;
            char saved = buffer[offset];
            buffer[offset] = '\0';
            measured = TextWidth(buffer, font);
            buffer[offset] = saved;
            state = TextRowAdvance(state, start, next, line.end,
                text[state.end] == ' ' || text[state.end] == '\t',
                (float)measured, (float)cursor->width, cursor->words);
        }
    }
    if (buffer != local) free(buffer);
    row->start = start;
    row->end = state.end;
    row->line_end = line.end;
    row->font = font;
    row->y = cursor->y;
    row->height = TextLineHeight(font) + cursor->gap;
    cursor->y += row->height;
    cursor->start = TextRowNextStart(cursor->source, state.end, line.end, cursor->words);
    cursor->loaded = cursor->start < line.end;
    cursor->next = line.next;
    row->last = !cursor->loaded && line.next > (int)cursor->source.length;
    return 1;
}

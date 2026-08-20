#include "terminal_pane.h"

#include <ctype.h>
#include <string.h>

#define TERMINAL_PANE_SELECTION_COPY_BUFFER_SIZE 262144

static int
pane_clamp_int(int value, int low, int high)
{
    if(value < low)
        return low;
    if(value > high)
        return high;
    return value;
}

static int
pane_max_int(int a, int b)
{
    return a > b ? a : b;
}

static int
pane_word_char(int ch)
{
    return isalnum((unsigned char)ch) || ch == '_' || ch == '-' ||
           ch == '.' || ch == '/' || ch == ':' || ch == '~';
}

static void
pane_word_bounds(const char *line, int len, int col, int *start, int *end)
{
    int left;
    int right;

    if(start != NULL)
        *start = col;
    if(end != NULL)
        *end = col + 1;
    if(line == NULL || len <= 0)
        return;
    col = pane_clamp_int(col, 0, pane_max_int(0, len));
    if(col >= len)
        col = len - 1;
    if(!pane_word_char((unsigned char)line[col]))
        return;
    left = col;
    right = col + 1;
    while(left > 0 && pane_word_char((unsigned char)line[left - 1]))
        left--;
    while(right < len && pane_word_char((unsigned char)line[right]))
        right++;
    if(start != NULL)
        *start = left;
    if(end != NULL)
        *end = right;
}

static int
pane_selection_line_text(TerminalPaneSelectionLineFn line_text,
                         void *userdata, int row, char *out, int out_size)
{
    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(line_text == NULL)
        return 0;
    return line_text(userdata, row, out, out_size);
}

void
TerminalPaneSelectionClear(TerminalPaneSelection *selection)
{
    if(selection == NULL)
        return;
    memset(selection, 0, sizeof(*selection));
}

void
TerminalPaneSelectionSetRange(TerminalPaneSelection *selection, int mode,
                              int dragging, int start_row, int start_col,
                              int end_row, int end_col)
{
    if(selection == NULL)
        return;
    selection->active = 1;
    selection->dragging = dragging ? 1 : 0;
    selection->mode = mode;
    selection->start_row = start_row;
    selection->start_col = start_col;
    selection->end_row = end_row;
    selection->end_col = end_col;
    selection->anchor_row = start_row;
    selection->anchor_start_col = start_col;
    selection->anchor_end_col = end_col;
}

void
TerminalPaneSelectionBeginChar(TerminalPaneSelection *selection, int row,
                               int col)
{
    TerminalPaneSelectionSetRange(selection, TERMINAL_PANE_SELECTION_CHAR, 1,
                                  row, col, row, col + 1);
}

void
TerminalPaneSelectionSelectLine(TerminalPaneSelection *selection,
                                TerminalPaneSelectionLineFn line_text,
                                void *userdata, int row)
{
    char line[4096];
    int len;

    if(selection == NULL || row < 0)
        return;
    (void)pane_selection_line_text(line_text, userdata, row, line,
                                   (int)sizeof(line));
    len = (int)strlen(line);
    TerminalPaneSelectionSetRange(selection, TERMINAL_PANE_SELECTION_LINE, 1,
                                  row, 0, row, len);
}

void
TerminalPaneSelectionSelectWord(TerminalPaneSelection *selection,
                                TerminalPaneSelectionLineFn line_text,
                                void *userdata, int row, int col)
{
    char line[4096];
    int len;
    int start;
    int end;

    if(selection == NULL || row < 0)
        return;
    (void)pane_selection_line_text(line_text, userdata, row, line,
                                   (int)sizeof(line));
    len = (int)strlen(line);
    col = pane_clamp_int(col, 0, pane_max_int(0, len));
    if(len == 0) {
        TerminalPaneSelectionSelectLine(selection, line_text, userdata, row);
        selection->mode = TERMINAL_PANE_SELECTION_WORD;
        return;
    }
    pane_word_bounds(line, len, col, &start, &end);
    TerminalPaneSelectionSetRange(selection, TERMINAL_PANE_SELECTION_WORD, 1,
                                  row, start, row, end);
}

void
TerminalPaneSelectionSelectAll(TerminalPaneSelection *selection, int total_rows,
                               int cols)
{
    if(selection == NULL || total_rows <= 0)
        return;
    if(cols < 0)
        cols = 0;
    TerminalPaneSelectionSetRange(selection, TERMINAL_PANE_SELECTION_CHAR, 0,
                                  0, 0, total_rows - 1, cols);
}

void
TerminalPaneSelectionUpdateEnd(TerminalPaneSelection *selection,
                               TerminalPaneSelectionLineFn line_text,
                               void *userdata, int row, int col)
{
    char line[4096];
    int len;

    if(selection == NULL || row < 0)
        return;
    if(selection->mode == TERMINAL_PANE_SELECTION_LINE) {
        (void)pane_selection_line_text(line_text, userdata, row, line,
                                       (int)sizeof(line));
        len = (int)strlen(line);
        if(row < selection->start_row) {
            char start_line[4096];

            (void)pane_selection_line_text(line_text, userdata,
                                           selection->start_row, start_line,
                                           (int)sizeof(start_line));
            selection->start_col = (int)strlen(start_line);
            selection->end_col = 0;
        } else {
            selection->start_col = 0;
            selection->end_col = len;
        }
        selection->end_row = row;
        return;
    }
    if(selection->mode == TERMINAL_PANE_SELECTION_WORD) {
        int start;
        int end;

        (void)pane_selection_line_text(line_text, userdata, row, line,
                                       (int)sizeof(line));
        len = (int)strlen(line);
        col = pane_clamp_int(col, 0, pane_max_int(0, len));
        pane_word_bounds(line, len, col, &start, &end);
        if(row < selection->anchor_row ||
           (row == selection->anchor_row &&
            start < selection->anchor_start_col)) {
            selection->start_row = selection->anchor_row;
            selection->start_col = selection->anchor_end_col;
            selection->end_row = row;
            selection->end_col = start;
        } else {
            selection->start_row = selection->anchor_row;
            selection->start_col = selection->anchor_start_col;
            selection->end_row = row;
            selection->end_col = end;
        }
        return;
    }
    selection->end_row = row;
    selection->end_col = col + 1;
}

int
TerminalPaneSelectionContains(const TerminalPaneSelection *selection, int row,
                              int col)
{
    int sr;
    int er;
    int sc;
    int ec;

    if(selection == NULL || !selection->active)
        return 0;
    sr = selection->start_row;
    er = selection->end_row;
    sc = selection->start_col;
    ec = selection->end_col;
    if(sr > er || (sr == er && sc > ec)) {
        int tmp;

        tmp = sr;
        sr = er;
        er = tmp;
        tmp = sc;
        sc = ec;
        ec = tmp;
    }
    if(row < sr || row > er)
        return 0;
    if(row == sr && col < sc)
        return 0;
    if(row == er && col >= ec)
        return 0;
    return 1;
}

int
TerminalPaneSelectionCollectText(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata, char *buffer,
    int buffer_size)
{
    int sr;
    int er;
    int sc;
    int ec;
    int row;
    int used = 0;

    if(buffer == NULL || buffer_size <= 0)
        return 0;
    buffer[0] = '\0';
    if(selection == NULL || !selection->active || line_text == NULL)
        return 0;
    sr = selection->start_row;
    er = selection->end_row;
    sc = selection->start_col;
    ec = selection->end_col;
    if(sr > er || (sr == er && sc > ec)) {
        int tmp;

        tmp = sr;
        sr = er;
        er = tmp;
        tmp = sc;
        sc = ec;
        ec = tmp;
    }
    for(row = sr; row <= er && used < buffer_size - 1; row++) {
        char line[4096];
        int len;
        int start = row == sr ? sc : 0;
        int end;
        int i;

        (void)pane_selection_line_text(line_text, userdata, row, line,
                                       (int)sizeof(line));
        len = (int)strlen(line);
        end = row == er ? ec : len;
        start = pane_clamp_int(start, 0, len);
        end = pane_clamp_int(end, start, len);
        for(i = start; i < end && used < buffer_size - 1; i++)
            buffer[used++] = line[i];
        if(row != er && used < buffer_size - 1 &&
           (line_wrapped == NULL || !line_wrapped(userdata, row)))
            buffer[used++] = '\n';
    }
    buffer[used] = '\0';
    return used > 0;
}

int
TerminalPaneSelectionUpdatePrimary(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata)
{
    static char buffer[TERMINAL_PANE_SELECTION_COPY_BUFFER_SIZE];

    if(TerminalPaneSelectionCollectText(
           selection, line_text, line_wrapped, userdata, buffer,
           (int)sizeof(buffer)))
        return SetUIPrimarySelectionFromText(buffer);
    return SetUIPrimarySelectionFromText("");
}

int
TerminalPaneSelectionCopyToClipboard(
    const TerminalPaneSelection *selection, TerminalPaneSelectionLineFn line_text,
    TerminalPaneSelectionWrappedFn line_wrapped, void *userdata,
    UIClipboardBuffer *clipboard)
{
    static char buffer[TERMINAL_PANE_SELECTION_COPY_BUFFER_SIZE];

    if(!TerminalPaneSelectionCollectText(selection, line_text, line_wrapped,
                                         userdata, buffer,
                                         (int)sizeof(buffer)))
        return 0;
    return CopyUISelectionTextToClipboard(clipboard, buffer);
}

int
TerminalPaneSelectionEdgeScrollDelta(float mouse_y, float viewport_y,
                                     float viewport_height, float edge_size)
{
    if(viewport_height <= 0.0f)
        return 0;
    if(edge_size < 0.0f)
        edge_size = 0.0f;
    if(edge_size > viewport_height / 2.0f)
        edge_size = viewport_height / 2.0f;
    if(mouse_y < viewport_y + edge_size)
        return 1;
    if(mouse_y > viewport_y + viewport_height - edge_size)
        return -1;
    return 0;
}

int
TerminalPaneSelectionFirstVisibleRow(int total_rows, int visible_rows,
                                     int scroll_offset)
{
    int first;

    if(total_rows <= 0 || visible_rows <= 0)
        return 0;
    if(scroll_offset < 0)
        scroll_offset = 0;
    first = total_rows - visible_rows - scroll_offset;
    return first > 0 ? first : 0;
}

int
TerminalPaneSelectionEdgeScrollRow(int first_visible_row, int visible_rows,
                                   int scroll_delta)
{
    if(visible_rows <= 0 || scroll_delta == 0)
        return -1;
    if(scroll_delta > 0)
        return first_visible_row;
    return first_visible_row + visible_rows - 1;
}

static int
pane_search_case_sensitive(const char *needle)
{
    const unsigned char *cursor = (const unsigned char *)needle;

    if(needle == NULL)
        return 0;
    while(*cursor != '\0') {
        if(isupper(*cursor))
            return 1;
        cursor++;
    }
    return 0;
}

static int
pane_search_chars_equal(unsigned char a, unsigned char b, int case_sensitive)
{
    if(case_sensitive)
        return a == b;
    return tolower(a) == tolower(b);
}

static const char *
pane_search_find_from(const char *line, const char *needle,
                      int case_sensitive)
{
    int needle_len;
    const char *cursor;

    if(line == NULL || needle == NULL)
        return NULL;
    needle_len = (int)strlen(needle);
    if(needle_len <= 0)
        return NULL;
    for(cursor = line; *cursor != '\0'; cursor++) {
        int i;

        for(i = 0; i < needle_len; i++) {
            if(cursor[i] == '\0' ||
               !pane_search_chars_equal((unsigned char)cursor[i],
                                        (unsigned char)needle[i],
                                        case_sensitive))
                break;
        }
        if(i == needle_len)
            return cursor;
    }
    return NULL;
}

static const char *
pane_search_last_before(const char *line, const char *needle, int limit,
                        int case_sensitive)
{
    const char *cursor;
    const char *last = NULL;
    int needle_len;

    if(line == NULL || needle == NULL)
        return NULL;
    needle_len = (int)strlen(needle);
    if(needle_len <= 0)
        return NULL;
    cursor = line;
    while((cursor = pane_search_find_from(cursor, needle,
                                          case_sensitive)) != NULL) {
        if((int)(cursor - line) + needle_len > limit)
            break;
        last = cursor;
        cursor++;
    }
    return last;
}

int
TerminalPaneSearchLines(TerminalPaneSelectionLineFn line_text, void *userdata,
                        int total_rows, const char *needle, int start_row,
                        int start_col, int direction, int wrap,
                        TerminalPaneSearchMatch *out)
{
    int needle_len;
    int case_sensitive;
    int pass;

    if(out != NULL) {
        out->row = -1;
        out->col = -1;
        out->length = 0;
    }
    if(line_text == NULL || total_rows <= 0 || needle == NULL ||
       needle[0] == '\0')
        return 0;
    needle_len = (int)strlen(needle);
    case_sensitive = pane_search_case_sensitive(needle);
    direction = direction >= 0 ? 1 : -1;
    if(start_row < 0)
        start_row = direction > 0 ? 0 : total_rows - 1;
    if(start_row >= total_rows)
        start_row = direction > 0 ? 0 : total_rows - 1;

    for(pass = 0; pass < (wrap ? 2 : 1); pass++) {
        int row;
        int end_row = direction > 0 ? total_rows : -1;

        for(row = start_row; row != end_row; row += direction) {
            char line[4096];
            const char *match = NULL;
            int line_len;
            int col_limit;

            (void)pane_selection_line_text(line_text, userdata, row, line,
                                           (int)sizeof(line));
            line_len = (int)strlen(line);
            if(direction > 0) {
                col_limit = row == start_row ? start_col : 0;
                col_limit = pane_clamp_int(col_limit, 0, line_len);
                match = pane_search_find_from(line + col_limit, needle,
                                              case_sensitive);
            } else {
                col_limit = row == start_row ? start_col : line_len;
                col_limit = pane_clamp_int(col_limit, 0, line_len);
                match = pane_search_last_before(line, needle, col_limit,
                                                case_sensitive);
            }
            if(match != NULL) {
                if(out != NULL) {
                    out->row = row;
                    out->col = (int)(match - line);
                    out->length = needle_len;
                }
                return 1;
            }
        }
        start_row = direction > 0 ? 0 : total_rows - 1;
        start_col = direction > 0 ? 0 : 4096;
    }
    return 0;
}

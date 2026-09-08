#include "ui_internal.h"

/* UTF-8 codec and text-buffer mutation helpers extracted from ui.c. These are
 * pure functions over caller-owned buffers (no UI state, no clipboard), so they
 * can be unit-tested without the GUI/raylib link. They remain ui-private:
 * declared in ui_internal.h and called from ui.c. */

int
ui_utf8_next_offset(const char *text, int offset)
{
    int codepoint_size = 0;
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset < 0)
        offset = 0;
    if(offset >= len)
        return len;

    GetCodepointNext(text + offset, &codepoint_size);
    if(codepoint_size <= 0)
        codepoint_size = 1;
    if(offset + codepoint_size > len)
        return len;
    return offset + codepoint_size;
}

int
ui_utf8_prev_offset(const char *text, int offset)
{
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    if(offset > len)
        offset = len;
    if(offset <= 0)
        return 0;

    offset--;
    while(offset > 0 && (((unsigned char)text[offset] & 0xC0) == 0x80))
        offset--;
    return offset;
}

static int
ui_utf8_clamp_offset(const char *text, int offset)
{
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    offset = ui_clampi(offset, 0, len);
    while(offset > 0 && offset < len &&
          (((unsigned char)text[offset] & 0xc0) == 0x80))
        offset--;
    return offset;
}

int
ui_text_composition_view(const char *text, int selection_start,
                         int selection_end, const char *preedit,
                         int preedit_cursor, int preedit_selection_length,
                         TextCompositionView *view)
{
    int text_len;
    int preedit_len;
    int start;
    int end;
    int cursor;
    int selected_end;
    size_t view_len;

    if(view == NULL)
        return 0;
    memset(view, 0, sizeof(*view));
    if(text == NULL)
        text = "";
    if(preedit == NULL || preedit[0] == '\0')
        return 0;

    text_len = (int)strlen(text);
    preedit_len = (int)strlen(preedit);
    start = ui_utf8_clamp_offset(text, selection_start);
    end = ui_utf8_clamp_offset(text, selection_end);
    if(start > end) {
        int swap = start;
        start = end;
        end = swap;
    }
    cursor = ui_utf8_clamp_offset(preedit, preedit_cursor);
    if(preedit_selection_length < 0)
        preedit_selection_length = 0;
    if(preedit_selection_length > preedit_len - cursor)
        preedit_selection_length = preedit_len - cursor;
    selected_end = ui_utf8_clamp_offset(
        preedit, cursor + preedit_selection_length);

    view_len = (size_t)text_len - (size_t)(end - start) +
               (size_t)preedit_len;
    view->text = malloc(view_len + 1);
    if(view->text == NULL)
        return 0;
    memcpy(view->text, text, (size_t)start);
    memcpy(view->text + start, preedit, (size_t)preedit_len);
    memcpy(view->text + start + preedit_len, text + end,
           (size_t)(text_len - end + 1));
    view->cursor = start + cursor;
    view->selection_start = start + cursor;
    view->selection_end = start + selected_end;
    view->composition_start = start;
    view->composition_end = start + preedit_len;
    return 1;
}

void
ui_text_composition_view_free(TextCompositionView *view)
{
    if(view == NULL)
        return;
    free(view->text);
    memset(view, 0, sizeof(*view));
}

static int
ui_text_codepoint_at(const char *text, int offset)
{
    int codepoint_size = 0;

    if(text == NULL || offset < 0 || text[offset] == '\0')
        return 0;
    return GetCodepointNext(text + offset, &codepoint_size);
}

static int
ui_text_is_blank(int codepoint)
{
    return codepoint == ' ' || codepoint == '\t' || codepoint == 0x3000;
}

static int
ui_text_is_separator(int codepoint)
{
    switch(codepoint) {
    case ',': case 0x3001:
    case '.': case 0x3002:
    case ';': case 0xff1b:
    case '(': case 0xff08:
    case ')': case 0xff09:
    case '{': case 0xff5b:
    case '}': case 0xff5d:
    case '[': case 0x300c:
    case ']': case 0x300d:
    case '|': case 0xff5c:
    case '!': case 0xff01:
    case '\\': case 0xffe5:
    case '/': case 0x30fb: case 0xff0f:
    case '\n': case '\r':
        return 1;
    default:
        return 0;
    }
}

static int
ui_text_is_word_boundary(const char *text, int offset)
{
    int previous_offset;
    int previous;
    int current;
    int previous_blank;
    int previous_separator;
    int current_blank;
    int current_separator;

    if(text == NULL || offset <= 0)
        return 0;
    previous_offset = ui_utf8_prev_offset(text, offset);
    previous = ui_text_codepoint_at(text, previous_offset);
    current = ui_text_codepoint_at(text, offset);
    previous_blank = ui_text_is_blank(previous);
    previous_separator = ui_text_is_separator(previous);
    current_blank = ui_text_is_blank(current);
    current_separator = ui_text_is_separator(current);
    return ((previous_blank || previous_separator) &&
            !(current_separator || current_blank)) ||
           (current_separator && !previous_separator);
}

int
ui_text_word_left(const char *text, int cursor)
{
    if(text == NULL)
        return 0;
    cursor = ui_clampi(cursor, 0, (int)strlen(text));
    cursor = ui_utf8_prev_offset(text, cursor);
    while(cursor > 0 && !ui_text_is_word_boundary(text, cursor))
        cursor = ui_utf8_prev_offset(text, cursor);
    return cursor;
}

int
ui_text_word_right(const char *text, int cursor)
{
    int len;

    if(text == NULL)
        return 0;
    len = (int)strlen(text);
    cursor = ui_clampi(cursor, 0, len);
    cursor = ui_utf8_next_offset(text, cursor);
    while(cursor < len && !ui_text_is_word_boundary(text, cursor))
        cursor = ui_utf8_next_offset(text, cursor);
    return cursor;
}

int
ui_utf8_codepoint_count(const char *text)
{
    int count = 0;

    if(text == NULL)
        return 0;
    for(int i = 0; text[i] != '\0';) {
        int next = ui_utf8_next_offset(text, i);
        if(next <= i)
            break;
        count++;
        i = next;
    }
    return count;
}

int
ui_utf8_encode(int codepoint, char out[5])
{
    if(out == NULL)
        return 0;
    if(codepoint < 0x80) {
        out[0] = (char)codepoint;
        out[1] = '\0';
        return 1;
    }
    if(codepoint < 0x800) {
        out[0] = (char)(0xC0 | (codepoint >> 6));
        out[1] = (char)(0x80 | (codepoint & 0x3F));
        out[2] = '\0';
        return 2;
    }
    if(codepoint < 0x10000) {
        out[0] = (char)(0xE0 | (codepoint >> 12));
        out[1] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[2] = (char)(0x80 | (codepoint & 0x3F));
        out[3] = '\0';
        return 3;
    }
    if(codepoint <= 0x10FFFF) {
        out[0] = (char)(0xF0 | (codepoint >> 18));
        out[1] = (char)(0x80 | ((codepoint >> 12) & 0x3F));
        out[2] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
        out[3] = (char)(0x80 | (codepoint & 0x3F));
        out[4] = '\0';
        return 4;
    }
    return 0;
}

int
ui_text_delete_range(char *text, size_t text_size, int *cursor, int start, int end)
{
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL)
        return 0;
    len = (int)strlen(text);
    start = ui_clampi(start, 0, len);
    end = ui_clampi(end, 0, len);
    if(end <= start)
        return 0;
    memmove(text + start, text + end, (size_t)(len - end + 1));
    *cursor = start;
    return 1;
}

int
ui_text_delete_key(char *text, size_t text_size, int *anchor, int *cursor,
                   int key, int modifier, int secure)
{
    int start;
    int end;

    if(text == NULL || anchor == NULL || cursor == NULL)
        return 0;
    start = *anchor < *cursor ? *anchor : *cursor;
    end = *anchor > *cursor ? *anchor : *cursor;
    if(start == end) {
        if(key == KEY_BACKSPACE) {
            if(modifier)
                start = secure ? 0 : ui_text_word_left(text, *cursor);
            else
                start = ui_utf8_prev_offset(text, *cursor);
        } else if(key == KEY_DELETE) {
            if(modifier)
                end = secure
                    ? (int)strlen(text) : ui_text_word_right(text, *cursor);
            else
                end = ui_utf8_next_offset(text, *cursor);
        } else {
            return 0;
        }
    }
    if(!ui_text_delete_range(text, text_size, cursor, start, end))
        return 0;
    *anchor = *cursor;
    return 1;
}

int
ui_text_insert_ascii(char *text, size_t text_size, int *cursor, char ch,
                     int max_codepoints)
{
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL || ch == '\0')
        return 0;
    if(max_codepoints > 0 && ui_utf8_codepoint_count(text) >= max_codepoints)
        return 0;
    len = (int)strlen(text);
    *cursor = ui_clampi(*cursor, 0, len);
    if((size_t)(len + 2) > text_size)
        return 0;
    memmove(text + *cursor + 1, text + *cursor,
            (size_t)(len - *cursor + 1));
    text[*cursor] = ch;
    (*cursor)++;
    return 1;
}

int
ui_text_insert_codepoint(char *text, size_t text_size, int *cursor, int codepoint,
                         int max_codepoints)
{
    char encoded[5];
    int encoded_len;
    int len;

    if(text == NULL || text_size == 0 || cursor == NULL || codepoint < 32)
        return 0;
    if(max_codepoints > 0 && ui_utf8_codepoint_count(text) >= max_codepoints)
        return 0;

    encoded_len = ui_utf8_encode(codepoint, encoded);
    if(encoded_len <= 0)
        return 0;
    len = (int)strlen(text);
    *cursor = ui_clampi(*cursor, 0, len);
    if((size_t)(len + encoded_len + 1) > text_size)
        return 0;

    memmove(text + *cursor + encoded_len, text + *cursor, (size_t)(len - *cursor + 1));
    memcpy(text + *cursor, encoded, (size_t)encoded_len);
    *cursor += encoded_len;
    return 1;
}

int
ui_text_insert_text(char *text, size_t text_size, int *cursor,
                    const char *input, int allow_newlines,
                    TextInputFilter filter, void *filter_user_data,
                    int max_codepoints)
{
    char *insert;
    int len;
    int out_len = 0;
    int inserted_codepoints = 0;
    int remaining_codepoints = 0;
    size_t remaining_bytes;

    if(text == NULL || text_size == 0 || cursor == NULL ||
       input == NULL || input[0] == '\0')
        return 0;

    len = (int)strlen(text);
    *cursor = ui_clampi(*cursor, 0, len);
    if((size_t)len + 1 >= text_size)
        return 0;

    remaining_bytes = text_size - (size_t)len - 1;
    if(max_codepoints > 0) {
        remaining_codepoints = max_codepoints - ui_utf8_codepoint_count(text);
        if(remaining_codepoints <= 0)
            return 0;
    }

    insert = malloc(remaining_bytes + 1);
    if(insert == NULL)
        return 0;

    for(int i = 0; input[i] != '\0' && remaining_bytes > 0;) {
        char encoded[5];
        int bytes = 0;
        int encoded_len;
        int cp;

        if(max_codepoints > 0 && inserted_codepoints >= remaining_codepoints)
            break;
        if(input[i] == '\r') {
            i++;
            continue;
        }
        if(input[i] == '\n') {
            if(!allow_newlines) {
                i++;
                continue;
            }
            if(remaining_bytes < 1)
                break;
            insert[out_len++] = '\n';
            remaining_bytes--;
            inserted_codepoints++;
            i++;
            continue;
        }

        cp = GetCodepointNext(&input[i], &bytes);
        if(bytes <= 0)
            break;
        i += bytes;
        if(cp < 32)
            continue;
        if(filter != NULL && !filter(cp, filter_user_data))
            continue;
        encoded_len = ui_utf8_encode(cp, encoded);
        if(encoded_len <= 0)
            continue;
        if((size_t)encoded_len > remaining_bytes)
            break;
        memcpy(insert + out_len, encoded, (size_t)encoded_len);
        out_len += encoded_len;
        remaining_bytes -= (size_t)encoded_len;
        inserted_codepoints++;
    }

    if(out_len <= 0) {
        free(insert);
        return 0;
    }

    memmove(text + *cursor + out_len, text + *cursor,
            (size_t)(len - *cursor + 1));
    memcpy(text + *cursor, insert, (size_t)out_len);
    *cursor += out_len;
    free(insert);
    return 1;
}

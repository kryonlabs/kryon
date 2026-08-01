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

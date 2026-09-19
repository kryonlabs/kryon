#include "ui_grapheme.h"

#include <string.h>

/* Compile the pinned portable dependency, as with the Monocypher adapter. */
#define UTF8PROC_STATIC
#include "../../vendor/utf8proc/utf8proc.c"

int
ui_utf8_valid(const char *text, int length)
{
    int offset = 0;
    if(text == NULL || length < 0)
        return 0;
    while(offset < length) {
        utf8proc_int32_t codepoint;
        utf8proc_ssize_t size = utf8proc_iterate(
            (const utf8proc_uint8_t *)text + offset, length - offset, &codepoint);
        if(size < 1)
            return 0;
        offset += (int)size;
    }
    return 1;
}

int
ui_grapheme_next_boundary(const char *text, int length, int offset)
{
    utf8proc_int32_t previous;
    utf8proc_int32_t state = 0;
    utf8proc_ssize_t size;

    if(offset >= length)
        return length;
    size = utf8proc_iterate((const utf8proc_uint8_t *)text + offset,
                           length - offset, &previous);
    if(size < 1)
        return offset + 1;
    offset += (int)size;
    while(offset < length) {
        utf8proc_int32_t current;

        size = utf8proc_iterate((const utf8proc_uint8_t *)text + offset,
                               length - offset, &current);
        if(size < 1 || utf8proc_grapheme_break_stateful(previous, current, &state))
            break;
        previous = current;
        offset += (int)size;
    }
    return offset;
}

int
ui_grapheme_floor_offset(const char *text, int offset)
{
    int length;
    int start;

    if(text == NULL || offset <= 0)
        return 0;
    length = (int)strlen(text);
    if(offset >= length)
        return length;
    if((unsigned char)text[offset - 1] < 128 &&
       (unsigned char)text[offset] < 128 &&
       !(text[offset - 1] == '\r' && text[offset] == '\n'))
        return offset;
    /* LF always ends a cluster, so earlier lines cannot affect this one. */
    start = offset;
    while(start > 0 && text[start - 1] != '\n')
        start--;
    while(start < offset) {
        int next = ui_grapheme_next_boundary(text, length, start);

        if(next > offset)
            break;
        start = next;
    }
    return start;
}

int
ui_grapheme_next_offset(const char *text, int offset)
{
    if(text == NULL)
        return 0;
    offset = ui_grapheme_floor_offset(text, offset);
    return ui_grapheme_next_boundary(text, (int)strlen(text), offset);
}

int
ui_grapheme_prev_offset(const char *text, int offset)
{
    int length;

    if(text == NULL || offset <= 0)
        return 0;
    length = (int)strlen(text);
    if(offset > length)
        offset = length;
    return ui_grapheme_floor_offset(text, offset - 1);
}

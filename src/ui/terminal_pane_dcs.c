#include "terminal_pane.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TERMINAL_PANE_DCS_DEFAULT_MAX_BYTES 4194304

static int
dcs_hex_value(int ch)
{
    if(ch >= '0' && ch <= '9')
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    if(ch >= 'A' && ch <= 'F')
        return 10 + ch - 'A';
    return -1;
}

static int
append_text(char *buffer, int buffer_size, int *used, const char *text)
{
    int len;

    if(buffer == NULL || used == NULL || text == NULL || buffer_size <= 0)
        return 0;
    len = (int)strlen(text);
    if(*used < 0 || *used + len >= buffer_size)
        return 0;
    memcpy(buffer + *used, text, (size_t)len);
    *used += len;
    buffer[*used] = '\0';
    return 1;
}

static int
append_hex_bytes(char *buffer, int buffer_size, int *used, const char *bytes)
{
    static const char hex[] = "0123456789abcdef";
    const unsigned char *cursor = (const unsigned char *)bytes;

    if(buffer == NULL || used == NULL || bytes == NULL || buffer_size <= 0)
        return 0;
    while(*cursor != '\0') {
        if(*used + 2 >= buffer_size)
            return 0;
        buffer[(*used)++] = hex[*cursor >> 4];
        buffer[(*used)++] = hex[*cursor & 15];
        cursor++;
    }
    buffer[*used] = '\0';
    return 1;
}

static int
decode_hex_name(char *out, int out_size, const char *text, int text_len)
{
    int used = 0;
    int i;

    if(out == NULL || out_size <= 0 || text == NULL || text_len <= 0 ||
       (text_len & 1) != 0)
        return 0;
    for(i = 0; i < text_len; i += 2) {
        int hi = dcs_hex_value((unsigned char)text[i]);
        int lo = dcs_hex_value((unsigned char)text[i + 1]);

        if(hi < 0 || lo < 0 || used >= out_size - 1)
            return 0;
        out[used++] = (char)((hi << 4) | lo);
    }
    out[used] = '\0';
    return used > 0;
}

static const char *
terminfo_value(const char *name)
{
    static const struct {
        const char *name;
        const char *value;
    } entries[] = {
        {"TN", "xterm-256color"},
        {"Co", "256"},
        {"RGB", "8/8/8"},
        {"kbs", "\x7f"},
        {"kcuu1", "\x1b[A"},
        {"kcud1", "\x1b[B"},
        {"kcuf1", "\x1b[C"},
        {"kcub1", "\x1b[D"},
        {"khome", "\x1b[H"},
        {"kend", "\x1b[F"},
        {"kich1", "\x1b[2~"},
        {"kdch1", "\x1b[3~"},
        {"kpp", "\x1b[5~"},
        {"knp", "\x1b[6~"},
        {"ka1", "\x1bOw"},
        {"ka3", "\x1bOy"},
        {"kb2", "\x1bOu"},
        {"kc1", "\x1bOq"},
        {"kc3", "\x1bOs"},
        {"kent", "\x1bOM"},
        {"kf1", "\x1bOP"},
        {"kf2", "\x1bOQ"},
        {"kf3", "\x1bOR"},
        {"kf4", "\x1bOS"},
        {"kf5", "\x1b[15~"},
        {"kf6", "\x1b[17~"},
        {"kf7", "\x1b[18~"},
        {"kf8", "\x1b[19~"},
        {"kf9", "\x1b[20~"},
        {"kf10", "\x1b[21~"},
        {"kf11", "\x1b[23~"},
        {"kf12", "\x1b[24~"},
        {"kf13", "\x1b[1;2P"},
        {"kf14", "\x1b[1;2Q"},
        {"kf15", "\x1b[1;2R"},
        {"kf16", "\x1b[1;2S"},
        {"kf17", "\x1b[15;2~"},
        {"kf18", "\x1b[17;2~"},
        {"kf19", "\x1b[18;2~"},
        {"kf20", "\x1b[19;2~"},
        {"kf21", "\x1b[20;2~"},
        {"kf22", "\x1b[21;2~"},
        {"kf23", "\x1b[23;2~"},
        {"kf24", "\x1b[24;2~"}
    };
    int i;

    if(name == NULL)
        return NULL;
    for(i = 0; i < (int)(sizeof(entries) / sizeof(entries[0])); i++) {
        if(strcmp(name, entries[i].name) == 0)
            return entries[i].value;
    }
    return NULL;
}

static int
format_failure(char *out, int out_size)
{
    int len;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    len = snprintf(out, (size_t)out_size, "\x1bP0+r\x1b\\");
    if(len <= 0 || len >= out_size) {
        out[0] = '\0';
        return 0;
    }
    return len;
}

static int
dcs_append_utf8(char *out, int out_size, int *used, unsigned int codepoint)
{
    if(out == NULL || used == NULL || out_size <= 0)
        return 0;
    if(codepoint < 0x80) {
        if(*used + 1 >= out_size)
            return 0;
        out[(*used)++] = (char)codepoint;
    } else if(codepoint < 0x800) {
        if(*used + 2 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xc0 | (codepoint >> 6));
        out[(*used)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if(codepoint < 0x10000) {
        if(*used + 3 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xe0 | (codepoint >> 12));
        out[(*used)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (codepoint & 0x3f));
    } else if(codepoint <= 0x10ffff) {
        if(*used + 4 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xf0 | (codepoint >> 18));
        out[(*used)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        out[(*used)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (codepoint & 0x3f));
    } else {
        return 0;
    }
    out[*used] = '\0';
    return 1;
}

void
InitTerminalPaneDCSBuffer(TerminalPaneDCSBuffer *buffer, int max_bytes)
{
    if(buffer == NULL)
        return;
    memset(buffer, 0, sizeof(*buffer));
    buffer->max_bytes =
        max_bytes > 0 ? max_bytes : TERMINAL_PANE_DCS_DEFAULT_MAX_BYTES;
}

void
ResetTerminalPaneDCSBuffer(TerminalPaneDCSBuffer *buffer)
{
    if(buffer == NULL)
        return;
    buffer->length = 0;
    buffer->ignored = 0;
    if(buffer->text != NULL)
        buffer->text[0] = '\0';
    if(buffer->max_bytes <= 0)
        buffer->max_bytes = TERMINAL_PANE_DCS_DEFAULT_MAX_BYTES;
}

void
FreeTerminalPaneDCSBuffer(TerminalPaneDCSBuffer *buffer)
{
    if(buffer == NULL)
        return;
    free(buffer->text);
    memset(buffer, 0, sizeof(*buffer));
}

int
AppendTerminalPaneDCSCodepoint(TerminalPaneDCSBuffer *buffer,
                               unsigned int codepoint)
{
    char bytes[8];
    int used = 0;
    int i;

    if(buffer == NULL || buffer->ignored)
        return 0;
    if(buffer->max_bytes <= 0)
        buffer->max_bytes = TERMINAL_PANE_DCS_DEFAULT_MAX_BYTES;
    if(!dcs_append_utf8(bytes, (int)sizeof(bytes), &used, codepoint)) {
        buffer->ignored = 1;
        return 0;
    }
    if(buffer->capacity <= buffer->length + used + 1) {
        int capacity = buffer->capacity > 0 ? buffer->capacity * 2 : 1024;
        char *text;

        while(capacity <= buffer->length + used + 1 &&
              capacity < buffer->max_bytes)
            capacity *= 2;
        if(capacity > buffer->max_bytes)
            capacity = buffer->max_bytes;
        if(capacity <= buffer->length + used + 1) {
            buffer->ignored = 1;
            return 0;
        }
        text = realloc(buffer->text, (size_t)capacity);
        if(text == NULL) {
            buffer->ignored = 1;
            return 0;
        }
        buffer->text = text;
        buffer->capacity = capacity;
    }
    for(i = 0; i < used; i++)
        buffer->text[buffer->length++] = bytes[i];
    buffer->text[buffer->length] = '\0';
    return used;
}

const char *
GetTerminalPaneDCSBufferText(TerminalPaneDCSBuffer *buffer)
{
    if(buffer == NULL || buffer->ignored || buffer->text == NULL)
        return NULL;
    buffer->text[buffer->length] = '\0';
    return buffer->text;
}

int
TerminalPaneDCSBufferIgnored(const TerminalPaneDCSBuffer *buffer)
{
    return buffer != NULL && buffer->ignored;
}

int
FormatTerminalPaneXTGETTCAPResponse(char *out, int out_size,
                                    const char *payload)
{
    const char *cursor;
    int used = 0;
    int first = 1;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(payload == NULL || strncmp(payload, "+q", 2) != 0)
        return 0;
    cursor = payload + 2;
    if(!append_text(out, out_size, &used, "\x1bP1+r"))
        return 0;
    while(*cursor != '\0') {
        const char *end = strchr(cursor, ';');
        char name[32];
        const char *value;
        int len = end != NULL ? (int)(end - cursor) : (int)strlen(cursor);

        if(!decode_hex_name(name, (int)sizeof(name), cursor, len))
            return format_failure(out, out_size);
        value = terminfo_value(name);
        if(value == NULL)
            return format_failure(out, out_size);
        if(!first && !append_text(out, out_size, &used, ";"))
            return 0;
        if(!append_hex_bytes(out, out_size, &used, name) ||
           !append_text(out, out_size, &used, "=") ||
           !append_hex_bytes(out, out_size, &used, value)) {
            out[0] = '\0';
            return 0;
        }
        first = 0;
        if(end == NULL)
            break;
        cursor = end + 1;
    }
    if(!append_text(out, out_size, &used, "\x1b\\"))
        return 0;
    return used;
}

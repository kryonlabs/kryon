#include "ui_tk.h"

#include <stdio.h>
#include <string.h>

/* Large enough that the editors source buffer (512 KiB) is the real ceiling,
 * not this. Matches the raylib SDL read buffer set via RAY_RAYLIB_CONFIG so
 * copy and paste caps stay symmetric. */
#define TK_CLIPBOARD_MAX (1024 * 1024)
#define CLIPBOARD_OSC52_ENCODED_SIZE 5464
#define CLIPBOARD_OSC52_RESPONSE_SIZE 5520

static char g_clipboard_text[TK_CLIPBOARD_MAX];
static char g_primary_selection_text[TK_CLIPBOARD_MAX];

int
SetClipboardTextValue(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    SetClipboardText(g_clipboard_text);
    return 1;
}

const char *
GetClipboardTextValue(void)
{
    const char *text = GetClipboardText();

    if(text != NULL && text[0] != '\0')
        snprintf(g_clipboard_text, sizeof(g_clipboard_text), "%s", text);
    return g_clipboard_text;
}

int
SetPrimarySelectionTextValue(const char *text)
{
    if(text == NULL)
        text = "";
    snprintf(g_primary_selection_text, sizeof(g_primary_selection_text), "%s",
             text);
    return 1;
}

const char *
GetPrimarySelectionTextValue(void)
{
    return g_primary_selection_text;
}

int
ClipboardSourceHasText(ClipboardSource source)
{
    const char *text;

    switch(source) {
    case CLIPBOARD_SOURCE_PRIMARY:
        text = GetPrimarySelectionTextValue();
        break;
    case CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD:
        text = GetPrimarySelectionTextValue();
        if(text != NULL && text[0] != '\0')
            return 1;
        text = GetClipboardTextValue();
        break;
    case CLIPBOARD_SOURCE_CLIPBOARD:
    default:
        text = GetClipboardTextValue();
        break;
    }
    return text != NULL && text[0] != '\0';
}

const char *
GetClipboardSourceText(const ClipboardBuffer *clipboard,
                         ClipboardSource source)
{
    const char *text;

    switch(source) {
    case CLIPBOARD_SOURCE_PRIMARY:
        return GetPrimarySelectionTextValue();
    case CLIPBOARD_SOURCE_PRIMARY_OR_CLIPBOARD:
        text = GetPrimarySelectionTextValue();
        if(text != NULL && text[0] != '\0')
            return text;
        (void)clipboard;
        return GetClipboardTextValue();
    case CLIPBOARD_SOURCE_CLIPBOARD:
    default:
        (void)clipboard;
        return GetClipboardTextValue();
    }
}

int
SetPrimarySelectionFromText(const char *text)
{
    if(text == NULL || text[0] == '\0')
        text = "";
    return SetPrimarySelectionTextValue(text);
}

int
CopySelectionTextToClipboard(ClipboardBuffer *clipboard, const char *text)
{
    int changed = 0;

    if(text == NULL || text[0] == '\0') {
        (void)SetPrimarySelectionTextValue("");
        return 0;
    }
    changed |= SetClipboardTextValue(text);
    changed |= SetClipboardBufferText(clipboard, text);
    changed |= SetPrimarySelectionTextValue(text);
    return changed;
}

int
ClipboardTargetIncludes(const char *target, char wanted)
{
    int i;

    if(target == NULL || target[0] == '\0')
        return wanted == 'c';
    for(i = 0; target[i] != '\0' && target[i] != ';' &&
              target[i] != '\a' && target[i] != 0x1b;
        i++) {
        if(target[i] == wanted)
            return 1;
    }
    return 0;
}

int
ClipboardTargetUsesPrimary(const char *target)
{
    return ClipboardTargetIncludes(target, 'p') &&
           !ClipboardTargetIncludes(target, 'c') &&
           !ClipboardTargetIncludes(target, 's');
}

const char *
GetClipboardTargetText(const ClipboardBuffer *clipboard,
                         const char *target)
{
    if(ClipboardTargetUsesPrimary(target))
        return GetPrimarySelectionTextValue();
    return GetClipboardBufferText(clipboard);
}

int
RequestClipboardTargetWrite(ClipboardBuffer *clipboard, const char *target,
                              const char *text)
{
    int changed = 0;
    int wrote = 0;

    if(ClipboardTargetIncludes(target, 'p')) {
        changed |= SetPrimarySelectionTextValue(text);
        wrote = 1;
    }
    if(ClipboardTargetIncludes(target, 'c') ||
       ClipboardTargetIncludes(target, 's') || !wrote)
        changed |= RequestClipboardBufferWrite(clipboard, text);
    return changed;
}

static int
ui_clipboard_base64_value(int ch)
{
    if(ch >= 'A' && ch <= 'Z')
        return ch - 'A';
    if(ch >= 'a' && ch <= 'z')
        return 26 + ch - 'a';
    if(ch >= '0' && ch <= '9')
        return 52 + ch - '0';
    if(ch == '+')
        return 62;
    if(ch == '/')
        return 63;
    return -1;
}

static int
ui_clipboard_decode_base64(char *out, int out_size, const char *text)
{
    unsigned int value = 0;
    int bits = 0;
    int used = 0;

    if(out == NULL || out_size <= 0 || text == NULL)
        return 0;
    out[0] = '\0';
    while(*text != '\0') {
        int ch = (unsigned char)*text++;
        int v;

        if(ch == '=')
            break;
        if(ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
            continue;
        v = ui_clipboard_base64_value(ch);
        if(v < 0)
            return 0;
        value = (value << 6) | v;
        bits += 6;
        if(bits >= 8) {
            bits -= 8;
            if(used >= out_size - 1)
                break;
            out[used++] = (char)((value >> bits) & 0xff);
        }
    }
    out[used] = '\0';
    return used;
}

static int
ui_clipboard_encode_base64(char *out, int out_size, const char *text, int size)
{
    static const char alphabet[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int used = 0;
    int i;

    if(out == NULL || out_size <= 0 || text == NULL || size < 0)
        return 0;
    out[0] = '\0';
    for(i = 0; i < size; i += 3) {
        unsigned int a = (unsigned char)text[i];
        unsigned int b = i + 1 < size ? (unsigned char)text[i + 1] : 0;
        unsigned int c = i + 2 < size ? (unsigned char)text[i + 2] : 0;

        if(used + 4 >= out_size)
            return 0;
        out[used++] = alphabet[(a >> 2) & 0x3f];
        out[used++] = alphabet[((a & 0x03) << 4) | ((b >> 4) & 0x0f)];
        out[used++] = i + 1 < size
                          ? alphabet[((b & 0x0f) << 2) | ((c >> 6) & 0x03)]
                          : '=';
        out[used++] = i + 2 < size ? alphabet[c & 0x3f] : '=';
    }
    out[used] = '\0';
    return used;
}

static int
ui_clipboard_copy_osc52_target(char *out, int out_size, const char *payload,
                               const char **text_payload)
{
    int used = 0;

    if(out == NULL || out_size <= 0 || text_payload == NULL)
        return 0;
    out[0] = '\0';
    *text_payload = NULL;
    if(payload == NULL)
        return 0;
    while(payload[used] != '\0' && payload[used] != ';' &&
          payload[used] != '\a' && payload[used] != 0x1b &&
          used < out_size - 1) {
        out[used] = payload[used];
        used++;
    }
    out[used] = '\0';
    if(payload[used] != ';')
        return 0;
    if(used == 0) {
        out[0] = 'c';
        out[1] = '\0';
    }
    *text_payload = payload + used + 1;
    return 1;
}

static int
ui_clipboard_send_osc52_response(ClipboardBuffer *clipboard,
                                 const char *target,
                                 ClipboardOSC52WriteFn write_response,
                                 void *userdata)
{
    char encoded[CLIPBOARD_OSC52_ENCODED_SIZE];
    char response[CLIPBOARD_OSC52_RESPONSE_SIZE];
    const char *text;
    int size;
    int encoded_size;

    if(clipboard == NULL || write_response == NULL)
        return 0;
    text = GetClipboardTargetText(clipboard, target);
    size = (int)strlen(text);
    encoded_size =
        ui_clipboard_encode_base64(encoded, (int)sizeof(encoded), text, size);
    if(size > 0 && encoded_size <= 0)
        return 0;
    snprintf(response, sizeof(response), "\x1b]52;%s;%s\a", target, encoded);
    return write_response(userdata, response);
}

int
HandleClipboardOSC52(ClipboardBuffer *clipboard, const char *payload,
                       ClipboardOSC52WriteFn write_response,
                       void *userdata)
{
    char target[32];
    const char *text_payload;

    if(clipboard == NULL ||
       !ui_clipboard_copy_osc52_target(target, (int)sizeof(target), payload,
                                       &text_payload))
        return 0;
    if(text_payload[0] == '?')
        return ui_clipboard_send_osc52_response(clipboard, target,
                                                write_response, userdata);
    if(text_payload[0] == '\0')
        return RequestClipboardTargetWrite(clipboard, target, "");
    {
        char decoded[CLIPBOARD_BUFFER_SIZE];

        if(ui_clipboard_decode_base64(decoded, (int)sizeof(decoded),
                                      text_payload) > 0)
            return RequestClipboardTargetWrite(clipboard, target, decoded);
    }
    return 0;
}

static int
ui_clipboard_utf8_payload_bytes(const char *text)
{
    unsigned char ch;
    int len;
    int i;

    if(text == NULL)
        return 0;
    ch = (unsigned char)text[0];
    if(ch >= 0xc2 && ch <= 0xdf)
        len = 2;
    else if(ch >= 0xe0 && ch <= 0xef)
        len = 3;
    else if(ch >= 0xf0 && ch <= 0xf4)
        len = 4;
    else
        return 0;
    for(i = 1; i < len; i++) {
        if(((unsigned char)text[i] & 0xc0) != 0x80)
            return 0;
    }
    return len;
}

static const char *
ui_clipboard_skip_paste_control_string(const char *cursor)
{
    unsigned char ch;
    const char *seq;
    int bel_terminated = 0;

    if(cursor == NULL || cursor[0] == '\0')
        return cursor;
    ch = (unsigned char)cursor[0];
    if(ch == 0x1b) {
        unsigned char next = (unsigned char)cursor[1];

        if(next == '[') {
            seq = cursor + 2;
            while(*seq != '\0' &&
                  !((unsigned char)*seq >= 0x40 &&
                    (unsigned char)*seq <= 0x7e))
                seq++;
            return *seq != '\0' ? seq + 1 : seq;
        }
        if(next == ']')
            bel_terminated = 1;
        else if(next != 'P' && next != 'X' && next != '^' && next != '_')
            return cursor + (cursor[1] != '\0' ? 2 : 1);
        seq = cursor + 2;
    } else if(ch == 0x9b) {
        seq = cursor + 1;
        while(*seq != '\0' &&
              !((unsigned char)*seq >= 0x40 &&
                (unsigned char)*seq <= 0x7e))
            seq++;
        return *seq != '\0' ? seq + 1 : seq;
    } else if(ch == 0x9d) {
        bel_terminated = 1;
        seq = cursor + 1;
    } else if(ch == 0x90 || ch == 0x98 || ch == 0x9e || ch == 0x9f) {
        seq = cursor + 1;
    } else {
        return NULL;
    }
    while(*seq != '\0') {
        unsigned char c = (unsigned char)*seq;

        if((bel_terminated && c == 0x07) || c == 0x9c)
            return seq + 1;
        if(c == 0x1b && seq[1] == '\\')
            return seq + 2;
        seq++;
    }
    return seq;
}

static int
ui_clipboard_write_paste_chunk(ClipboardPasteWriteFn write_text,
                               void *userdata, const char *text, int size)
{
    if(write_text == NULL || text == NULL || size <= 0)
        return 0;
    return write_text(userdata, text, size);
}

int
WriteClipboardPaste(const char *text, int bracketed,
                      ClipboardPasteWriteFn write_text, void *userdata)
{
    int written = 0;
    const char *cursor;
    const char *chunk;

    if(text == NULL || text[0] == '\0' || write_text == NULL)
        return 0;
    if(!bracketed)
        return ui_clipboard_write_paste_chunk(write_text, userdata, text,
                                              (int)strlen(text));
    written += ui_clipboard_write_paste_chunk(write_text, userdata,
                                              "\x1b[200~", 6);
    cursor = text;
    chunk = cursor;
    while(*cursor != '\0') {
        unsigned char ch = (unsigned char)*cursor;

        if(ch >= 0x80) {
            int utf8_len = ui_clipboard_utf8_payload_bytes(cursor);

            if(utf8_len > 0) {
                cursor += utf8_len;
                continue;
            }
        }
        if(ch == 0x1b || ch == 0x9b) {
            const char *next = ui_clipboard_skip_paste_control_string(cursor);

            if(cursor > chunk)
                written += ui_clipboard_write_paste_chunk(
                    write_text, userdata, chunk, (int)(cursor - chunk));
            cursor = next != NULL ? next : cursor + 1;
            chunk = cursor;
            continue;
        }
        if(ch == 0x90 || ch == 0x98 || ch == 0x9d || ch == 0x9e ||
           ch == 0x9f) {
            const char *next = ui_clipboard_skip_paste_control_string(cursor);

            if(cursor > chunk)
                written += ui_clipboard_write_paste_chunk(
                    write_text, userdata, chunk, (int)(cursor - chunk));
            cursor = next != NULL ? next : cursor + 1;
            chunk = cursor;
            continue;
        }
        if((ch < 0x20 && ch != '\t' && ch != '\r' && ch != '\n') ||
           ch == 0x7f || (ch >= 0x80 && ch < 0xa0)) {
            if(cursor > chunk)
                written += ui_clipboard_write_paste_chunk(
                    write_text, userdata, chunk, (int)(cursor - chunk));
            cursor++;
            chunk = cursor;
            continue;
        }
        cursor++;
    }
    if(cursor > chunk)
        written += ui_clipboard_write_paste_chunk(write_text, userdata, chunk,
                                                  (int)(cursor - chunk));
    written += ui_clipboard_write_paste_chunk(write_text, userdata,
                                              "\x1b[201~", 6);
    return written;
}

int
WriteClipboardTextPaste(ClipboardBuffer *clipboard, const char *text,
                          int bracketed, ClipboardPasteWriteFn write_text,
                          void *userdata)
{
    if(text == NULL || text[0] == '\0')
        return 0;
    (void)SetClipboardBufferText(clipboard, text);
    return WriteClipboardPaste(text, bracketed, write_text, userdata);
}

int
WriteClipboardSourcePaste(ClipboardBuffer *clipboard,
                            ClipboardSource source, int bracketed,
                            ClipboardPasteWriteFn write_text, void *userdata)
{
    const char *text;

    text = GetClipboardSourceText(clipboard, source);
    return WriteClipboardTextPaste(clipboard, text, bracketed, write_text,
                                     userdata);
}

static int
ui_clipboard_buffer_copy_text(char *dst, int dst_size, const char *text)
{
    int changed;

    if(dst == NULL || dst_size <= 0)
        return 0;
    if(text == NULL)
        text = "";
    changed = strcmp(dst, text) != 0;
    snprintf(dst, (size_t)dst_size, "%s", text);
    return changed;
}

void
InitClipboardBuffer(ClipboardBuffer *buffer, const char *text)
{
    if(buffer == NULL)
        return;
    buffer->text[0] = '\0';
    buffer->pending = 0;
    (void)ui_clipboard_buffer_copy_text(buffer->text,
                                        (int)sizeof(buffer->text), text);
}

int
SetClipboardBufferText(ClipboardBuffer *buffer, const char *text)
{
    int changed;

    if(buffer == NULL)
        return 0;
    changed = ui_clipboard_buffer_copy_text(buffer->text,
                                            (int)sizeof(buffer->text), text);
    buffer->pending = 0;
    return changed;
}

int
RequestClipboardBufferWrite(ClipboardBuffer *buffer, const char *text)
{
    int changed;
    int was_pending;

    if(buffer == NULL)
        return 0;
    was_pending = buffer->pending;
    changed = ui_clipboard_buffer_copy_text(buffer->text,
                                            (int)sizeof(buffer->text), text);
    buffer->pending = 1;
    return changed || !was_pending;
}

const char *
GetClipboardBufferText(const ClipboardBuffer *buffer)
{
    if(buffer == NULL)
        return "";
    return buffer->text;
}

int
ClipboardBufferHasPendingWrite(const ClipboardBuffer *buffer)
{
    return buffer != NULL && buffer->pending;
}

int
SyncClipboardBufferFromHost(ClipboardBuffer *buffer)
{
    const char *text;

    if(buffer == NULL || buffer->pending)
        return 0;
    text = GetClipboardTextValue();
    return ui_clipboard_buffer_copy_text(buffer->text,
                                         (int)sizeof(buffer->text), text);
}

int
FlushClipboardBufferToHost(ClipboardBuffer *buffer)
{
    if(buffer == NULL || !buffer->pending)
        return 0;
    SetClipboardTextValue(buffer->text);
    buffer->pending = 0;
    return 1;
}

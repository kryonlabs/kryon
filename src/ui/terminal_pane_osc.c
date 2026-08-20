#include "terminal_pane.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
osc_hex_value(int ch)
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
append_osc_safe_text(char *buffer, int buffer_size, int *used,
                     const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;

    if(buffer == NULL || buffer_size <= 0 || used == NULL)
        return 0;
    if(cursor == NULL)
        cursor = (const unsigned char *)"";
    while(*cursor != '\0') {
        if(*cursor == '\a' || *cursor == 0x1b ||
           (*cursor < 0x20 && *cursor != '\t')) {
            cursor++;
            continue;
        }
        if(*used + 1 >= buffer_size)
            return 0;
        buffer[(*used)++] = (char)*cursor++;
    }
    buffer[*used] = '\0';
    return 1;
}

static int
parse_hex_byte(const char *text, int *used)
{
    int hi;
    int lo;

    if(text == NULL || text[0] == '\0')
        return -1;
    hi = osc_hex_value((unsigned char)text[0]);
    if(hi < 0)
        return -1;
    lo = osc_hex_value((unsigned char)text[1]);
    if(lo < 0) {
        if(used != NULL)
            *used = 1;
        return (hi << 4) | hi;
    }
    if(used != NULL)
        *used = 2;
    return (hi << 4) | lo;
}

static int
parse_hash_color_component(const char *text, int digits)
{
    int hi;
    int lo;

    if(text == NULL || digits <= 0)
        return -1;
    hi = osc_hex_value((unsigned char)text[0]);
    if(hi < 0)
        return -1;
    if(digits == 1)
        return (hi << 4) | hi;
    lo = osc_hex_value((unsigned char)text[1]);
    if(lo < 0)
        return -1;
    return (hi << 4) | lo;
}

static int
parse_hash_color(const char *text)
{
    int len = 0;
    int digits;
    int r;
    int g;
    int b;

    if(text == NULL || text[0] != '#')
        return TERMINAL_PANE_COLOR_DEFAULT;
    text++;
    while(text[len] != '\0') {
        if(osc_hex_value((unsigned char)text[len]) < 0)
            return TERMINAL_PANE_COLOR_DEFAULT;
        len++;
    }
    if(len < 3 || len > 12 || (len % 3) != 0)
        return TERMINAL_PANE_COLOR_DEFAULT;
    digits = len / 3;
    r = parse_hash_color_component(text, digits);
    g = parse_hash_color_component(text + digits, digits);
    b = parse_hash_color_component(text + digits * 2, digits);
    if(r >= 0 && g >= 0 && b >= 0)
        return TERMINAL_PANE_COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
    return TERMINAL_PANE_COLOR_DEFAULT;
}

static int
copy_osc_token(const char **cursor, char *out, int out_size)
{
    const char *start;
    int used = 0;

    if(cursor == NULL || *cursor == NULL || **cursor == '\0' ||
       out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    start = *cursor;
    while(*start != '\0' && *start != ';') {
        if(used + 1 < out_size)
            out[used++] = *start;
        start++;
    }
    out[used] = '\0';
    *cursor = *start == ';' ? start + 1 : start;
    return 1;
}

static int
parse_decimal_span(const char *start, const char *end, int max_value,
                   int *out)
{
    int value = 0;

    if(start == NULL || end == NULL || start == end || out == NULL ||
       max_value < 0)
        return 0;
    while(start < end) {
        if(*start < '0' || *start > '9')
            return 0;
        value = value * 10 + (*start - '0');
        if(value > max_value)
            return 0;
        start++;
    }
    *out = value;
    return 1;
}

static int
parse_palette_index_token(const char *text, int *out)
{
    const char *end;

    if(text == NULL)
        return 0;
    end = text + strlen(text);
    return parse_decimal_span(text, end, 255, out);
}

static int
parse_rgbi_component(const char **cursor)
{
    char *end;
    double value;

    if(cursor == NULL || *cursor == NULL || **cursor == '\0')
        return -1;
    value = strtod(*cursor, &end);
    if(end == *cursor || value < 0.0 || value > 1.0)
        return -1;
    *cursor = end;
    return (int)(value * 255.0 + 0.5);
}

static int
parse_rgbi_color(const char *text)
{
    const char *cursor;
    int r;
    int g;
    int b;

    if(text == NULL || strncmp(text, "rgbi:", 5) != 0)
        return TERMINAL_PANE_COLOR_DEFAULT;
    cursor = text + 5;
    r = parse_rgbi_component(&cursor);
    if(r < 0 || *cursor != '/')
        return TERMINAL_PANE_COLOR_DEFAULT;
    cursor++;
    g = parse_rgbi_component(&cursor);
    if(g < 0 || *cursor != '/')
        return TERMINAL_PANE_COLOR_DEFAULT;
    cursor++;
    b = parse_rgbi_component(&cursor);
    if(b < 0 || *cursor != '\0')
        return TERMINAL_PANE_COLOR_DEFAULT;
    return TERMINAL_PANE_COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
}

int
ParseTerminalPaneOSCColor(const char *text)
{
    int r;
    int g;
    int b;
    int used;

    if(text == NULL)
        return TERMINAL_PANE_COLOR_DEFAULT;
    if(text[0] == '#')
        return parse_hash_color(text);
    if(strncmp(text, "rgbi:", 5) == 0)
        return parse_rgbi_color(text);
    if(strncmp(text, "rgb:", 4) == 0 || strncmp(text, "rgba:", 5) == 0) {
        const char *cursor = strchr(text, ':');

        if(cursor == NULL)
            return TERMINAL_PANE_COLOR_DEFAULT;
        cursor++;
        r = parse_hex_byte(cursor, &used);
        cursor = strchr(cursor, '/');
        if(cursor == NULL)
            return TERMINAL_PANE_COLOR_DEFAULT;
        cursor++;
        g = parse_hex_byte(cursor, &used);
        cursor = strchr(cursor, '/');
        if(cursor == NULL)
            return TERMINAL_PANE_COLOR_DEFAULT;
        cursor++;
        b = parse_hex_byte(cursor, &used);
        if(r >= 0 && g >= 0 && b >= 0)
            return TERMINAL_PANE_COLOR_TRUE_RGB | (r << 16) | (g << 8) | b;
    }
    return TERMINAL_PANE_COLOR_DEFAULT;
}

int
ParseTerminalPaneOSCCommand(const char *text, int *out_code,
                            const char **out_payload)
{
    const char *separator;
    const char *end;
    int code;

    if(out_code != NULL)
        *out_code = -1;
    if(out_payload != NULL)
        *out_payload = "";
    if(text == NULL)
        return 0;
    separator = strchr(text, ';');
    end = separator != NULL ? separator : text + strlen(text);
    if(!parse_decimal_span(text, end, 999999, &code))
        return 0;
    if(out_code != NULL)
        *out_code = code;
    if(out_payload != NULL)
        *out_payload = separator != NULL ? separator + 1 : "";
    return 1;
}

int
NextTerminalPaneOSCPaletteEntry(const char **cursor,
                                TerminalPaneOSCPaletteEntry *out)
{
    char index_text[16];
    char color_text[128];
    int index;
    int color;

    if(out != NULL) {
        out->index = -1;
        out->query = 0;
        out->color = TERMINAL_PANE_COLOR_DEFAULT;
        out->valid = 0;
    }
    if(cursor == NULL || *cursor == NULL || **cursor == '\0')
        return 0;
    if(!copy_osc_token(cursor, index_text, (int)sizeof(index_text)))
        return 0;
    if(!copy_osc_token(cursor, color_text, (int)sizeof(color_text)))
        return 1;
    if(out == NULL)
        return 1;
    if(!parse_palette_index_token(index_text, &index))
        return 1;
    if(strcmp(color_text, "?") == 0) {
        out->index = index;
        out->query = 1;
        out->valid = 1;
        return 1;
    }
    color = ParseTerminalPaneOSCColor(color_text);
    if(color == TERMINAL_PANE_COLOR_DEFAULT)
        return 1;
    out->index = index;
    out->color = color;
    out->valid = 1;
    return 1;
}

int
NextTerminalPaneOSCPaletteResetIndex(const char **cursor, int *out_index)
{
    char index_text[16];
    int index = -1;

    if(out_index != NULL)
        *out_index = -1;
    if(cursor == NULL || *cursor == NULL || **cursor == '\0')
        return 0;
    if(!copy_osc_token(cursor, index_text, (int)sizeof(index_text)))
        return 0;
    if(parse_palette_index_token(index_text, &index) && out_index != NULL)
        *out_index = index;
    return 1;
}

int
TerminalPaneDefaultPaletteColor(int index)
{
    TerminalPanePalette palette;
    Color color;

    if(index < 0)
        index = 0;
    if(index > 255)
        index = 255;
    palette = GetTerminalPaneDefaultPalette();
    color = palette.ansi[index];
    return TerminalPaneColorToRGB(color);
}

int
TerminalPaneOSCColorTargetForCode(int code)
{
    switch(code) {
    case 10:
        return TERMINAL_PANE_OSC_COLOR_FOREGROUND;
    case 11:
        return TERMINAL_PANE_OSC_COLOR_BACKGROUND;
    case 12:
        return TERMINAL_PANE_OSC_COLOR_CURSOR;
    case 13:
        return TERMINAL_PANE_OSC_COLOR_MOUSE_FOREGROUND;
    case 14:
        return TERMINAL_PANE_OSC_COLOR_MOUSE_BACKGROUND;
    case 17:
        return TERMINAL_PANE_OSC_COLOR_SELECTION_BACKGROUND;
    case 19:
        return TERMINAL_PANE_OSC_COLOR_SELECTION_FOREGROUND;
    default:
        break;
    }
    return TERMINAL_PANE_OSC_COLOR_INVALID;
}

int
TerminalPaneOSCColorTargetForResetCode(int code)
{
    if(code < 100)
        return TERMINAL_PANE_OSC_COLOR_INVALID;
    return TerminalPaneOSCColorTargetForCode(code - 100);
}

int
TerminalPaneOSCColorQueryValue(int target,
                               TerminalPaneOSCColorState state)
{
    switch(target) {
    case TERMINAL_PANE_OSC_COLOR_FOREGROUND:
        return state.foreground != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.foreground
                   : state.base_foreground;
    case TERMINAL_PANE_OSC_COLOR_BACKGROUND:
        return state.background != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.background
                   : state.base_background;
    case TERMINAL_PANE_OSC_COLOR_CURSOR:
        return state.cursor != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.cursor
                   : state.base_cursor;
    case TERMINAL_PANE_OSC_COLOR_MOUSE_FOREGROUND:
        return state.mouse_foreground != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.mouse_foreground
                   : state.foreground != TERMINAL_PANE_COLOR_DEFAULT
                         ? state.foreground
                         : state.base_foreground;
    case TERMINAL_PANE_OSC_COLOR_MOUSE_BACKGROUND:
        return state.mouse_background != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.mouse_background
                   : state.background != TERMINAL_PANE_COLOR_DEFAULT
                         ? state.background
                         : state.base_background;
    case TERMINAL_PANE_OSC_COLOR_SELECTION_BACKGROUND:
        return state.selection_background != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.selection_background
                   : state.base_selection_background;
    case TERMINAL_PANE_OSC_COLOR_SELECTION_FOREGROUND:
        return state.selection_foreground != TERMINAL_PANE_COLOR_DEFAULT
                   ? state.selection_foreground
                   : state.base_selection_foreground;
    default:
        break;
    }
    return TERMINAL_PANE_COLOR_DEFAULT;
}

int
FormatTerminalPaneOSCColorResponse(char *out, int out_size, int code,
                                   int color)
{
    int rgb;
    int r;
    int g;
    int b;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if((color & TERMINAL_PANE_COLOR_TRUE_RGB) == 0)
        return 0;
    rgb = color & 0xffffff;
    r = (rgb >> 16) & 255;
    g = (rgb >> 8) & 255;
    b = rgb & 255;
    return snprintf(out, (size_t)out_size,
                    "\x1b]%d;rgb:%02x%02x/%02x%02x/%02x%02x\a",
                    code, r, r, g, g, b, b);
}

int
FormatTerminalPaneOSCPaletteResponse(char *out, int out_size, int index,
                                     int color)
{
    int rgb;
    int r;
    int g;
    int b;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(index < 0 || index >= 256 ||
       (color & TERMINAL_PANE_COLOR_TRUE_RGB) == 0)
        return 0;
    rgb = color & 0xffffff;
    r = (rgb >> 16) & 255;
    g = (rgb >> 8) & 255;
    b = rgb & 255;
    return snprintf(out, (size_t)out_size,
                    "\x1b]4;%d;rgb:%02x%02x/%02x%02x/%02x%02x\a",
                    index, r, r, g, g, b, b);
}

int
CopyTerminalPaneOSCHyperlinkURL(char *out, int out_size, const char *url)
{
    const unsigned char *cursor = (const unsigned char *)url;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(cursor == NULL)
        return 0;
    while(*cursor != '\0') {
        if(*cursor == '\a' || *cursor == 0x1b || *cursor == 0x7f ||
           *cursor < 0x20) {
            cursor++;
            continue;
        }
        if(used + 1 >= out_size)
            break;
        out[used++] = (char)*cursor++;
    }
    out[used] = '\0';
    return used > 0;
}

int
CopyTerminalPaneOSCHyperlinkID(char *out, int out_size, const char *params)
{
    const char *cursor = params;
    const char *params_end;
    const char *value;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(params == NULL)
        return 0;
    params_end = strchr(params, ';');
    if(params_end == NULL)
        params_end = params + strlen(params);
    while(cursor < params_end) {
        const char *end = cursor;
        int length;

        while(end < params_end && *end != ':')
            end++;
        length = (int)(end - cursor);

        if(length >= 3 && strncmp(cursor, "id=", 3) == 0) {
            value = cursor + 3;
            while(used + 1 < out_size && value < cursor + length) {
                unsigned char ch = (unsigned char)*value++;

                if(ch == '\a' || ch == 0x1b || ch == 0x7f || ch < 0x20)
                    continue;
                out[used++] = (char)ch;
            }
            out[used] = '\0';
            return used > 0;
        }
        if(end >= params_end)
            break;
        cursor = end + 1;
    }
    return 0;
}

int
TerminalPaneOSCTitleTargets(const char *payload, int *window, int *icon)
{
    const char *cursor = payload;
    int saw_token = 0;

    if(window != NULL)
        *window = 0;
    if(icon != NULL)
        *icon = 0;
    if(cursor == NULL || cursor[0] == '\0') {
        if(window != NULL)
            *window = 1;
        if(icon != NULL)
            *icon = 1;
        return 1;
    }
    while(cursor[0] != '\0') {
        int value;

        if(cursor[0] == ';') {
            cursor++;
            continue;
        }
        {
            const char *start = cursor;

            while(cursor[0] != '\0' && cursor[0] != ';')
                cursor++;
            saw_token = 1;
            if(!parse_decimal_span(start, cursor, 999999, &value))
                continue;
        }
        if(value == 0) {
            if(window != NULL)
                *window = 1;
            if(icon != NULL)
                *icon = 1;
        } else if(value == 1) {
            if(icon != NULL)
                *icon = 1;
        } else if(value == 2) {
            if(window != NULL)
                *window = 1;
        }
    }
    if(!saw_token) {
        if(window != NULL)
            *window = 1;
        if(icon != NULL)
            *icon = 1;
    }
    return 1;
}

int
CopyTerminalPaneTitleText(char *out, int out_size, const char *title)
{
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(!append_osc_safe_text(out, out_size, &used, title)) {
        out[0] = '\0';
        return 0;
    }
    return 1;
}

int
FormatTerminalPaneOSCTitleReport(char *out, int out_size, int icon,
                                 const char *title)
{
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(out_size < 5)
        return 0;
    out[used++] = '\x1b';
    out[used++] = ']';
    out[used++] = icon ? 'L' : 'l';
    if(!CopyTerminalPaneTitleText(out + used, out_size - used, title)) {
        out[0] = '\0';
        return 0;
    }
    used += (int)strlen(out + used);
    if(used + 2 >= out_size) {
        out[0] = '\0';
        return 0;
    }
    out[used++] = '\x1b';
    out[used++] = '\\';
    out[used] = '\0';
    return used;
}

void
TerminalPaneOSCPushTitle(char *stack, int depth, int item_size, int *count,
                         const char *value)
{
    int i;

    if(stack == NULL || depth <= 0 || item_size <= 0 || count == NULL)
        return;
    if(value == NULL)
        value = "";
    if(*count < 0)
        *count = 0;
    if(*count >= depth) {
        for(i = 1; i < depth; i++)
            snprintf(stack + (i - 1) * item_size, (size_t)item_size, "%s",
                     stack + i * item_size);
        *count = depth - 1;
    }
    snprintf(stack + (*count) * item_size, (size_t)item_size, "%s", value);
    (*count)++;
}

void
TerminalPaneOSCPopTitle(char *stack, int depth, int item_size, int *count,
                        char *value, int value_size)
{
    char *item;

    if(stack == NULL || depth <= 0 || item_size <= 0 || count == NULL ||
       value == NULL || value_size <= 0)
        return;
    if(*count <= 0)
        return;
    if(*count > depth)
        *count = depth;
    (*count)--;
    item = stack + (*count) * item_size;
    snprintf(value, (size_t)value_size, "%s", item);
    item[0] = '\0';
}

int
DecodeTerminalPaneOSCFileURIPath(char *out, int out_size, const char *uri)
{
    const char *path;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(uri == NULL || strncmp(uri, "file://", 7) != 0)
        return 0;
    path = strchr(uri + 7, '/');
    if(path == NULL)
        return 0;
    while(*path != '\0' && used < out_size - 1) {
        if(*path == '%' && osc_hex_value((unsigned char)path[1]) >= 0 &&
           osc_hex_value((unsigned char)path[2]) >= 0) {
            out[used++] =
                (char)((osc_hex_value((unsigned char)path[1]) << 4) |
                       osc_hex_value((unsigned char)path[2]));
            path += 3;
        } else {
            out[used++] = *path++;
        }
    }
    out[used] = '\0';
    return used > 0;
}

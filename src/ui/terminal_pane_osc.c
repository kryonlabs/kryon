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

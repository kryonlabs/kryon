#include "terminal_pane.h"

#include <stdio.h>
#include <string.h>

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
append_param(char *buffer, int buffer_size, int *used, int *first,
             const char *param)
{
    if(first == NULL)
        return 0;
    if(!*first && !append_text(buffer, buffer_size, used, ";"))
        return 0;
    if(!append_text(buffer, buffer_size, used, param))
        return 0;
    *first = 0;
    return 1;
}

static int
append_number(char *buffer, int buffer_size, int *used, int *first,
              int value)
{
    char text[32];

    snprintf(text, sizeof(text), "%d", value);
    return append_param(buffer, buffer_size, used, first, text);
}

static int
append_color(char *buffer, int buffer_size, int *used, int *first,
             int prefix, int value)
{
    char text[64];
    int rgb;

    if(value == TERMINAL_PANE_COLOR_DEFAULT)
        return 1;
    if((value & TERMINAL_PANE_COLOR_TRUE_RGB) != 0) {
        rgb = value & 0xffffff;
        snprintf(text, sizeof(text), "%d;2;%d;%d;%d", prefix,
                 (rgb >> 16) & 255, (rgb >> 8) & 255, rgb & 255);
    } else {
        snprintf(text, sizeof(text), "%d;5;%d", prefix, value & 255);
    }
    return append_param(buffer, buffer_size, used, first, text);
}

static int
clear_fail(char *out)
{
    if(out != NULL)
        out[0] = '\0';
    return 0;
}

int
FormatTerminalPaneSGRStatus(char *out, int out_size,
                            TerminalPaneSGRStatus status)
{
    int used = 0;
    int first = 1;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if((status.styles & TERMINAL_PANE_SGR_BOLD) &&
       !append_number(out, out_size, &used, &first, 1))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_FAINT) &&
       !append_number(out, out_size, &used, &first, 2))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_ITALIC) &&
       !append_number(out, out_size, &used, &first, 3))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_UNDERLINE) &&
       !append_number(out, out_size, &used, &first, 4))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_BLINK) &&
       !append_number(out, out_size, &used, &first, 5))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_INVERSE) &&
       !append_number(out, out_size, &used, &first, 7))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_CONCEAL) &&
       !append_number(out, out_size, &used, &first, 8))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_STRIKE) &&
       !append_number(out, out_size, &used, &first, 9))
        return clear_fail(out);
    if((status.styles & TERMINAL_PANE_SGR_OVERLINE) &&
       !append_number(out, out_size, &used, &first, 53))
        return clear_fail(out);
    if(!append_color(out, out_size, &used, &first, 38,
                     status.foreground) ||
       !append_color(out, out_size, &used, &first, 48,
                     status.background) ||
       !append_color(out, out_size, &used, &first, 58,
                     status.underline))
        return clear_fail(out);
    if(first && !append_number(out, out_size, &used, &first, 0))
        return clear_fail(out);
    if(!append_text(out, out_size, &used, "m"))
        return clear_fail(out);
    return used;
}

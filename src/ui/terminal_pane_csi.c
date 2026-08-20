#include "terminal_pane.h"

#include <stdio.h>

static int
format_csi_literal(char *out, int out_size, const char *text)
{
    int len;

    if(out == NULL || out_size <= 0 || text == NULL)
        return 0;
    out[0] = '\0';
    len = snprintf(out, (size_t)out_size, "%s", text);
    if(len <= 0 || len >= out_size) {
        out[0] = '\0';
        return 0;
    }
    return len;
}

int
FormatTerminalPaneDeviceStatusReport(char *out, int out_size,
                                     int private_mode, int request,
                                     int cursor_row, int cursor_col)
{
    int len;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(private_mode) {
        if(request == 6) {
            len = snprintf(out, (size_t)out_size, "\x1b[?%d;%dR",
                           cursor_row + 1, cursor_col + 1);
            if(len <= 0 || len >= out_size) {
                out[0] = '\0';
                return 0;
            }
            return len;
        }
        if(request == 15)
            return format_csi_literal(out, out_size, "\x1b[?10n");
        if(request == 25)
            return format_csi_literal(out, out_size, "\x1b[?20n");
        if(request == 26)
            return format_csi_literal(out, out_size, "\x1b[?27;1;0;0n");
        if(request == 55)
            return format_csi_literal(out, out_size, "\x1b[?53n");
        if(request == 56)
            return format_csi_literal(out, out_size, "\x1b[?57;0n");
        if(request == 75)
            return format_csi_literal(out, out_size, "\x1b[?70n");
        if(request == 85)
            return format_csi_literal(out, out_size, "\x1b[?83n");
        return 0;
    }
    if(request == 5)
        return format_csi_literal(out, out_size, "\x1b[0n");
    if(request == 6) {
        len = snprintf(out, (size_t)out_size, "\x1b[%d;%dR",
                       cursor_row + 1, cursor_col + 1);
        if(len <= 0 || len >= out_size) {
            out[0] = '\0';
            return 0;
        }
        return len;
    }
    return 0;
}

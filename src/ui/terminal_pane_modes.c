#include "terminal_pane.h"

#include <stdio.h>

int
TerminalPaneModeReportStatus(TerminalPaneModeState state, int private_mode,
                             int mode)
{
    if(!private_mode) {
        if(mode == 4)
            return state.insert_mode ? 1 : 2;
        if(mode == 20)
            return state.newline_mode ? 1 : 2;
        return 0;
    }
    if(mode == 12)
        return state.cursor_blink ? 1 : 2;
    if(mode == 25)
        return state.cursor_visible ? 1 : 2;
    if(mode == 6)
        return state.origin_mode ? 1 : 2;
    if(mode == 7)
        return state.autowrap ? 1 : 2;
    if(mode == 1)
        return state.application_cursor_keys ? 1 : 2;
    if(mode == 9 || mode == 1000 || mode == 1002 || mode == 1003)
        return state.mouse_mode == mode ? 1 : 2;
    if(mode == 1004)
        return state.focus_reporting ? 1 : 2;
    if(mode == 1005)
        return state.mouse_utf8 ? 1 : 2;
    if(mode == 1006)
        return state.mouse_sgr ? 1 : 2;
    if(mode == 1007)
        return state.alternate_scroll ? 1 : 2;
    if(mode == 1015)
        return state.mouse_urxvt ? 1 : 2;
    if(mode == 1016)
        return state.mouse_pixels ? 1 : 2;
    if(mode == 2004)
        return state.bracketed_paste ? 1 : 2;
    if(mode == 1048)
        return 2;
    if(mode == 47 || mode == 1047 || mode == 1049)
        return state.alternate_screen ? 1 : 2;
    return 0;
}

int
FormatTerminalPaneModeReport(char *out, int out_size,
                             TerminalPaneModeState state, int private_mode,
                             int mode)
{
    int len;
    int status;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    status = TerminalPaneModeReportStatus(state, private_mode, mode);
    if(private_mode)
        len = snprintf(out, (size_t)out_size, "\x1b[?%d;%d$y", mode,
                       status);
    else
        len = snprintf(out, (size_t)out_size, "\x1b[%d;%d$y", mode,
                       status);
    if(len <= 0 || len >= out_size) {
        out[0] = '\0';
        return 0;
    }
    return len;
}

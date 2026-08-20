#include "terminal_pane.h"

#include <stdio.h>

int
TerminalPaneModeStateSetMode(TerminalPaneModeState *state, int mode,
                             int enabled)
{
    if(state == NULL)
        return TERMINAL_PANE_MODE_ACTION_NONE;
    if(mode == 4) {
        state->insert_mode = enabled ? 1 : 0;
        return TERMINAL_PANE_MODE_ACTION_NONE;
    }
    if(mode == 20) {
        state->newline_mode = enabled ? 1 : 0;
        return TERMINAL_PANE_MODE_ACTION_NONE;
    }
    return TERMINAL_PANE_MODE_ACTION_NONE;
}

int
TerminalPaneModeStateSetPrivateMode(TerminalPaneModeState *state, int mode,
                                    int enabled)
{
    if(state == NULL)
        return TERMINAL_PANE_MODE_ACTION_NONE;
    enabled = enabled ? 1 : 0;
    if(mode == 12) {
        state->cursor_blink = enabled;
    } else if(mode == 25) {
        state->cursor_visible = enabled;
    } else if(mode == 6) {
        state->origin_mode = enabled;
        return TERMINAL_PANE_MODE_ACTION_ORIGIN_CURSOR;
    } else if(mode == 7) {
        state->autowrap = enabled;
    } else if(mode == 1) {
        state->application_cursor_keys = enabled;
    } else if(mode == 9 || mode == 1000 || mode == 1002 || mode == 1003) {
        if(enabled)
            state->mouse_mode = mode;
        else if(state->mouse_mode == mode)
            state->mouse_mode = 0;
    } else if(mode == 1004) {
        state->focus_reporting = enabled;
    } else if(mode == 1005) {
        if(enabled) {
            state->mouse_sgr = 0;
            state->mouse_urxvt = 0;
            state->mouse_pixels = 0;
        }
        state->mouse_utf8 = enabled;
    } else if(mode == 1006) {
        if(enabled) {
            state->mouse_utf8 = 0;
            state->mouse_urxvt = 0;
            state->mouse_pixels = 0;
        }
        state->mouse_sgr = enabled;
    } else if(mode == 1007) {
        state->alternate_scroll = enabled;
    } else if(mode == 1015) {
        if(enabled) {
            state->mouse_utf8 = 0;
            state->mouse_sgr = 0;
            state->mouse_pixels = 0;
        }
        state->mouse_urxvt = enabled;
    } else if(mode == 1016) {
        if(enabled) {
            state->mouse_utf8 = 0;
            state->mouse_sgr = 0;
            state->mouse_urxvt = 0;
        }
        state->mouse_pixels = enabled;
    } else if(mode == 2004) {
        state->bracketed_paste = enabled;
    } else if(mode == 1048) {
        return enabled ? TERMINAL_PANE_MODE_ACTION_SAVE_CURSOR
                       : TERMINAL_PANE_MODE_ACTION_RESTORE_CURSOR;
    } else if(mode == 47) {
        state->alternate_screen = enabled;
    } else if(mode == 1047) {
        if(enabled) {
            state->alternate_screen = 1;
            return TERMINAL_PANE_MODE_ACTION_CLEAR_SCREEN;
        }
        state->alternate_screen = 0;
        return TERMINAL_PANE_MODE_ACTION_CLEAR_ALTERNATE;
    } else if(mode == 1049) {
        if(enabled) {
            state->alternate_screen = 1;
            return TERMINAL_PANE_MODE_ACTION_SAVE_CURSOR |
                   TERMINAL_PANE_MODE_ACTION_CLEAR_SCREEN;
        }
        state->alternate_screen = 0;
        return TERMINAL_PANE_MODE_ACTION_CLEAR_ALTERNATE |
               TERMINAL_PANE_MODE_ACTION_RESTORE_CURSOR;
    }
    return TERMINAL_PANE_MODE_ACTION_NONE;
}

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

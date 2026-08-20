#include "terminal_pane.h"

#include <stdio.h>

static int
terminal_pane_mouse_finish(char *out, int out_size, int used)
{
    if(out == NULL || out_size <= 0 || used < 0 || used >= out_size)
        return 0;
    out[used] = '\0';
    return used;
}

static int
terminal_pane_mouse_utf8(char *out, int out_size, int *used,
                         unsigned int codepoint)
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
    return 1;
}

static int
terminal_pane_mouse_format(char *out, int out_size, const char *format,
                           int a, int b, int c, int suffix)
{
    int len;

    if(out == NULL || out_size <= 0 || format == NULL)
        return 0;
    len = snprintf(out, (size_t)out_size, format, a, b, c, suffix);
    if(len < 0 || len >= out_size)
        return 0;
    return len;
}

int
EncodeTerminalPaneMouse(char *out, int out_size, int button, int col, int row,
                        int pixel_x, int pixel_y, int pressed, int motion,
                        int mods, TerminalPaneMouseMode mode)
{
    int cb;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(mode.mode == 0 || col < 0 || row < 0)
        return 0;
    if(pixel_x < 0)
        pixel_x = col;
    if(pixel_y < 0)
        pixel_y = row;
    if(mode.mode == 9 &&
       (motion || !pressed || button >= TERMINAL_PANE_MOUSE_WHEEL_UP))
        return 0;
    if(motion && (mode.mode == 9 || mode.mode == 1000))
        return 0;
    if(motion && mode.mode == 1002 && button == TERMINAL_PANE_MOUSE_RELEASE)
        return 0;

    cb = button;
    if(cb < TERMINAL_PANE_MOUSE_LEFT)
        cb = TERMINAL_PANE_MOUSE_LEFT;
    if(cb > TERMINAL_PANE_MOUSE_WHEEL_DOWN)
        cb = TERMINAL_PANE_MOUSE_RELEASE;
    if(!pressed && !motion && !mode.sgr)
        cb = TERMINAL_PANE_MOUSE_RELEASE;
    if(motion)
        cb += 32;
    if(mods & TERMINAL_PANE_MOD_SHIFT)
        cb += 4;
    if(mods & TERMINAL_PANE_MOD_ALT)
        cb += 8;
    if(mods & TERMINAL_PANE_MOD_CTRL)
        cb += 16;

    if(mode.sgr || mode.pixels) {
        int x = mode.pixels ? pixel_x : col;
        int y = mode.pixels ? pixel_y : row;

        return terminal_pane_mouse_format(out, out_size, "\x1b[<%d;%d;%d%c",
                                          cb, x + 1, y + 1,
                                          pressed || motion ? 'M' : 'm');
    }

    if(mode.urxvt)
        return terminal_pane_mouse_format(out, out_size, "\x1b[%d;%d;%dM",
                                          32 + cb, col + 1, row + 1, 0);

    if(mode.utf8) {
        int used = 3;

        if(out_size <= used)
            return 0;
        out[0] = '\x1b';
        out[1] = '[';
        out[2] = 'M';
        if(!terminal_pane_mouse_utf8(out, out_size, &used,
                                     (unsigned int)(32 + cb)) ||
           !terminal_pane_mouse_utf8(out, out_size, &used,
                                     (unsigned int)(33 + col)) ||
           !terminal_pane_mouse_utf8(out, out_size, &used,
                                     (unsigned int)(33 + row)))
            return 0;
        return terminal_pane_mouse_finish(out, out_size, used);
    }

    if(col > 222 || row > 222 || out_size <= 6)
        return 0;
    out[0] = '\x1b';
    out[1] = '[';
    out[2] = 'M';
    out[3] = (char)(32 + cb);
    out[4] = (char)(33 + col);
    out[5] = (char)(33 + row);
    return terminal_pane_mouse_finish(out, out_size, 6);
}

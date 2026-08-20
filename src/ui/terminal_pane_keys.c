#include "terminal_pane.h"

#include <stdio.h>
#include <string.h>

static int
terminal_pane_key_finish(char *out, int out_size, int used)
{
    if(out == NULL || out_size <= 0 || used < 0 || used >= out_size)
        return 0;
    out[used] = '\0';
    return used;
}

static int
terminal_pane_key_copy(char *out, int out_size, const char *text)
{
    int len;

    if(out == NULL || out_size <= 0 || text == NULL)
        return 0;
    len = (int)strlen(text);
    if(len >= out_size)
        return 0;
    memcpy(out, text, (size_t)len + 1);
    return len;
}

static int
terminal_pane_key_format_modified(char *out, int out_size, int modifier,
                                  unsigned int codepoint)
{
    int len;

    if(out == NULL || out_size <= 0)
        return 0;
    len = snprintf(out, (size_t)out_size, "\x1b[27;%d;%u~", modifier,
                   codepoint);
    if(len < 0 || len >= out_size)
        return 0;
    return len;
}

static int
terminal_pane_key_format_one(char *out, int out_size, const char *format,
                             int value)
{
    int len;

    if(out == NULL || out_size <= 0 || format == NULL)
        return 0;
    len = snprintf(out, (size_t)out_size, format, value);
    if(len < 0 || len >= out_size)
        return 0;
    return len;
}

static int
terminal_pane_key_format_two(char *out, int out_size, const char *format,
                             int a, int b)
{
    int len;

    if(out == NULL || out_size <= 0 || format == NULL)
        return 0;
    len = snprintf(out, (size_t)out_size, format, a, b);
    if(len < 0 || len >= out_size)
        return 0;
    return len;
}

static int
terminal_pane_key_utf8(char *out, int out_size, int *used,
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
terminal_pane_key_control_codepoint(unsigned int codepoint,
                                    unsigned char *out)
{
    if(out == NULL)
        return 0;
    if(codepoint >= 'a' && codepoint <= 'z') {
        *out = (unsigned char)(codepoint - 'a' + 1);
        return 1;
    }
    if(codepoint >= 'A' && codepoint <= 'Z') {
        *out = (unsigned char)(codepoint - 'A' + 1);
        return 1;
    }
    switch(codepoint) {
    case ' ':
    case '@':
    case '2':
        *out = 0x00;
        return 1;
    case '[':
    case '{':
    case '3':
        *out = 0x1b;
        return 1;
    case '\\':
    case '|':
    case '4':
        *out = 0x1c;
        return 1;
    case ']':
    case '}':
    case '5':
        *out = 0x1d;
        return 1;
    case '^':
    case '~':
    case '6':
        *out = 0x1e;
        return 1;
    case '_':
    case '-':
    case '/':
    case '7':
        *out = 0x1f;
        return 1;
    case '?':
    case '8':
        *out = 0x7f;
        return 1;
    default:
        return 0;
    }
}

static int
terminal_pane_key_modifier_number(int mods)
{
    int modifier = 1;

    if(mods & TERMINAL_PANE_MOD_SHIFT)
        modifier += 1;
    if(mods & TERMINAL_PANE_MOD_ALT)
        modifier += 2;
    if(mods & TERMINAL_PANE_MOD_CTRL)
        modifier += 4;
    return modifier;
}

static int
terminal_pane_key_modified_codepoint(char *out, int out_size,
                                     unsigned int codepoint, int mods,
                                     TerminalPaneKeyMode mode)
{
    int modifier;

    if(mode.modify_other_keys <= 0 ||
       (mods & (TERMINAL_PANE_MOD_SHIFT | TERMINAL_PANE_MOD_ALT |
                TERMINAL_PANE_MOD_CTRL)) == 0)
        return 0;
    if(codepoint < 0x20 || codepoint == 0x7f)
        return 0;
    if(mode.modify_other_keys == 1 &&
       (mods & (TERMINAL_PANE_MOD_ALT | TERMINAL_PANE_MOD_CTRL)) == 0)
        return 0;
    modifier = terminal_pane_key_modifier_number(mods);
    if(modifier <= 1)
        return 0;
    return terminal_pane_key_format_modified(out, out_size, modifier,
                                             codepoint);
}

static int
terminal_pane_key_modified_special(char *out, int out_size,
                                   unsigned int codepoint, int mods,
                                   TerminalPaneKeyMode mode)
{
    int modifier;

    if(mode.modify_other_keys <= 0 ||
       (mods & (TERMINAL_PANE_MOD_SHIFT | TERMINAL_PANE_MOD_ALT |
                TERMINAL_PANE_MOD_CTRL)) == 0)
        return 0;
    if(mode.modify_other_keys == 1 &&
       (mods & (TERMINAL_PANE_MOD_ALT | TERMINAL_PANE_MOD_CTRL)) == 0)
        return 0;
    modifier = terminal_pane_key_modifier_number(mods);
    if(modifier <= 1)
        return 0;
    return terminal_pane_key_format_modified(out, out_size, modifier,
                                             codepoint);
}

int
EncodeTerminalPaneCodepoint(char *out, int out_size, unsigned int codepoint,
                            int mods, TerminalPaneKeyMode mode)
{
    int used = 0;

    if(out == NULL || out_size <= 0 || codepoint == 0)
        return 0;
    out[0] = '\0';
    used = terminal_pane_key_modified_codepoint(out, out_size, codepoint, mods,
                                                mode);
    if(used > 0)
        return used;
    if((mods & TERMINAL_PANE_MOD_CTRL) != 0) {
        unsigned char control = 0;

        if(terminal_pane_key_control_codepoint(codepoint, &control)) {
            if((mods & TERMINAL_PANE_MOD_ALT) != 0) {
                if(used + 1 >= out_size)
                    return 0;
                out[used++] = '\x1b';
            }
            if(used + 1 >= out_size)
                return 0;
            out[used++] = (char)control;
            return terminal_pane_key_finish(out, out_size, used);
        }
    }
    if((mods & TERMINAL_PANE_MOD_ALT) != 0) {
        if(used + 1 >= out_size)
            return 0;
        out[used++] = '\x1b';
    }
    if(!terminal_pane_key_utf8(out, out_size, &used, codepoint))
        return 0;
    return terminal_pane_key_finish(out, out_size, used);
}

TerminalPaneMappedKey
MapTerminalPaneFunctionKey(int function_index, int mods)
{
    TerminalPaneMappedKey mapped = {0};

    mapped.mods = mods;
    if(function_index < 1 || function_index > 24)
        return mapped;
    if(function_index <= 12 && (mods & TERMINAL_PANE_MOD_SHIFT) != 0) {
        mapped.key = TERMINAL_PANE_KEY_F13 + function_index - 1;
        mapped.mods = mods & ~TERMINAL_PANE_MOD_SHIFT;
        return mapped;
    }
    mapped.key = TERMINAL_PANE_KEY_F1 + function_index - 1;
    return mapped;
}

int
EncodeTerminalPaneKey(char *out, int out_size, int key, int mods,
                      TerminalPaneKeyMode mode)
{
    char final = 0;
    int modifier = terminal_pane_key_modifier_number(mods);
    const char *plain = NULL;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(key == TERMINAL_PANE_KEY_ENTER) {
        int len = terminal_pane_key_modified_special(out, out_size, 13, mods,
                                                     mode);
        if(len > 0)
            return len;
        plain = "\r";
    } else if(key == TERMINAL_PANE_KEY_BACKSPACE) {
        int len = terminal_pane_key_modified_special(out, out_size, 127, mods,
                                                     mode);
        if(len > 0)
            return len;
        plain = "\x7f";
    } else if(key == TERMINAL_PANE_KEY_TAB) {
        if((mods & TERMINAL_PANE_MOD_SHIFT) != 0) {
            if((mods & TERMINAL_PANE_MOD_ALT) != 0)
                return terminal_pane_key_copy(out, out_size, "\x1b\x1b[Z");
            return terminal_pane_key_copy(out, out_size, "\x1b[Z");
        }
        plain = "\t";
    } else if(key == TERMINAL_PANE_KEY_ESCAPE) {
        int len = terminal_pane_key_modified_special(out, out_size, 27, mods,
                                                     mode);
        if(len > 0)
            return len;
        plain = "\x1b";
    } else if(key == TERMINAL_PANE_KEY_UP)
        final = 'A';
    else if(key == TERMINAL_PANE_KEY_DOWN)
        final = 'B';
    else if(key == TERMINAL_PANE_KEY_RIGHT)
        final = 'C';
    else if(key == TERMINAL_PANE_KEY_LEFT)
        final = 'D';
    else if(key == TERMINAL_PANE_KEY_HOME)
        final = 'H';
    else if(key == TERMINAL_PANE_KEY_END)
        final = 'F';
    else if(key == TERMINAL_PANE_KEY_INSERT ||
            key == TERMINAL_PANE_KEY_DELETE ||
            key == TERMINAL_PANE_KEY_PAGE_UP ||
            key == TERMINAL_PANE_KEY_PAGE_DOWN) {
        int code = 2;

        if(key == TERMINAL_PANE_KEY_DELETE)
            code = 3;
        else if(key == TERMINAL_PANE_KEY_PAGE_UP)
            code = 5;
        else if(key == TERMINAL_PANE_KEY_PAGE_DOWN)
            code = 6;
        if(modifier == 1)
            return terminal_pane_key_format_one(out, out_size, "\x1b[%d~",
                                                code);
        return terminal_pane_key_format_two(out, out_size, "\x1b[%d;%d~",
                                            code, modifier);
    } else if(key >= TERMINAL_PANE_KEY_F1 && key <= TERMINAL_PANE_KEY_F24) {
        static const int function_codes[] = {
            11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24,
            11, 12, 13, 14, 15, 17, 18, 19, 20, 21, 23, 24
        };
        int index = key - TERMINAL_PANE_KEY_F1;
        int code = function_codes[index];
        int effective_modifier = modifier;

        if(index >= 12) {
            index -= 12;
            if((mods & TERMINAL_PANE_MOD_SHIFT) == 0)
                effective_modifier += 1;
        }
        if(index < 4 && effective_modifier == 1) {
            if(out_size <= 3)
                return 0;
            out[0] = '\x1b';
            out[1] = 'O';
            out[2] = (char)('P' + index);
            return terminal_pane_key_finish(out, out_size, 3);
        }
        if(index < 4) {
            int len = snprintf(out, (size_t)out_size, "\x1b[1;%d%c",
                               effective_modifier, 'P' + index);
            if(len < 0 || len >= out_size)
                return 0;
            return len;
        }
        if(effective_modifier == 1)
            return terminal_pane_key_format_one(out, out_size, "\x1b[%d~",
                                                code);
        return terminal_pane_key_format_two(out, out_size, "\x1b[%d;%d~",
                                            code, effective_modifier);
    }

    if(plain != NULL) {
        if((mods & TERMINAL_PANE_MOD_ALT) != 0) {
            if(out_size <= 1)
                return 0;
            out[0] = '\x1b';
            return terminal_pane_key_copy(out + 1, out_size - 1, plain) > 0
                       ? (int)strlen(out)
                       : 0;
        }
        return terminal_pane_key_copy(out, out_size, plain);
    }
    if(final != 0) {
        if(modifier == 1 && mode.application_cursor_keys &&
           (final == 'A' || final == 'B' || final == 'C' || final == 'D' ||
            final == 'H' || final == 'F')) {
            if(out_size <= 3)
                return 0;
            out[0] = '\x1b';
            out[1] = 'O';
            out[2] = final;
            return terminal_pane_key_finish(out, out_size, 3);
        }
        if(modifier == 1) {
            if(out_size <= 3)
                return 0;
            out[0] = '\x1b';
            out[1] = '[';
            out[2] = final;
            return terminal_pane_key_finish(out, out_size, 3);
        }
        {
            int len = snprintf(out, (size_t)out_size, "\x1b[1;%d%c",
                               modifier, final);
            if(len < 0 || len >= out_size)
                return 0;
            return len;
        }
    }
    return 0;
}

int
EncodeTerminalPaneKeypad(char *out, int out_size, char key,
                         TerminalPaneKeyMode mode)
{
    char app = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(!mode.application_keypad) {
        if(out_size <= 1)
            return 0;
        out[0] = key;
        return terminal_pane_key_finish(out, out_size, 1);
    }
    if(key >= '0' && key <= '9')
        app = (char)('p' + key - '0');
    else if(key == '.')
        app = 'n';
    else if(key == '/')
        app = 'o';
    else if(key == '*')
        app = 'j';
    else if(key == '-')
        app = 'm';
    else if(key == '+')
        app = 'k';
    else if(key == '=')
        app = 'X';
    else if(key == '\r')
        app = 'M';
    if(app == 0 || out_size <= 3)
        return 0;
    out[0] = '\x1b';
    out[1] = 'O';
    out[2] = app;
    return terminal_pane_key_finish(out, out_size, 3);
}

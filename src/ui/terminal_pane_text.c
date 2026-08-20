#include "terminal_pane.h"

int
AppendTerminalPaneUTF8Codepoint(char *out, int out_size, int *used,
                                unsigned int codepoint)
{
    if(out == NULL || used == NULL || out_size <= 0 || *used < 0 ||
       *used >= out_size)
        return 0;
    out[*used] = '\0';
    if(codepoint == 0 || codepoint > 0x10ffff)
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
    } else {
        if(*used + 4 >= out_size)
            return 0;
        out[(*used)++] = (char)(0xf0 | (codepoint >> 18));
        out[(*used)++] = (char)(0x80 | ((codepoint >> 12) & 0x3f));
        out[(*used)++] = (char)(0x80 | ((codepoint >> 6) & 0x3f));
        out[(*used)++] = (char)(0x80 | (codepoint & 0x3f));
    }
    out[*used] = '\0';
    return 1;
}

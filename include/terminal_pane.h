#ifndef TERMINAL_PANE_H
#define TERMINAL_PANE_H

#include "kryon_compat.generated.h"
#include "terminal.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct TerminalPaneColors {
    Color background;
    Color text;
    Color muted_text;
    Color selection;
    Color selection_text;
    Color cursor;
    Color border;
    Color scroll_indicator;
    Color scroll_indicator_text;
    Color bell_overlay;
    Color bell_border;
} TerminalPaneColors;

typedef struct TerminalPanePalette {
    Color ansi[256];
} TerminalPanePalette;

typedef struct TerminalPaneMetrics {
    int cols;
    int rows;
    int cell_width;
    int line_height;
    Rectangle content;
} TerminalPaneMetrics;

typedef struct TerminalPane {
    Rectangle bounds;
    Terminal *terminal;
    int focused;
    int font_size;
    int padding;
    int show_cursor;
    int handle_input;
    TerminalPaneColors colors;
} TerminalPane;

typedef struct TerminalPaneResult {
    int focused;
    int cols;
    int rows;
    int wrote_input;
} TerminalPaneResult;

TerminalPaneColors GetTerminalPaneThemeColors(void);
static inline TerminalPanePalette GetTerminalPaneDefaultPalette(void)
{
    static const Color base16[16] = {
        {24, 24, 24, 255},     {205, 49, 49, 255},
        {13, 188, 121, 255},   {229, 229, 16, 255},
        {36, 114, 200, 255},   {188, 63, 188, 255},
        {17, 168, 205, 255},   {229, 229, 229, 255},
        {102, 102, 102, 255},  {241, 76, 76, 255},
        {35, 209, 139, 255},   {245, 245, 67, 255},
        {59, 142, 234, 255},   {214, 112, 214, 255},
        {41, 184, 219, 255},   {255, 255, 255, 255},
    };
    TerminalPanePalette palette = {0};
    int i;
    int r;
    int g;
    int b;

    for(i = 0; i < 16; i++)
        palette.ansi[i] = base16[i];
    i = 16;
    for(r = 0; r < 6; r++) {
        for(g = 0; g < 6; g++) {
            for(b = 0; b < 6; b++) {
                palette.ansi[i++] =
                    (Color){(unsigned char)(r == 0 ? 0 : 55 + r * 40),
                            (unsigned char)(g == 0 ? 0 : 55 + g * 40),
                            (unsigned char)(b == 0 ? 0 : 55 + b * 40),
                            255};
            }
        }
    }
    for(i = 232; i < 256; i++) {
        unsigned char gray = (unsigned char)(8 + (i - 232) * 10);

        palette.ansi[i] = (Color){gray, gray, gray, 255};
    }
    return palette;
}
TerminalPaneMetrics MeasureTerminalPane(Rectangle bounds, int font_size, int padding);
Rectangle TerminalPaneContentBounds(Rectangle bounds, int top_inset,
                                    int padding);
TerminalPaneMetrics MeasureTerminalPaneContent(Rectangle content,
                                               int font_size);
int TerminalPaneHandleInput(Terminal *terminal);
TerminalPaneResult DrawTerminalPane(TerminalPane pane);

#ifdef __cplusplus
}
#endif

#endif

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
    Color cursor;
    Color border;
} TerminalPaneColors;

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
TerminalPaneMetrics MeasureTerminalPane(Rectangle bounds, int font_size, int padding);
int TerminalPaneHandleInput(Terminal *terminal);
TerminalPaneResult DrawTerminalPane(TerminalPane pane);

#ifdef __cplusplus
}
#endif

#endif

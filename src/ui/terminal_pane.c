#include "terminal_pane.h"

#include "theme.h"
#include "ui_clip.h"
#include "ui_scaling.h"
#include "ui_text.h"

static int
pane_color_empty(Color color)
{
    return color.r == 0 && color.g == 0 && color.b == 0 && color.a == 0;
}

static Color
pane_color(Color configured, Color fallback)
{
    return pane_color_empty(configured) ? fallback : configured;
}

TerminalPaneColors
GetTerminalPaneThemeColors(void)
{
    Color bg = GetThemeBackground();
    Color text = GetThemeText();
    Color surface = GetThemeSurface();
    Color link = GetThemeLink();
    Color border =
        Fade(surface.r == bg.r && surface.g == bg.g && surface.b == bg.b
                 ? text
                 : surface,
             0.72f);
    Color selection = Fade(link, 0.28f);

    return (TerminalPaneColors){
        bg,
        text,
        Fade(text, 0.70f),
        selection,
        bg,
        text,
        link,
        border,
        Fade(bg, 0.82f),
        Fade(text, 0.78f),
        Fade(link, 0.16f),
        Fade(link, 0.85f)
    };
}

TerminalPaneMetrics
MeasureTerminalPaneContent(Rectangle content, int font_size)
{
    TerminalPaneMetrics metrics = {0};
    int font;

    font = font_size > 0 ? font_size : ScaleUIPx(13);
    metrics.cell_width = MeasureUIText("M", font);
    if(metrics.cell_width < 6)
        metrics.cell_width = font * 6 / 10;
    metrics.line_height = GetUITextLineHeight(font);
    if(metrics.line_height < font + 2)
        metrics.line_height = font + ScaleUIPx(2);
    metrics.content = content;
    if(metrics.content.width < 0)
        metrics.content.width = 0;
    if(metrics.content.height < 0)
        metrics.content.height = 0;
    metrics.cols = metrics.cell_width > 0 ? (int)(metrics.content.width / metrics.cell_width) : 0;
    metrics.rows = metrics.line_height > 0 ? (int)(metrics.content.height / metrics.line_height) : 0;
    if(metrics.cols < 8)
        metrics.cols = 8;
    if(metrics.rows < 4)
        metrics.rows = 4;
    if(metrics.cols > TERMINAL_MAX_COLS)
        metrics.cols = TERMINAL_MAX_COLS;
    if(metrics.rows > TERMINAL_MAX_ROWS)
        metrics.rows = TERMINAL_MAX_ROWS;
    return metrics;
}

Rectangle
TerminalPaneContentBounds(Rectangle bounds, int top_inset, int padding)
{
    int top = top_inset > 0 ? top_inset : 0;
    int pad = padding >= 0 ? padding : ScaleUIPx(6);
    Rectangle content = {
        bounds.x + (float)pad,
        bounds.y + (float)(top + pad),
        bounds.width - (float)(pad * 2),
        bounds.height - (float)(top + pad * 2)
    };

    if(content.width < 0)
        content.width = 0;
    if(content.height < 0)
        content.height = 0;
    return content;
}

TerminalPaneMetrics
MeasureTerminalPane(Rectangle bounds, int font_size, int padding)
{
    return MeasureTerminalPaneContent(
        TerminalPaneContentBounds(bounds, 0, padding), font_size);
}

int
TerminalPaneHandleInput(Terminal *terminal)
{
    int wrote = 0;
    int control = IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL);
    int shift = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);
    int ch;
    char seq[8];

    if(terminal == NULL || !terminal->running)
        return 0;

    if(control && !shift) {
        if(IsKeyPressed(KEY_C))
            wrote += TerminalWrite(terminal, "\x03", 1);
        if(IsKeyPressed(KEY_D))
            wrote += TerminalWrite(terminal, "\x04", 1);
        if(IsKeyPressed(KEY_L))
            wrote += TerminalWrite(terminal, "\x0c", 1);
        if(wrote > 0)
            return wrote;
    }
    if(IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_KP_ENTER))
        wrote += TerminalWrite(terminal, "\r", 1);
    if(IsKeyPressed(KEY_BACKSPACE))
        wrote += TerminalWrite(terminal, "\x7f", 1);
    if(IsKeyPressed(KEY_TAB))
        wrote += TerminalWrite(terminal, "\t", 1);
    if(IsKeyPressed(KEY_UP))
        wrote += TerminalWrite(terminal, "\x1b[A", 3);
    if(IsKeyPressed(KEY_DOWN))
        wrote += TerminalWrite(terminal, "\x1b[B", 3);
    if(IsKeyPressed(KEY_RIGHT))
        wrote += TerminalWrite(terminal, "\x1b[C", 3);
    if(IsKeyPressed(KEY_LEFT))
        wrote += TerminalWrite(terminal, "\x1b[D", 3);
    if(IsKeyPressed(KEY_HOME))
        wrote += TerminalWrite(terminal, "\x1b[H", 3);
    if(IsKeyPressed(KEY_END))
        wrote += TerminalWrite(terminal, "\x1b[F", 3);

    ch = GetCharPressed();
    while(ch > 0) {
        if(ch >= 32 && ch < 127) {
            seq[0] = (char)ch;
            wrote += TerminalWrite(terminal, seq, 1);
        }
        ch = GetCharPressed();
    }
    return wrote;
}

TerminalPaneResult
DrawTerminalPane(TerminalPane pane)
{
    TerminalPaneResult result = {0};
    TerminalPaneMetrics metrics;
    TerminalPaneColors theme;
    Color background;
    Color text;
    Color cursor;
    Color border;
    char line[512];
    int font;
    int row;
    int cursor_on;

    metrics = MeasureTerminalPane(pane.bounds, pane.font_size, pane.padding);
    result.focused = pane.focused;
    result.cols = metrics.cols;
    result.rows = metrics.rows;

    theme = GetTerminalPaneThemeColors();
    background = pane_color(pane.colors.background, theme.background);
    text = pane_color(pane.colors.text, theme.text);
    cursor = pane_color(pane.colors.cursor, theme.cursor);
    border = pane_color(pane.colors.border, theme.border);
    font = pane.font_size > 0 ? pane.font_size : ScaleUIPx(13);

    DrawRectangleRec(pane.bounds, background);
    DrawRectangleLinesEx(pane.bounds, 1.0f, border);

    if(CheckCollisionPointRec(GetMousePosition(), pane.bounds) &&
       IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
        result.focused = 1;

    if(pane.terminal != NULL) {
        if(pane.terminal->running &&
           (pane.terminal->cols != metrics.cols || pane.terminal->rows != metrics.rows))
            TerminalResize(pane.terminal, metrics.cols, metrics.rows);
        if(result.focused && pane.handle_input)
            result.wrote_input = TerminalPaneHandleInput(pane.terminal);
    }

    BeginUIClip((int)metrics.content.x, (int)metrics.content.y,
                (int)metrics.content.width, (int)metrics.content.height);
    if(pane.terminal != NULL) {
        int rows = pane.terminal->rows;

        if(rows > metrics.rows)
            rows = metrics.rows;
        for(row = 0; row < rows; row++) {
            TerminalLine(pane.terminal, row, line, (int)sizeof(line));
            DrawUIText(line, (int)metrics.content.x,
                       (int)metrics.content.y + row * metrics.line_height,
                       font, text);
        }
        cursor_on = ((int)(GetTime() * 2.0) & 1) == 0;
        if(result.focused && pane.show_cursor && cursor_on &&
           pane.terminal->cx >= 0 && pane.terminal->cy >= 0 &&
           pane.terminal->cx < metrics.cols && pane.terminal->cy < metrics.rows) {
            DrawRectangle((int)metrics.content.x + pane.terminal->cx * metrics.cell_width,
                          (int)metrics.content.y + pane.terminal->cy * metrics.line_height,
                          metrics.cell_width, metrics.line_height, cursor);
        }
    }
    EndUIClip();

    return result;
}

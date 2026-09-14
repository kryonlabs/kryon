#include "terminal_pane.h"

#include "theme.h"
#include "ui_clip.h"
#include "ui_internal.h"
#include "ui_scaling.h"
#include "ui_style_internal.h"

#include "runtime/terminal_pane.h"

#include <stdio.h>

static float
terminal_pane_runtime_scale(void)
{
    return (float)Scale(1000) / 1000.0f;
}

static int
pane_color_visible(Color color)
{
    return color.a != 0;
}

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
    Style app = ui_unpack_style(ResolveActiveStyle(
        ui_pack_style_states((ControlStyle){.normal = {.opacity = 1}}).normal,
        StyleDefaultFacts(StyleKindApp()),
        ButtonStateNormal));
    Style surface_style = ui_surface_style();
    Style text_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindText());
    Style link_style = ui_resolve_button_style_kind((ButtonProps){0},
                                                    ButtonStateNormal,
                                                    StyleKindLink());
    Color bg = app.background.a != 0 ? app.background : surface_style.background;
    Color text = text_style.foreground;
    Color surface = surface_style.background;
    Color link = link_style.foreground;
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

TerminalPaneColors
ResolveTerminalPaneThemeColors(TerminalPaneColors colors)
{
    TerminalPaneColors fallback = GetTerminalPaneThemeColors();

    if(!pane_color_visible(colors.background))
        colors.background = fallback.background;
    if(!pane_color_visible(colors.text))
        colors.text = fallback.text;
    if(!pane_color_visible(colors.muted_text))
        colors.muted_text = fallback.muted_text;
    if(!pane_color_visible(colors.selection))
        colors.selection = fallback.selection;
    if(!pane_color_visible(colors.selection_text))
        colors.selection_text = fallback.selection_text;
    if(!pane_color_visible(colors.cursor))
        colors.cursor = fallback.cursor;
    if(!pane_color_visible(colors.link))
        colors.link = fallback.link;
    if(!pane_color_visible(colors.border))
        colors.border = fallback.border;
    if(!pane_color_visible(colors.scroll_indicator))
        colors.scroll_indicator = fallback.scroll_indicator;
    if(!pane_color_visible(colors.scroll_indicator_text))
        colors.scroll_indicator_text = fallback.scroll_indicator_text;
    if(!pane_color_visible(colors.bell_overlay))
        colors.bell_overlay = fallback.bell_overlay;
    if(!pane_color_visible(colors.bell_border))
        colors.bell_border = fallback.bell_border;
    return colors;
}

TerminalPaneMetrics
MeasureTerminalPaneContent(Rectangle content, int font_size)
{
    TerminalPaneMetrics metrics = {0};
    TerminalPaneMetricsPolicy policy;
    int font;

    policy = TerminalPaneMetricsPolicyFor(terminal_pane_runtime_scale());
    font = TerminalPaneFontSizeFor(font_size, policy);
    metrics.cell_width = TextWidth("M", font);
    if(metrics.cell_width < 6)
        metrics.cell_width = font * 6 / 10;
    metrics.line_height = TerminalPaneLineHeightFor(TextLineHeight(font),
                                                    font, policy);
    metrics.content = content;
    if(metrics.content.width < 0)
        metrics.content.width = 0;
    if(metrics.content.height < 0)
        metrics.content.height = 0;
    metrics.cols = metrics.cell_width > 0 ? (int)(metrics.content.width / metrics.cell_width) : 0;
    metrics.rows = metrics.line_height > 0 ? (int)(metrics.content.height / metrics.line_height) : 0;
    metrics.cols = TerminalPaneClampCols(metrics.cols, TERMINAL_MAX_COLS,
                                         policy);
    metrics.rows = TerminalPaneClampRows(metrics.rows, TERMINAL_MAX_ROWS,
                                         policy);
    return metrics;
}

Rectangle
TerminalPaneContentBounds(Rectangle bounds, int top_inset, int padding)
{
    TerminalPaneMetricsPolicy policy =
        TerminalPaneMetricsPolicyFor(terminal_pane_runtime_scale());
    return TerminalPaneContentBoundsFor(bounds, top_inset,
                                        TerminalPanePaddingFor(padding,
                                                               policy));
}

TerminalPaneMetrics
MeasureTerminalPane(Rectangle bounds, int font_size, int padding)
{
    return MeasureTerminalPaneContent(
        TerminalPaneContentBounds(bounds, 0, padding), font_size);
}

int
FormatTerminalPaneScrollIndicatorLabel(char *out, int out_size,
                                       int scroll_offset)
{
    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(scroll_offset <= 0)
        return 0;
    return snprintf(out, (size_t)out_size, "%d lines", scroll_offset);
}

Rectangle
MeasureTerminalPaneScrollIndicator(TerminalPaneScrollIndicator indicator)
{
    char label[64];
    TerminalPaneScrollIndicatorMetrics metrics;
    int font;
    int label_w;
    Rectangle badge = {0};

    if(indicator.viewport.width <= 0.0f || indicator.viewport.height <= 0.0f ||
       FormatTerminalPaneScrollIndicatorLabel(label, (int)sizeof(label),
                                              indicator.scroll_offset) <= 0)
        return badge;
    metrics = TerminalPaneScrollIndicatorMetricsFor(
        terminal_pane_runtime_scale());
    font = indicator.font_size > 0 ? indicator.font_size : metrics.font_size;
    label_w = TextWidth(label, font);
    return TerminalPaneScrollIndicatorBoundsFor(indicator.viewport, label_w,
                                                metrics);
}

Rectangle
DrawTerminalPaneScrollIndicator(TerminalPaneScrollIndicator indicator)
{
    TerminalPaneColors colors;
    Rectangle badge;
    char label[64];
    TerminalPaneScrollIndicatorMetrics metrics;
    int font;

    badge = MeasureTerminalPaneScrollIndicator(indicator);
    if(badge.width <= 0.0f || badge.height <= 0.0f ||
       FormatTerminalPaneScrollIndicatorLabel(label, (int)sizeof(label),
                                              indicator.scroll_offset) <= 0)
        return badge;
    colors = ResolveTerminalPaneThemeColors(indicator.colors);
    metrics = TerminalPaneScrollIndicatorMetricsFor(
        terminal_pane_runtime_scale());
    font = indicator.font_size > 0 ? indicator.font_size : metrics.font_size;
    DrawRectangleRec(badge, colors.scroll_indicator);
    DrawRectangleLines((int)badge.x, (int)badge.y, (int)badge.width,
                       (int)badge.height, colors.border);
    RenderText(label, (int)badge.x + metrics.text_x,
               (int)badge.y + metrics.text_y, font,
               colors.scroll_indicator_text);
    return badge;
}

#if !defined(KRYON_NATIVE_PLAN9)
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
    TerminalPaneMetricsPolicy policy;

    metrics = MeasureTerminalPane(pane.bounds, pane.font_size, pane.padding);
    result.focused = pane.focused;
    result.cols = metrics.cols;
    result.rows = metrics.rows;

    theme = GetTerminalPaneThemeColors();
    background = pane_color(pane.colors.background, theme.background);
    text = pane_color(pane.colors.text, theme.text);
    cursor = pane_color(pane.colors.cursor, theme.cursor);
    border = pane_color(pane.colors.border, theme.border);
    policy = TerminalPaneMetricsPolicyFor(terminal_pane_runtime_scale());
    font = TerminalPaneFontSizeFor(pane.font_size, policy);

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

    BeginClip((int)metrics.content.x, (int)metrics.content.y,
                (int)metrics.content.width, (int)metrics.content.height);
    if(pane.terminal != NULL) {
        int rows = pane.terminal->rows;

        if(rows > metrics.rows)
            rows = metrics.rows;
        for(row = 0; row < rows; row++) {
            TerminalLine(pane.terminal, row, line, (int)sizeof(line));
            RenderText(line, (int)metrics.content.x,
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
    EndClip();

    return result;
}
#endif

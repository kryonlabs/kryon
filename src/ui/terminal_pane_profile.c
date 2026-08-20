#include "terminal_pane.h"

#include <stddef.h>
#include <stdio.h>
#include <string.h>

static int
color_visible(Color color)
{
    return color.a != 0;
}

static int
resolve_profile_color(int configured, Color fallback)
{
    if(configured != TERMINAL_PANE_COLOR_DEFAULT)
        return configured;
    if(color_visible(fallback))
        return TerminalPaneColorToRGB(fallback);
    return TERMINAL_PANE_COLOR_DEFAULT;
}

static int
hex_value(int ch)
{
    if(ch >= '0' && ch <= '9')
        return ch - '0';
    if(ch >= 'a' && ch <= 'f')
        return 10 + ch - 'a';
    if(ch >= 'A' && ch <= 'F')
        return 10 + ch - 'A';
    return -1;
}

int
TerminalPaneColorToRGB(Color color)
{
    if(!color_visible(color))
        return TERMINAL_PANE_COLOR_DEFAULT;
    return TERMINAL_PANE_COLOR_TRUE_RGB | ((int)color.r << 16) |
           ((int)color.g << 8) | (int)color.b;
}

TerminalPaneProfileColors
TerminalPaneProfileColorsFromTheme(TerminalPaneColors colors)
{
    TerminalPaneProfileColors profile;

    profile.foreground = TerminalPaneColorToRGB(colors.text);
    profile.background = TerminalPaneColorToRGB(colors.background);
    profile.cursor = TerminalPaneColorToRGB(colors.cursor);
    profile.selection_foreground = TerminalPaneColorToRGB(colors.selection_text);
    if(profile.selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        profile.selection_foreground = profile.background;
    profile.selection_background = TerminalPaneColorToRGB(colors.selection);
    return profile;
}

TerminalPaneProfileColors
ResolveTerminalPaneProfileColors(TerminalPaneProfileColors configured,
                                 TerminalPaneColors fallback)
{
    TerminalPaneProfileColors colors;

    colors.foreground =
        resolve_profile_color(configured.foreground, fallback.text);
    colors.background =
        resolve_profile_color(configured.background, fallback.background);
    colors.cursor = resolve_profile_color(configured.cursor, fallback.cursor);
    colors.selection_foreground = resolve_profile_color(
        configured.selection_foreground, fallback.selection_text);
    if(colors.selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        colors.selection_foreground = colors.background;
    colors.selection_background = resolve_profile_color(
        configured.selection_background, fallback.selection);
    return colors;
}

void
TerminalPaneProfileStateApplyNew(TerminalPaneProfileState *state,
                                 TerminalPaneProfileColors colors)
{
    if(state == NULL)
        return;
    state->base_foreground = colors.foreground;
    state->base_background = colors.background;
    state->base_cursor = colors.cursor;
    state->base_selection_foreground = colors.selection_foreground;
    state->base_selection_background = colors.selection_background;
    state->foreground = colors.foreground;
    state->background = colors.background;
    if(state->cursor == TERMINAL_PANE_COLOR_DEFAULT)
        state->cursor = state->base_cursor;
    if(state->selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_foreground = state->base_selection_foreground;
    if(state->selection_background == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_background = state->base_selection_background;
}

void
TerminalPaneProfileStateSeedMissing(TerminalPaneProfileState *state,
                                    TerminalPaneProfileColors colors)
{
    if(state == NULL)
        return;
    state->base_foreground = colors.foreground;
    state->base_background = colors.background;
    state->base_cursor = colors.cursor;
    state->base_selection_foreground = colors.selection_foreground;
    state->base_selection_background = colors.selection_background;
    if(state->foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->foreground = state->base_foreground;
    if(state->background == TERMINAL_PANE_COLOR_DEFAULT)
        state->background = state->base_background;
    if(state->cursor == TERMINAL_PANE_COLOR_DEFAULT)
        state->cursor = state->base_cursor;
    if(state->selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_foreground = state->base_selection_foreground;
    if(state->selection_background == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_background = state->base_selection_background;
}

void
TerminalPaneProfileStateSyncChanged(TerminalPaneProfileState *state,
                                    TerminalPaneProfileColors old_colors,
                                    TerminalPaneProfileColors new_colors)
{
    if(state == NULL)
        return;
    if(state->base_foreground == old_colors.foreground ||
       state->base_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->base_foreground = new_colors.foreground;
    if(state->base_background == old_colors.background ||
       state->base_background == TERMINAL_PANE_COLOR_DEFAULT)
        state->base_background = new_colors.background;
    if(state->base_cursor == old_colors.cursor ||
       state->base_cursor == TERMINAL_PANE_COLOR_DEFAULT)
        state->base_cursor = new_colors.cursor;
    if(state->foreground == old_colors.foreground ||
       state->foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->foreground = new_colors.foreground;
    if(state->background == old_colors.background ||
       state->background == TERMINAL_PANE_COLOR_DEFAULT)
        state->background = new_colors.background;
    if(state->cursor == old_colors.cursor ||
       state->cursor == TERMINAL_PANE_COLOR_DEFAULT)
        state->cursor = new_colors.cursor;
    if(state->base_selection_foreground == old_colors.selection_foreground ||
       state->base_selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->base_selection_foreground = new_colors.selection_foreground;
    if(state->base_selection_background == old_colors.selection_background ||
       state->base_selection_background == TERMINAL_PANE_COLOR_DEFAULT)
        state->base_selection_background = new_colors.selection_background;
    if(state->selection_foreground == old_colors.selection_foreground ||
       state->selection_foreground == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_foreground = new_colors.selection_foreground;
    if(state->selection_background == old_colors.selection_background ||
       state->selection_background == TERMINAL_PANE_COLOR_DEFAULT)
        state->selection_background = new_colors.selection_background;
}

int
ParseTerminalPaneCursorStyle(const char *text, int fallback)
{
    if(text == NULL || text[0] == '\0')
        return fallback;
    if(strcmp(text, "default") == 0 || strcmp(text, "0") == 0)
        return TERMINAL_PANE_CURSOR_DEFAULT;
    if(strcmp(text, "block") == 0 || strcmp(text, "1") == 0)
        return TERMINAL_PANE_CURSOR_BLOCK;
    if(strcmp(text, "underline") == 0 || strcmp(text, "2") == 0)
        return TERMINAL_PANE_CURSOR_UNDERLINE;
    if(strcmp(text, "bar") == 0 || strcmp(text, "beam") == 0 ||
       strcmp(text, "3") == 0)
        return TERMINAL_PANE_CURSOR_BAR;
    return fallback;
}

const char *
TerminalPaneCursorStyleName(int style)
{
    if(style == TERMINAL_PANE_CURSOR_UNDERLINE)
        return "underline";
    if(style == TERMINAL_PANE_CURSOR_BAR)
        return "bar";
    return "block";
}

int
ParseTerminalPaneProfileColor(const char *text, int *out)
{
    int r1;
    int r2;
    int g1;
    int g2;
    int b1;
    int b2;

    if(out == NULL || text == NULL)
        return 0;
    if(strcmp(text, "default") == 0 || strcmp(text, "system") == 0 ||
       strcmp(text, "theme") == 0) {
        *out = TERMINAL_PANE_COLOR_DEFAULT;
        return 1;
    }
    if(text[0] == '#')
        text++;
    if(strlen(text) != 6)
        return 0;
    r1 = hex_value((unsigned char)text[0]);
    r2 = hex_value((unsigned char)text[1]);
    g1 = hex_value((unsigned char)text[2]);
    g2 = hex_value((unsigned char)text[3]);
    b1 = hex_value((unsigned char)text[4]);
    b2 = hex_value((unsigned char)text[5]);
    if(r1 < 0 || r2 < 0 || g1 < 0 || g2 < 0 || b1 < 0 || b2 < 0)
        return 0;
    *out = TERMINAL_PANE_COLOR_TRUE_RGB | (((r1 << 4) | r2) << 16) |
           (((g1 << 4) | g2) << 8) | ((b1 << 4) | b2);
    return 1;
}

int
FormatTerminalPaneProfileColor(char *out, int out_size, int color)
{
    if(out == NULL || out_size <= 0 || color == TERMINAL_PANE_COLOR_DEFAULT)
        return 0;
    if((color & TERMINAL_PANE_COLOR_TRUE_RGB) == 0)
        return 0;
    return snprintf(out, (size_t)out_size, "#%06x", color & 0xffffff);
}

int
EscapeTerminalPaneText(char *out, int out_size, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    if(cursor == NULL)
        cursor = (const unsigned char *)"";
    while(*cursor != '\0' && used < out_size - 1) {
        const char *escaped = NULL;

        if(*cursor == '\\')
            escaped = "\\\\";
        else if(*cursor == '\t')
            escaped = "\\t";
        else if(*cursor == '\n')
            escaped = "\\n";
        else if(*cursor == '\r')
            escaped = "\\r";
        if(escaped != NULL) {
            if(used + 2 >= out_size)
                break;
            out[used++] = escaped[0];
            out[used++] = escaped[1];
        } else {
            out[used++] = (char)*cursor;
        }
        cursor++;
    }
    out[used] = '\0';
    return used;
}

int
UnescapeTerminalPaneText(char *out, int out_size, const char *text)
{
    int used = 0;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(text == NULL)
        return 0;
    while(*text != '\0' && used < out_size - 1) {
        if(*text == '\\' && text[1] != '\0') {
            text++;
            if(*text == 't')
                out[used++] = '\t';
            else if(*text == 'n')
                out[used++] = '\n';
            else if(*text == 'r')
                out[used++] = '\r';
            else
                out[used++] = *text;
        } else {
            out[used++] = *text;
        }
        text++;
    }
    out[used] = '\0';
    return used;
}

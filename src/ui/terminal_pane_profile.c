#include "terminal_pane.h"

#include <stddef.h>
#include <stdlib.h>
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

static void
profile_copy_text(char *dst, int dst_size, const char *src)
{
    if(dst == NULL || dst_size <= 0)
        return;
    if(src == NULL)
        src = "";
    snprintf(dst, (size_t)dst_size, "%s", src);
}

static int
profile_setting_name_is(const char *name, const char *a, const char *b,
                        const char *c, const char *d, const char *e)
{
    if(name == NULL)
        return 0;
    return (a != NULL && strcmp(name, a) == 0) ||
           (b != NULL && strcmp(name, b) == 0) ||
           (c != NULL && strcmp(name, c) == 0) ||
           (d != NULL && strcmp(name, d) == 0) ||
           (e != NULL && strcmp(name, e) == 0);
}

static int
profile_setting_in_range(int value, int min_value, int max_value)
{
    return min_value <= max_value && value >= min_value && value <= max_value;
}

static int
profile_parse_decimal(const char *text, int *out)
{
    int value = 0;
    const unsigned char *cursor = (const unsigned char *)text;

    if(text == NULL || text[0] == '\0' || out == NULL)
        return 0;
    while(*cursor != '\0') {
        if(*cursor < '0' || *cursor > '9')
            return 0;
        value = value * 10 + (*cursor - '0');
        if(value > 255)
            return 0;
        cursor++;
    }
    *out = value;
    return 1;
}

int
TerminalPaneColorToRGB(Color color)
{
    if(!color_visible(color))
        return TERMINAL_PANE_COLOR_DEFAULT;
    return TERMINAL_PANE_COLOR_TRUE_RGB | ((int)color.r << 16) |
           ((int)color.g << 8) | (int)color.b;
}

Color
ResolveTerminalPaneColor(const TerminalPanePalette *palette, int value,
                         Color fallback)
{
    if(value >= 0 && (value & TERMINAL_PANE_COLOR_TRUE_RGB) != 0) {
        int rgb = value & 0xffffff;

        return (Color){(unsigned char)((rgb >> 16) & 255),
                       (unsigned char)((rgb >> 8) & 255),
                       (unsigned char)(rgb & 255), 255};
    }
    if(value >= 0 && value < 256) {
        if(palette != NULL)
            return palette->ansi[value];
        return GetTerminalPaneDefaultPalette().ansi[value];
    }
    return fallback;
}

Color
ResolveTerminalPaneColorWithOverrides(const TerminalPanePalette *palette,
                                      const int *overrides, int value,
                                      Color fallback)
{
    if(overrides != NULL && value >= 0 && value < 256 &&
       overrides[value] != TERMINAL_PANE_COLOR_DEFAULT)
        value = overrides[value];
    return ResolveTerminalPaneColor(palette, value, fallback);
}

TerminalPaneViewColors
ResolveTerminalPaneViewColors(const TerminalPanePalette *palette,
                              const int *overrides,
                              TerminalPaneProfileColors colors,
                              TerminalPaneColors fallback)
{
    TerminalPaneViewColors resolved;

    fallback = ResolveTerminalPaneThemeColors(fallback);
    resolved.foreground = ResolveTerminalPaneColorWithOverrides(
        palette, overrides, colors.foreground, fallback.text);
    resolved.background = ResolveTerminalPaneColorWithOverrides(
        palette, overrides, colors.background, fallback.background);
    resolved.cursor = ResolveTerminalPaneColorWithOverrides(
        palette, overrides, colors.cursor, resolved.foreground);
    resolved.selection_foreground = ResolveTerminalPaneColorWithOverrides(
        palette, overrides, colors.selection_foreground, resolved.background);
    resolved.selection_background = ResolveTerminalPaneColorWithOverrides(
        palette, overrides, colors.selection_background, fallback.selection);
    return resolved;
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

TerminalPaneProfileLimits
GetDefaultTerminalPaneProfileLimits(void)
{
    TerminalPaneProfileLimits limits;

    limits.default_font_size = 16;
    limits.min_font_size = 10;
    limits.max_font_size = 48;
    limits.default_scrollback_limit = 5000;
    limits.min_scrollback_limit = 100;
    limits.max_scrollback_limit = 100000;
    limits.default_cursor_style = TERMINAL_PANE_CURSOR_BLOCK;
    return limits;
}

void
InitTerminalPaneProfileSettings(TerminalPaneProfileSettings *settings,
                                TerminalPaneProfileLimits limits)
{
    if(settings == NULL)
        return;
    memset(settings, 0, sizeof(*settings));
    settings->font_size = limits.default_font_size;
    settings->scrollback_limit = limits.default_scrollback_limit;
    settings->cursor_style = limits.default_cursor_style;
    settings->terminal_foreground = TERMINAL_PANE_COLOR_DEFAULT;
    settings->terminal_background = TERMINAL_PANE_COLOR_DEFAULT;
    settings->terminal_cursor = TERMINAL_PANE_COLOR_DEFAULT;
    settings->terminal_selection_foreground = TERMINAL_PANE_COLOR_DEFAULT;
    settings->terminal_selection_background = TERMINAL_PANE_COLOR_DEFAULT;
}

int
ApplyTerminalPaneProfileSetting(TerminalPaneProfileSettings *settings,
                                TerminalPaneProfileLimits limits,
                                const char *name, const char *value)
{
    int parsed;

    if(settings == NULL || name == NULL || value == NULL)
        return 0;
    if(profile_setting_name_is(name, "font_size", "font-size", NULL, NULL,
                               NULL)) {
        parsed = atoi(value);
        if(!profile_setting_in_range(parsed, limits.min_font_size,
                                     limits.max_font_size))
            return 0;
        settings->font_size = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "scrollback", "scrollback_limit",
                               "scrollback-limit", NULL, NULL)) {
        parsed = atoi(value);
        if(!profile_setting_in_range(parsed, limits.min_scrollback_limit,
                                     limits.max_scrollback_limit))
            return 0;
        settings->scrollback_limit = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "cursor_style", "cursor-style", NULL,
                               NULL, NULL)) {
        settings->cursor_style =
            ParseTerminalPaneCursorStyle(value, settings->cursor_style);
        return 1;
    }
    if(strcmp(name, "shell") == 0) {
        profile_copy_text(settings->shell, (int)sizeof(settings->shell),
                          value);
        return 1;
    }
    if(profile_setting_name_is(name, "working_directory",
                               "working-directory", "cwd", NULL, NULL)) {
        profile_copy_text(settings->working_directory,
                          (int)sizeof(settings->working_directory), value);
        return 1;
    }
    if(strcmp(name, "command") == 0) {
        profile_copy_text(settings->command, (int)sizeof(settings->command),
                          value);
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_font", "terminal-font",
                               "font", NULL, NULL)) {
        profile_copy_text(settings->terminal_font,
                          (int)sizeof(settings->terminal_font), value);
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_foreground",
                               "terminal-foreground", "foreground", NULL,
                               NULL)) {
        if(!ParseTerminalPaneProfileColor(value, &parsed))
            return 0;
        settings->terminal_foreground = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_background",
                               "terminal-background", "background", NULL,
                               NULL)) {
        if(!ParseTerminalPaneProfileColor(value, &parsed))
            return 0;
        settings->terminal_background = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_cursor", "terminal-cursor",
                               "cursor", "cursor_color", "cursor-color")) {
        if(!ParseTerminalPaneProfileColor(value, &parsed))
            return 0;
        settings->terminal_cursor = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_selection_foreground",
                               "terminal-selection-foreground",
                               "selection_foreground",
                               "selection-foreground", NULL)) {
        if(!ParseTerminalPaneProfileColor(value, &parsed))
            return 0;
        settings->terminal_selection_foreground = parsed;
        return 1;
    }
    if(profile_setting_name_is(name, "terminal_selection_background",
                               "terminal-selection-background",
                               "selection_background",
                               "selection-background", NULL)) {
        if(!ParseTerminalPaneProfileColor(value, &parsed))
            return 0;
        settings->terminal_selection_background = parsed;
        return 1;
    }
    return 0;
}

const char *
TerminalPaneProfilePromptTitle(int prompt)
{
    switch(prompt) {
    case TERMINAL_PANE_PROFILE_PROMPT_SHELL:
        return "Shell";
    case TERMINAL_PANE_PROFILE_PROMPT_WORKING_DIRECTORY:
        return "Working Directory";
    case TERMINAL_PANE_PROFILE_PROMPT_TERMINAL_FONT:
        return "Terminal Font";
    case TERMINAL_PANE_PROFILE_PROMPT_FONT_SIZE:
        return "Font Size";
    case TERMINAL_PANE_PROFILE_PROMPT_SCROLLBACK:
        return "Scrollback Lines";
    case TERMINAL_PANE_PROFILE_PROMPT_FOREGROUND:
        return "Terminal Foreground";
    case TERMINAL_PANE_PROFILE_PROMPT_BACKGROUND:
        return "Terminal Background";
    case TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR:
        return "Terminal Cursor";
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_FOREGROUND:
        return "Selection Foreground";
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_BACKGROUND:
        return "Selection Background";
    default:
        break;
    }
    return "Profile";
}

const char *
TerminalPaneProfilePromptSettingName(int prompt)
{
    switch(prompt) {
    case TERMINAL_PANE_PROFILE_PROMPT_SHELL:
        return "shell";
    case TERMINAL_PANE_PROFILE_PROMPT_WORKING_DIRECTORY:
        return "working-directory";
    case TERMINAL_PANE_PROFILE_PROMPT_TERMINAL_FONT:
        return "terminal-font";
    case TERMINAL_PANE_PROFILE_PROMPT_FONT_SIZE:
        return "font-size";
    case TERMINAL_PANE_PROFILE_PROMPT_SCROLLBACK:
        return "scrollback";
    case TERMINAL_PANE_PROFILE_PROMPT_FOREGROUND:
        return "terminal-foreground";
    case TERMINAL_PANE_PROFILE_PROMPT_BACKGROUND:
        return "terminal-background";
    case TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR:
        return "terminal-cursor";
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_FOREGROUND:
        return "terminal-selection-foreground";
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_BACKGROUND:
        return "terminal-selection-background";
    default:
        break;
    }
    return NULL;
}

int
TerminalPaneProfilePromptAffectsColors(int prompt)
{
    return prompt == TERMINAL_PANE_PROFILE_PROMPT_FOREGROUND ||
           prompt == TERMINAL_PANE_PROFILE_PROMPT_BACKGROUND ||
           prompt == TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR ||
           prompt == TERMINAL_PANE_PROFILE_PROMPT_SELECTION_FOREGROUND ||
           prompt == TERMINAL_PANE_PROFILE_PROMPT_SELECTION_BACKGROUND;
}

int
TerminalPaneProfilePromptAffectsFont(int prompt)
{
    return prompt == TERMINAL_PANE_PROFILE_PROMPT_TERMINAL_FONT;
}

int
TerminalPaneProfilePromptAffectsScrollback(int prompt)
{
    return prompt == TERMINAL_PANE_PROFILE_PROMPT_SCROLLBACK;
}

static int
format_profile_prompt_color(char *out, int out_size, int color)
{
    if(out == NULL || out_size <= 0)
        return 0;
    if(color == TERMINAL_PANE_COLOR_DEFAULT)
        return snprintf(out, (size_t)out_size, "default");
    if(FormatTerminalPaneProfileColor(out, out_size, color) > 0)
        return (int)strlen(out);
    out[0] = '\0';
    return 0;
}

int
FormatTerminalPaneProfilePromptValue(
    char *out, int out_size, const TerminalPaneProfileSettings *settings,
    int prompt)
{
    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    if(settings == NULL)
        return 0;
    switch(prompt) {
    case TERMINAL_PANE_PROFILE_PROMPT_SHELL:
        return snprintf(out, (size_t)out_size, "%s", settings->shell);
    case TERMINAL_PANE_PROFILE_PROMPT_WORKING_DIRECTORY:
        return snprintf(out, (size_t)out_size, "%s",
                        settings->working_directory);
    case TERMINAL_PANE_PROFILE_PROMPT_TERMINAL_FONT:
        return snprintf(out, (size_t)out_size, "%s",
                        settings->terminal_font);
    case TERMINAL_PANE_PROFILE_PROMPT_FONT_SIZE:
        return snprintf(out, (size_t)out_size, "%d", settings->font_size);
    case TERMINAL_PANE_PROFILE_PROMPT_SCROLLBACK:
        return snprintf(out, (size_t)out_size, "%d",
                        settings->scrollback_limit);
    case TERMINAL_PANE_PROFILE_PROMPT_FOREGROUND:
        return format_profile_prompt_color(out, out_size,
                                           settings->terminal_foreground);
    case TERMINAL_PANE_PROFILE_PROMPT_BACKGROUND:
        return format_profile_prompt_color(out, out_size,
                                           settings->terminal_background);
    case TERMINAL_PANE_PROFILE_PROMPT_CURSOR_COLOR:
        return format_profile_prompt_color(out, out_size,
                                           settings->terminal_cursor);
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_FOREGROUND:
        return format_profile_prompt_color(
            out, out_size, settings->terminal_selection_foreground);
    case TERMINAL_PANE_PROFILE_PROMPT_SELECTION_BACKGROUND:
        return format_profile_prompt_color(
            out, out_size, settings->terminal_selection_background);
    default:
        break;
    }
    return 0;
}

int
ApplyTerminalPaneProfilePromptValue(TerminalPaneProfileSettings *settings,
                                    TerminalPaneProfileLimits limits,
                                    int prompt, const char *value)
{
    const char *name = TerminalPaneProfilePromptSettingName(prompt);

    if(name == NULL)
        return 0;
    return ApplyTerminalPaneProfileSetting(settings, limits, name, value);
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
TerminalPaneCursorStyleReportCode(int style, int blink)
{
    if(style == TERMINAL_PANE_CURSOR_BLOCK)
        return blink ? 1 : 2;
    if(style == TERMINAL_PANE_CURSOR_UNDERLINE)
        return blink ? 3 : 4;
    if(style == TERMINAL_PANE_CURSOR_BAR)
        return blink ? 5 : 6;
    return 0;
}

int
DecodeTerminalPaneCursorStyleRequest(int code, int *style, int *blink)
{
    int decoded_style;
    int decoded_blink;

    if(code <= 0) {
        decoded_style = TERMINAL_PANE_CURSOR_DEFAULT;
        decoded_blink = 1;
    } else if(code == 1) {
        decoded_style = TERMINAL_PANE_CURSOR_BLOCK;
        decoded_blink = 1;
    } else if(code == 2) {
        decoded_style = TERMINAL_PANE_CURSOR_BLOCK;
        decoded_blink = 0;
    } else if(code == 3 || code == 4) {
        decoded_style = TERMINAL_PANE_CURSOR_UNDERLINE;
        decoded_blink = code == 3 ? 1 : 0;
    } else if(code == 5 || code == 6) {
        decoded_style = TERMINAL_PANE_CURSOR_BAR;
        decoded_blink = code == 5 ? 1 : 0;
    } else {
        return 0;
    }
    if(style != NULL)
        *style = decoded_style;
    if(blink != NULL)
        *blink = decoded_blink;
    return 1;
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
    if(profile_parse_decimal(text, out))
        return 1;
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
    if(color >= 0 && color < 256)
        return snprintf(out, (size_t)out_size, "%d", color);
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

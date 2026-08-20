#include "terminal_pane.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *
terminal_pane_title_fallback(const char *fallback)
{
    return fallback != NULL && fallback[0] != '\0' ? fallback : "terminal";
}

static const char *
terminal_pane_path_basename(const char *path)
{
    const char *end;
    const char *slash;

    if(path == NULL || path[0] == '\0')
        return "";
    end = path + strlen(path);
    while(end > path && end[-1] == '/')
        end--;
    if(end == path)
        return "/";
    slash = end;
    while(slash > path && slash[-1] != '/')
        slash--;
    return slash;
}

static const char *
terminal_pane_pathish_title_start(const char *text)
{
    const char *colon;

    if(text == NULL)
        return "";
    colon = strrchr(text, ':');
    if(colon != NULL && (colon[1] == '/' || colon[1] == '~'))
        return colon + 1;
    return text;
}

int
FormatTerminalPaneSessionTitle(char *out, int out_size, const char *text,
                               const char *fallback)
{
    const char *title;
    int len;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    title = terminal_pane_path_basename(terminal_pane_pathish_title_start(text));
    len = (int)strlen(title);
    while(len > 1 && title[len - 1] == '/')
        len--;
    if(len <= 0) {
        title = terminal_pane_title_fallback(fallback);
        len = (int)strlen(title);
    }
    if(len >= out_size)
        len = out_size - 1;
    if(len > 0)
        memcpy(out, title, (size_t)len);
    out[len] = '\0';
    return len;
}

static int
terminal_pane_session_copy_field(char *out, int out_size, const char *text)
{
    if(out == NULL || out_size <= 0)
        return 0;
    return UnescapeTerminalPaneText(out, out_size, text != NULL ? text : "");
}

int
FormatTerminalPaneSessionRecord(char *out, int out_size,
                                TerminalPaneSessionRecord record)
{
    char cwd[4096];
    char shell[2048];
    char title[1024];
    char command[4096];
    int len;

    if(out == NULL || out_size <= 0)
        return 0;
    out[0] = '\0';
    EscapeTerminalPaneText(cwd, (int)sizeof(cwd), record.cwd);
    EscapeTerminalPaneText(shell, (int)sizeof(shell), record.shell);
    EscapeTerminalPaneText(title, (int)sizeof(title), record.title);
    EscapeTerminalPaneText(command, (int)sizeof(command), record.command);
    len = snprintf(out, (size_t)out_size, "tab\t%s\t%s\t%s\t%d\t%s\t%d",
                   cwd, shell, title, record.title_override ? 1 : 0, command,
                   record.scroll_offset);
    if(len < 0 || len >= out_size) {
        out[0] = '\0';
        return 0;
    }
    return len;
}

int
ParseTerminalPaneSessionActive(const char *line, int *active)
{
    if(line == NULL || strncmp(line, "active=", 7) != 0)
        return 0;
    if(active != NULL)
        *active = atoi(line + 7);
    return 1;
}

int
ParseTerminalPaneSessionRecord(const char *line, TerminalPaneSessionRecord *out)
{
    char scratch[8192];
    char *fields[6] = {0};
    char *cursor;
    int count = 0;
    size_t len;

    if(line == NULL || out == NULL || strncmp(line, "tab\t", 4) != 0)
        return 0;
    len = strlen(line);
    if(len >= sizeof(scratch))
        return 0;
    memcpy(scratch, line, len + 1);
    while(len > 0 && (scratch[len - 1] == '\n' || scratch[len - 1] == '\r'))
        scratch[--len] = '\0';
    cursor = scratch + 4;
    while(count < 6) {
        char *next = strchr(cursor, '\t');

        fields[count++] = cursor;
        if(next == NULL)
            break;
        *next = '\0';
        cursor = next + 1;
    }
    if(count < 3)
        return 0;
    memset(out, 0, sizeof(*out));
    terminal_pane_session_copy_field(out->cwd, (int)sizeof(out->cwd),
                                     fields[0]);
    terminal_pane_session_copy_field(out->shell, (int)sizeof(out->shell),
                                     fields[1]);
    terminal_pane_session_copy_field(out->title, (int)sizeof(out->title),
                                     fields[2]);
    if(count >= 4 && fields[3] != NULL)
        out->title_override = atoi(fields[3]) != 0;
    if(count >= 5 && fields[4] != NULL)
        terminal_pane_session_copy_field(out->command,
                                         (int)sizeof(out->command),
                                         fields[4]);
    if(count >= 6 && fields[5] != NULL)
        out->scroll_offset = atoi(fields[5]);
    return 1;
}

#include "terminal_pane.h"

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

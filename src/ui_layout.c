#include "ui_layout.h"
#include "ui_scaling.h"
#include <stddef.h>

static int g_view_width = 320;
static int g_view_height = 560;

void
SetUIViewSize(int width, int height)
{
    g_view_width = width;
    g_view_height = height;
}

int
GetUIViewWidth(void)
{
    return g_view_width;
}

int
GetUIViewHeight(void)
{
    return g_view_height;
}

void
GetUICenteredColumn(int max_w, int side_pad, int *x, int *w)
{
    int available_w = g_view_width - side_pad * 2;

    if(available_w < 0)
        available_w = 0;
    if(max_w > available_w)
        max_w = available_w;
    if(max_w < 0)
        max_w = 0;

    if(x != NULL)
        *x = (g_view_width - max_w) / 2;
    if(w != NULL)
        *w = max_w;
}

int
GetUIPageSidePadding(void)
{
    int padding = g_view_width / 50;

    if(padding < 12)
        padding = 12;
    if(padding > 24)
        padding = 24;
    return padding;
}

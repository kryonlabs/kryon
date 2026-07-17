#include "ui_layout.h"
#include "ui_core.h"
#include "ui_scaling.h"
#include <stddef.h>

void
SetUIViewSize(int width, int height)
{
    ui_view_width = width;
    ui_view_height = height;
}

int
GetUIViewWidth(void)
{
    return ui_view_width;
}

int
GetUIViewHeight(void)
{
    return ui_view_height;
}

void
GetUICenteredColumn(int max_w, int side_pad, int *x, int *w)
{
    int available_w = ui_view_width - side_pad * 2;

    if(available_w < 0)
        available_w = 0;
    if(max_w > available_w)
        max_w = available_w;
    if(max_w < 0)
        max_w = 0;

    if(x != NULL)
        *x = (ui_view_width - max_w) / 2;
    if(w != NULL)
        *w = max_w;
}

int
GetUIPageSidePadding(void)
{
    int padding = ui_view_width / 50;

    if(padding < 12)
        padding = 12;
    if(padding > 24)
        padding = 24;
    return padding;
}

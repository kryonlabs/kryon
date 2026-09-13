#include "ui_layout.h"
#include "ui_core.h"
#include "ui_scaling.h"
#include "runtime/layout.h"
#include <stddef.h>

void
SetViewSize(int width, int height)
{
    ui_view_width = width;
    ui_view_height = height;
}

int
GetViewWidth(void)
{
    return ui_view_width;
}

int
GetViewHeight(void)
{
    return ui_view_height;
}

void
GetCenteredColumn(int max_w, int side_pad, int *x, int *w)
{
    CenteredColumnLayout layout =
        CenteredColumnFor(ui_view_width, max_w, side_pad);

    if(x != NULL)
        *x = layout.x;
    if(w != NULL)
        *w = layout.width;
}

int
GetPageSidePadding(void)
{
    return PageSidePaddingFor(ui_view_width);
}

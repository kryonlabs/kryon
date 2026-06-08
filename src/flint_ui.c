#include "flint_ui.h"

#include "flint_layout.h"
#include "flint_scaling.h"

void
flint_ui_init(int width, int height, float dpi)
{
    flint_set_dpi_scale(dpi);
    flint_set_view_size(width, height);
}

void
flint_ui_draw_bevel(int x, int y, int w, int h, Color light, Color dark)
{
    int border = (int)(flint_get_dpi_scale() + 0.5f);
    if(border < 1)
        border = 1;

    DrawRectangle(x, y, w, border, light);
    DrawRectangle(x, y, border, h, light);
    DrawRectangle(x + w - border, y, border, h, dark);
    DrawRectangle(x, y + h - border, w, border, dark);
}

void
flint_ui_draw_text_lines(const char **lines, int count, int x, int *y, int font, int line_h, Color color)
{
    for(int i = 0; i < count; i++) {
        DrawText(lines[i], x, *y, font, color);
        *y += line_h;
    }
}

int
flint_ui_icon_btn_size(int size)
{
    switch(size) {
    case 0:
        return flint_clamp_px(18, 16, 40);
    case 1:
        return flint_clamp_px(22, 20, 36);
    case 2:
        return flint_clamp_px(26, 24, 40);
    case 3:
        return flint_clamp_px(30, 28, 44);
    default:
        return flint_clamp_px(22, 20, 36);
    }
}

int
flint_ui_icon_btn_padding(int size)
{
    switch(size) {
    case 0:
        return flint_px(8);
    case 1:
        return flint_px(10);
    case 2:
        return flint_px(12);
    case 3:
        return flint_px(14);
    default:
        return flint_px(10);
    }
}

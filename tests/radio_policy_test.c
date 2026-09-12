#include <assert.h>
#include <math.h>
#include <string.h>

#include "runtime/radio.h"

static void
check_rect(Rectangle got, float x, float y, float width, float height)
{
    assert(fabsf(got.x - x) < 0.001f);
    assert(fabsf(got.y - y) < 0.001f);
    assert(fabsf(got.width - width) < 0.001f);
    assert(fabsf(got.height - height) < 0.001f);
}

int
main(void)
{
    RadioPaint paint;

    assert(RadioSize(2.0f) == 40);
    assert(RadioTouchSize(2.0f) == 80);
    assert(RadioSize(0.0f) == 20);
    assert(strcmp(RadioMarkText(false), "○") == 0);
    assert(strcmp(RadioMarkText(true), "◉") == 0);

    paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 160, 30},
        .scale = 1.0f,
        .text_color = 0x111111ff,
        .icon_color = 0x222222ff,
        .button_color = 0x333333ff,
        .primary_color = 0x444444ff,
        .surface_variant_color = 0x555555ff,
        .disabled_color = 0x666666ff,
    });
    check_rect(paint.mark_bounds, 10, 25, 20, 20);
    check_rect(paint.label_bounds, 38, 20, 132, 30);
    assert(paint.ring_color == 0x222222ff);
    assert(paint.fill_radius == 0.0f);

    paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 160, 30},
        .checked = true,
        .disabled = true,
        .default_style = true,
        .scale = 1.0f,
        .text_color = 0x111111ff,
        .icon_color = 0x222222ff,
        .button_color = 0x333333ff,
        .primary_color = 0x444444ff,
        .surface_variant_color = 0x555555ff,
        .disabled_color = 0x666666ff,
    });
    check_rect(paint.mark_bounds, 20, 25, 20, 20);
    check_rect(paint.label_bounds, 54, 20, 116, 30);
    assert(paint.ring_color == 0x666666ff);
    assert(paint.fill_color == 0x666666ff);
    return 0;
}

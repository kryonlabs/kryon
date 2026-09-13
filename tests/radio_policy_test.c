#include <assert.h>
#include <math.h>
#include <string.h>

#include "runtime/radio.h"

static StyleFrame
test_radio_frame(uint32_t background, uint32_t foreground, uint32_t border)
{
    StyleFrame frame = {0};
    frame.value.fields = StyleBackground | StyleForeground | StyleBorder;
    frame.value.background = background;
    frame.value.foreground = foreground;
    frame.value.border = border;
    frame.value.border_width = 2.0f;
    return frame;
}

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
    StyleFrame styled_ring;
    StyleFrame zero_ring;

    assert(RadioSizeForStyle((StyleFrame){0}, 2.0f) == 40);
    assert(RadioTouchSizeForStyle((StyleFrame){0}, 2.0f) == 80);
    assert(strcmp(RadioMarkText(false), "○") == 0);
    assert(strcmp(RadioMarkText(true), "◉") == 0);

    paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 160, 30},
        .scale = 1.0f,
        .frame = test_radio_frame(0x111111ff, 0x111111ff, 0x222222ff),
        .selected = test_radio_frame(0x444444ff, 0x111111ff, 0x444444ff),
    });
    check_rect(paint.mark_bounds, 20, 25, 20, 20);
    check_rect(paint.label_bounds, 58, 20, 112, 30);
    assert(paint.ring_color == 0x222222ff);
    assert(paint.fill_radius == 0.0f);

    styled_ring = test_radio_frame(0x111111ff, 0x111111ff, 0x222222ff);
    styled_ring.value.fields |= StyleIconSize | StylePaddingX | StyleGap;
    styled_ring.value.icon_size = 12.0f;
    styled_ring.value.padding_x = 30.0f;
    styled_ring.value.gap = 3.0f;
    paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 160, 30},
        .scale = 1.0f,
        .frame = styled_ring,
        .selected = test_radio_frame(0x444444ff, 0x111111ff, 0x444444ff),
    });
    assert(RadioSizeForStyle(styled_ring, 1.0f) == 12);
    assert(RadioTouchSizeForStyle(styled_ring, 1.0f) == 30);
    check_rect(paint.mark_bounds, 19, 29, 12, 12);
    check_rect(paint.label_bounds, 43, 20, 127, 30);

    zero_ring = styled_ring;
    zero_ring.value.icon_size = 0.0f;
    zero_ring.value.padding_x = 0.0f;
    assert(RadioSizeForStyle(zero_ring, 1.0f) == 0);
    assert(RadioTouchSizeForStyle(zero_ring, 1.0f) == 0);

    paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 160, 30},
        .checked = true,
        .disabled = true,
        .scale = 1.0f,
        .frame = (StyleFrame){.value = {
            .fields = StyleBackground | StyleForeground | StyleBorder |
                      StyleBorderWidth | StylePaddingX | StyleGap,
            .background = 0x333333ff,
            .foreground = 0x666666ff,
            .border = 0x666666ff,
            .border_width = 2.0f,
            .padding_x = 40.0f,
            .gap = 4.0f,
        }},
        .selected = test_radio_frame(0x666666ff, 0x666666ff, 0x666666ff),
    });
    check_rect(paint.mark_bounds, 20, 25, 20, 20);
    check_rect(paint.label_bounds, 54, 20, 116, 30);
    assert(paint.ring_color == 0x666666ff);
    assert(paint.fill_color == 0x666666ff);
    return 0;
}

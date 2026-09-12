#include <assert.h>
#include <math.h>

#include "runtime/modal.h"

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
    ModalMetrics metrics = ModalMetricsFor(2.0f);
    ModalLayout layout;

    assert(metrics.screen_pad == 48);
    assert(metrics.min_width == 560);
    assert(metrics.default_max_width == 840);
    assert(metrics.edge_pad == 16);
    assert(metrics.title_height == 96);
    assert(metrics.padding_x == 36);
    assert(metrics.padding_bottom == 36);
    assert(metrics.message_gap == 36);
    assert(metrics.prompt_height == 76);
    assert(metrics.prompt_gap == 36);
    assert(metrics.button_height == 88);
    assert(metrics.button_gap == 16);
    assert(metrics.action_padding_x == 48);
    assert(metrics.action_min_width == 176);
    assert(metrics.action_max_width == 300);
    assert(metrics.content_min_width == 240);
    assert(metrics.min_height == 320);

    assert(ModalClampWidth(900, 0, metrics) == 840);
    assert(ModalClampWidth(500, 0, metrics) == 484);
    assert(ModalContentWidth(260, metrics) == 240);
    assert(ModalContentWidth(840, metrics) == 768);
    assert(ModalActionWidth(10, metrics) == 176);
    assert(ModalActionWidth(280, metrics) == 300);
    assert(ModalActionRowsStep(176, 1, 176, 320, 16) == 2);
    assert(ModalActionRowWidthStep(176, 176, 320, 16) == 176);
    assert(ModalActionRowsStep(176, 1, 100, 320, 16) == 1);
    assert(ModalActionRowWidthStep(176, 100, 320, 16) == 292);
    assert(ModalButtonsHeight(0, metrics) == 0);
    assert(ModalButtonsHeight(3, metrics) == 296);

    metrics = ModalMetricsFor(1.0f);
    layout = ModalLayoutFor(640, 480, 0, 24, 1, true, metrics);
    check_rect(layout.panel, 110, 136, 420, 208);
    assert(layout.content_width == 384);
    assert(layout.message_x == 128);
    assert(layout.message_y == 184);
    assert(layout.button_y == 282);
    assert(layout.prompt_y == 226);
    assert(layout.prompt_height == 38);
    assert(layout.buttons_height == 44);
    return 0;
}

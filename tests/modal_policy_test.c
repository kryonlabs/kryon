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

static ModalMetrics
test_modal_metrics(float scale, StyleFrame panel, StyleFrame title,
                   StyleFrame message, StyleFrame action, StyleFrame close)
{
    return ModalMetricsFor(scale, panel, title, message, action, close);
}

int
main(void)
{
    StyleFrame panel = {0};
    StyleFrame title = {0};
    StyleFrame message = {0};
    StyleFrame action = {0};
    StyleFrame close = {0};
    ModalMetrics metrics;
    ModalLayout layout;
    ModalFrameLayout frame_layout;

    panel.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                         StyleIconSize | StyleContentOffset;
    panel.value.padding_x = 18.0f;
    panel.value.padding_y = 8.0f;
    panel.value.gap = 24.0f;
    panel.value.icon_size = 280.0f;
    panel.value.offset_x = 420.0f;
    panel.value.offset_y = 58.0f;
    title.value.fields = StylePaddingX | StylePaddingY | StyleIconSize |
                         StyleContentOffset;
    title.value.padding_x = 24.0f;
    title.value.padding_y = 18.0f;
    title.value.icon_size = 48.0f;
    title.value.offset_y = 14.0f;
    message.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                           StyleIconSize | StyleContentOffset;
    message.value.padding_x = 120.0f;
    message.value.padding_y = 160.0f;
    message.value.gap = 18.0f;
    message.value.icon_size = 38.0f;
    message.value.offset_y = 18.0f;
    action.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                          StyleContentOffset;
    action.value.padding_x = 24.0f;
    action.value.gap = 8.0f;
    action.value.icon_size = 44.0f;
    action.value.offset_x = 88.0f;
    action.value.offset_y = 150.0f;
    close.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                         StyleIconSize | StyleContentOffset;
    close.value.padding_x = 8.0f;
    close.value.padding_y = 16.0f;
    close.value.gap = 6.0f;
    close.value.icon_size = 20.0f;
    close.value.offset_x = 120.0f;
    close.value.offset_y = 96.0f;

    metrics = test_modal_metrics(2.0f, panel, title, message, action, close);

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
    assert(metrics.frame_min_width == 240);
    assert(metrics.frame_min_height == 192);
    assert(metrics.frame_title_y == 28);
    assert(metrics.frame_content_y == 116);
    assert(metrics.frame_content_bottom_pad == 32);
    assert(metrics.frame_icon_size == 40);
    assert(metrics.frame_icon_padding == 16);
    assert(metrics.frame_icon_edge_gap == 12);
    assert(metrics.frame_title_side_padding == 48);

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

    metrics = test_modal_metrics(1.0f, panel, title, message, action, close);
    layout = ModalLayoutFor(640, 480, 0, 24, 1, true, metrics);
    check_rect(layout.panel, 110, 136, 420, 208);
    assert(layout.content_width == 384);
    assert(layout.message_x == 128);
    assert(layout.message_y == 184);
    assert(layout.button_y == 282);
    assert(layout.prompt_y == 226);
    assert(layout.prompt_height == 38);
    assert(layout.buttons_height == 44);

    check_rect(ModalFramePanelFor(640, 480, 900, 900, metrics),
               12, 12, 616, 456);
    frame_layout = ModalFrameLayoutFor((Rectangle){100, 80, 80, 70},
                                       metrics);
    check_rect(frame_layout.panel, 100, 80, 120, 96);
    check_rect(frame_layout.content, 118, 138, 84, 22);
    check_rect(frame_layout.left_button, 106, 86, 36, 36);
    check_rect(frame_layout.right_button, 178, 86, 36, 36);
    assert(frame_layout.title_y == 94);
    assert(frame_layout.title_max_width == 24);
    assert(frame_layout.icon_size == 20);
    assert(frame_layout.icon_padding == 8);

    panel.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                         StyleIconSize | StyleContentOffset;
    panel.value.padding_x = 20.0f;
    panel.value.padding_y = 10.0f;
    panel.value.gap = 30.0f;
    panel.value.icon_size = 300.0f;
    panel.value.offset_x = 460.0f;
    panel.value.offset_y = 60.0f;
    title.value.fields = StylePaddingX | StylePaddingY | StyleIconSize |
                         StyleContentOffset;
    title.value.padding_x = 26.0f;
    title.value.padding_y = 19.0f;
    title.value.icon_size = 52.0f;
    title.value.offset_y = 12.0f;
    message.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                           StyleIconSize | StyleContentOffset;
    message.value.padding_x = 140.0f;
    message.value.padding_y = 180.0f;
    message.value.gap = 20.0f;
    message.value.icon_size = 40.0f;
    message.value.offset_y = 22.0f;
    action.value.fields = StylePaddingX | StyleGap | StyleIconSize |
                          StyleContentOffset;
    action.value.padding_x = 28.0f;
    action.value.gap = 10.0f;
    action.value.icon_size = 48.0f;
    action.value.offset_x = 96.0f;
    action.value.offset_y = 170.0f;
    close.value.fields = StylePaddingX | StylePaddingY | StyleGap |
                         StyleIconSize | StyleContentOffset;
    close.value.padding_x = 9.0f;
    close.value.padding_y = 17.0f;
    close.value.gap = 7.0f;
    close.value.icon_size = 21.0f;
    close.value.offset_x = 130.0f;
    close.value.offset_y = 100.0f;

    metrics = test_modal_metrics(1.0f, panel, title, message, action, close);
    assert(metrics.screen_pad == 30);
    assert(metrics.min_width == 300);
    assert(metrics.default_max_width == 460);
    assert(metrics.edge_pad == 10);
    assert(metrics.title_height == 52);
    assert(metrics.padding_x == 20);
    assert(metrics.padding_bottom == 19);
    assert(metrics.message_gap == 20);
    assert(metrics.prompt_height == 40);
    assert(metrics.prompt_gap == 22);
    assert(metrics.button_height == 48);
    assert(metrics.button_gap == 10);
    assert(metrics.action_padding_x == 28);
    assert(metrics.action_min_width == 96);
    assert(metrics.action_max_width == 170);
    assert(metrics.content_min_width == 140);
    assert(metrics.min_height == 180);
    assert(metrics.frame_min_width == 130);
    assert(metrics.frame_min_height == 100);
    assert(metrics.frame_title_y == 12);
    assert(metrics.frame_content_y == 60);
    assert(metrics.frame_content_bottom_pad == 17);
    assert(metrics.frame_icon_size == 21);
    assert(metrics.frame_icon_padding == 9);
    assert(metrics.frame_icon_edge_gap == 7);
    assert(metrics.frame_title_side_padding == 26);

    panel.value.gap = 0.0f;
    title.value.icon_size = 0.0f;
    action.value.icon_size = 0.0f;
    close.value.icon_size = 0.0f;
    metrics = test_modal_metrics(1.0f, panel, title, message, action, close);
    assert(metrics.screen_pad == 0);
    assert(metrics.title_height == 0);
    assert(metrics.button_height == 44);
    assert(metrics.frame_icon_size == 0);
    return 0;
}

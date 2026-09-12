#include "runtime/button.h"
#include "runtime/checkbox.h"
#include "runtime/radio.h"
#include "runtime/slider.h"
#include "runtime/toggle.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

static StyleFrame
test_style_frame(uint32_t background, uint32_t foreground, uint32_t border)
{
    StyleFrame frame = {0};
    frame.value.fields = StyleBackground | StyleForeground | StyleBorder;
    frame.value.background = background;
    frame.value.foreground = foreground;
    frame.value.border = border;
    frame.value.focus = border;
    frame.value.opacity = 1.0f;
    return frame;
}

static uint32_t
opacity_color(uint32_t color, float opacity)
{
    uint32_t alpha = (uint32_t)((float)(color & 255u) * opacity);
    return (color & 0xffffff00u) | alpha;
}

static void
check_float(const char *name, float got, float want)
{
    float diff = got - want;
    if(diff < 0.0f)
        diff = -diff;
    if(diff <= 0.001f)
        return;
    fprintf(stderr, "%s: got %.3f want %.3f\n", name, got, want);
    exit(1);
}

int
main(void)
{
    StyleFrame frame = test_style_frame(0x00000000, 0x111111ff,
                                        0x222222ff);
    CheckboxSpec checkbox = {
        .bounds = {10, 20, 160, 30},
        .checked = 1,
        .enabled = 1,
        .focused = 1,
        .scale = 1.0f,
        .box = frame,
        .active = frame
    };
    SwatchSpec swatch = {
        .bounds = {10, 20, 80, 30},
        .color = {20, 40, 60, 128},
        .focused = 1,
        .scale = 1.0f,
        .face = frame
    };
    CheckboxPaint checkbox_paint;
    SwatchPaint swatch_paint;
    RadioPaint radio_paint;
    StyleFrame thumb = test_style_frame(0x334455ff, 0xaabbccff,
                                        0x667788ff);
    SliderPaint slider_paint;
    TogglePaint toggle_paint;

    checkbox.box.value.border_width = 0.0f;
    checkbox.active.value.border_width = 0.0f;
    swatch.face.value.border_width = 0.0f;
    swatch.face.value.fields |= StyleBackgroundEnd;
    swatch.face.value.background_end = 0x445566ff;

    checkbox_paint = CheckboxPaintFor(checkbox);
    swatch_paint = SwatchPaintFor(swatch);
    radio_paint = RadioPaintFor((RadioSpec){
        .bounds = {10, 20, 140, 28},
        .checked = 1,
        .scale = 1.0f,
        .frame = frame,
        .selected = frame
    });
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 160, 30},
        .ratio = 0.5f,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 80, 34},
        .checked = 1,
        .enabled = 1,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });

    check_float("checkbox keeps zero border width",
                checkbox_paint.border_width, 0.0f);
    check_float("swatch keeps zero border width",
                swatch_paint.border_width, 0.0f);
    check_float("swatch keeps zero focus width",
                swatch_paint.focus_width, 0.0f);
    if(swatch_paint.checker_base_color != swatch.face.value.background) {
        fprintf(stderr, "swatch checker base did not come from style\n");
        return 1;
    }
    if(swatch_paint.checker_alt_color != swatch.face.value.background_end) {
        fprintf(stderr, "swatch checker alt did not come from style\n");
        return 1;
    }
    check_float("radio keeps zero stroke width",
                radio_paint.stroke_width, 0.0f);
    if(slider_paint.thumb_fill_color != thumb.value.background) {
        fprintf(stderr, "slider thumb fill did not come from thumb style\n");
        return 1;
    }
    if(slider_paint.thumb_edge_color != thumb.value.border) {
        fprintf(stderr, "slider thumb edge did not come from thumb style\n");
        return 1;
    }
    if(toggle_paint.thumb_fill_color != thumb.value.background) {
        fprintf(stderr, "toggle thumb fill did not come from thumb style\n");
        return 1;
    }
    if(toggle_paint.thumb_edge_color != thumb.value.border) {
        fprintf(stderr, "toggle thumb edge did not come from thumb style\n");
        return 1;
    }

    thumb.value.opacity = 0.25f;
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 160, 30},
        .ratio = 0.5f,
        .disabled = 1,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    if(slider_paint.thumb_fill_color !=
       opacity_color(thumb.value.background, 0.25f)) {
        fprintf(stderr, "slider disabled thumb fill ignored style opacity\n");
        return 1;
    }
    if(slider_paint.thumb_edge_color !=
       opacity_color(thumb.value.border, 0.25f)) {
        fprintf(stderr, "slider disabled thumb edge ignored style opacity\n");
        return 1;
    }

    frame.value.opacity = 0.30f;
    thumb.value.opacity = 0.20f;
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 120, 34},
        .checked = 1,
        .enabled = 0,
        .has_labels = 1,
        .off_width = 22,
        .on_width = 18,
        .font = 16,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    if(toggle_paint.off_label_color !=
       opacity_color(frame.value.foreground, 0.30f)) {
        fprintf(stderr, "toggle disabled off label ignored track opacity\n");
        return 1;
    }
    if(toggle_paint.on_label_color !=
       opacity_color(frame.value.foreground, 0.30f)) {
        fprintf(stderr, "toggle disabled on label ignored active opacity\n");
        return 1;
    }
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 80, 34},
        .checked = 1,
        .enabled = 0,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    if(toggle_paint.thumb_fill_color !=
       opacity_color(thumb.value.background, 0.20f)) {
        fprintf(stderr, "toggle disabled thumb fill ignored style opacity\n");
        return 1;
    }
    if(toggle_paint.thumb_edge_color !=
       opacity_color(thumb.value.border, 0.20f)) {
        fprintf(stderr, "toggle disabled thumb edge ignored style opacity\n");
        return 1;
    }

    return 0;
}

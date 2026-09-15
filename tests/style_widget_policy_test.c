#include "runtime/button.h"
#include "runtime/checkbox.h"
#include "runtime/collapsible.h"
#include "runtime/drag.h"
#include "runtime/dropdown.h"
#include "runtime/fieldset.h"
#include "runtime/focus.h"
#include "runtime/image.h"
#include "runtime/menu.h"
#include "runtime/modal.h"
#include "runtime/navigation_bar.h"
#include "runtime/paned_view.h"
#include "runtime/plot.h"
#include "runtime/progress.h"
#include "runtime/radio.h"
#include "runtime/separator.h"
#include "runtime/slider.h"
#include "runtime/table_view.h"
#include "runtime/toggle.h"
#include "runtime/toast.h"

#include "../src/ui/ui_style_internal.h"

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
    StyleData opacity_style = {.fields = StyleBackground, .background = 0x202631ff};
    Style unpacked = ui_unpack_style(opacity_style);
    if(unpacked.opacity != 1.0f || (unpacked.fields & StyleOpacity) != 0) {
        fprintf(stderr, "omitted surface opacity must be visible without declaring an override\n");
        return 1;
    }
    opacity_style.fields |= StyleOpacity;
    unpacked = ui_unpack_style(opacity_style);
    if(unpacked.opacity != 0.0f) {
        fprintf(stderr, "explicit zero surface opacity must remain transparent\n");
        return 1;
    }
    opacity_style.opacity = 0.34f;
    unpacked = ui_unpack_style(opacity_style);
    if(unpacked.opacity != 0.34f) {
        fprintf(stderr, "explicit scrim opacity must be preserved\n");
        return 1;
    }

    StyleFrame frame = test_style_frame(0x00000000, 0x111111ff,
                                        0x222222ff);
    CheckboxSpec checkbox = {
        .bounds = {10, 20, 160, 30},
        .checked = 1,
        .enabled = 1,
        .focused = 1,
        .scale = 1.0f,
        .box = frame,
        .active = frame,
        .label = frame
    };
    SwatchSpec swatch = {
        .bounds = {10, 20, 80, 30},
        .color = {20, 40, 60, 128},
        .focused = 1,
        .scale = 1.0f,
        .face = frame
    };
    CheckboxPaint checkbox_paint;
    FieldsetPaint fieldset_paint;
    PlotTextPaint plot_text_paint;
    BulletPaint bullet_paint;
    SwatchPaint swatch_paint;
    RadioPaint radio_paint;
    ProgressPaint progress_paint;
    StyleFrame thumb = test_style_frame(0x334455ff, 0xaabbccff,
                                        0x667788ff);
    DragPointerDecision drag_pointer;
    SliderPaint slider_paint;
    SliderPointerDecision slider_pointer;
    SliderRatioDecision slider_ratio;
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

    if(SeparatorLineRole() != 7 || SeparatorLabelRole() != 6 ||
       SeparatorBulletRole() != 8) {
        fprintf(stderr, "separator role policy changed\n");
        return 1;
    }
    if(ProgressTrackRole() != 4 || ProgressFillRole() != 5 ||
       ProgressLabelRole() != 6) {
        fprintf(stderr, "progress role policy changed\n");
        return 1;
    }
    if(PanedViewHandleRole() != 12 || ToastLabelRole() != 6 ||
       ImageLabelRole() != 6 || FocusBoxRole() != 9 ||
       FocusLabelRole() != 6) {
        fprintf(stderr, "small widget role policy changed\n");
        return 1;
    }
    if(CollapsibleHeaderRole() != 13 ||
       CollapsibleTreeHeaderRole() != 14 ||
       CollapsibleCloseRole() != 15 ||
       CollapsibleHeaderRoleFor(0) != 13 ||
       CollapsibleHeaderRoleFor(1) != 14) {
        fprintf(stderr, "collapsible role policy changed\n");
        return 1;
    }
    if(MenuBarRole() != 1 || MenuPopupRole() != 2 ||
       MenuContextRole() != 3) {
        fprintf(stderr, "menu role policy changed\n");
        return 1;
    }
    if(DropdownPanelRole() != 2 || DropdownOptionRole() != 26 ||
       DropdownScrollbarRole() != 27) {
        fprintf(stderr, "dropdown role policy changed\n");
        return 1;
    }
    if(ModalPanelRole() != 2 || ModalTitleRole() != 16 ||
       ModalActionRole() != 17 || ModalScrimRole() != 19 ||
       ModalMessageRole() != 20 || ModalCloseRole() != 15) {
        fprintf(stderr, "modal role policy changed\n");
        return 1;
    }
    if(TableViewPanelRole() != 2 || TableViewHeaderRole() != 13 ||
       TableViewDividerRole() != 18 || TableViewRowRole() != 21 ||
       TableViewCellRole() != 22 || TableViewSelectionRole() != 23) {
        fprintf(stderr, "table view role policy changed\n");
        return 1;
    }
    if(NavigationBarPanelRole() != 2 || NavigationBarRowRole() != 21 ||
       NavigationBarActionRole() != 17 || NavigationBarRouteRole() != 18) {
        fprintf(stderr, "navigation bar role policy changed\n");
        return 1;
    }
    if(RadioRingRole() != 11 || RadioMarkRole() != 10 ||
       RadioLabelRole() != 6) {
        fprintf(stderr, "radio role policy changed\n");
        return 1;
    }

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
    frame.value.padding_x = 12.0f;
    frame.value.padding_y = 5.0f;
    frame.value.gap = 7.0f;
    frame.value.font_size = 14.0f;
    fieldset_paint = FieldsetPaintFor((Rectangle){20, 30, 120, 60}, 40.0f,
        1, 1.0f, frame);
    check_float("fieldset padding falls back when unset",
                fieldset_paint.title_background.x, 28.0f);
    check_float("fieldset title height falls back when unset",
                fieldset_paint.title_text.height, 18.0f);
    frame.value.fields |= StylePaddingX | StylePaddingY | StyleGap |
                          StyleFontSize;
    fieldset_paint = FieldsetPaintFor((Rectangle){20, 30, 120, 60}, 40.0f,
        1, 1.0f, frame);
    check_float("fieldset title background padding comes from style",
                fieldset_paint.title_background.x, 32.0f);
    check_float("fieldset title background width comes from style",
                fieldset_paint.title_background.width, 64.0f);
    check_float("fieldset title background y comes from style",
                fieldset_paint.title_background.y, 25.0f);
    check_float("fieldset title text y comes from style",
                fieldset_paint.title_text.y, 23.0f);
    check_float("fieldset title height comes from style",
                fieldset_paint.title_text.height, 14.0f);
    frame.value.padding_y = 0.0f;
    frame.value.gap = 0.0f;
    frame.value.font_size = 0.0f;
    fieldset_paint = FieldsetPaintFor((Rectangle){20, 30, 120, 60}, 40.0f,
        1, 1.0f, frame);
    check_float("fieldset keeps explicit zero title offset",
                fieldset_paint.title_background.y, 30.0f);
    check_float("fieldset keeps explicit zero text offset",
                fieldset_paint.title_text.y, 30.0f);
    check_float("fieldset title height protects layout",
                fieldset_paint.title_text.height, 18.0f);
    frame.value.fields &= ~(StylePaddingX | StylePaddingY | StyleGap |
                            StyleFontSize);
    frame.value.padding_x = 11.0f;
    frame.value.padding_y = 3.0f;
    frame.value.font_size = 13.0f;
    plot_text_paint = PlotTextPaintFor((Rectangle){20, 30, 120, 60},
        24.0f, 31.0f, 1.0f, frame, 1, 1);
    check_float("plot padding falls back when unset",
                plot_text_paint.label_bounds.x, 26.0f);
    check_float("plot text height falls back when unset",
                plot_text_paint.label_bounds.height, 18.0f);
    frame.value.fields |= StylePaddingX | StylePaddingY | StyleFontSize;
    plot_text_paint = PlotTextPaintFor((Rectangle){20, 30, 120, 60},
        24.0f, 31.0f, 1.0f, frame, 1, 1);
    check_float("plot label x comes from style",
                plot_text_paint.label_bounds.x, 31.0f);
    check_float("plot label y comes from style",
                plot_text_paint.label_bounds.y, 33.0f);
    check_float("plot label height comes from style",
                plot_text_paint.label_bounds.height, 13.0f);
    check_float("plot overlay x comes from style",
                plot_text_paint.overlay_bounds.x, 98.0f);
    check_float("plot overlay y comes from style",
                plot_text_paint.overlay_bounds.y, 33.0f);
    frame.value.padding_y = 0.0f;
    frame.value.font_size = 0.0f;
    plot_text_paint = PlotTextPaintFor((Rectangle){20, 30, 120, 60},
        24.0f, 31.0f, 1.0f, frame, 1, 1);
    check_float("plot keeps explicit zero padding y",
                plot_text_paint.label_bounds.y, 30.0f);
    check_float("plot text height protects layout",
                plot_text_paint.label_bounds.height, 18.0f);
    frame.value.fields &= ~(StylePaddingX | StylePaddingY | StyleFontSize);
    frame.value.icon_size = 8.0f;
    bullet_paint = BulletPaintFor((Rectangle){10, 20, 20, 12}, frame);
    check_float("bullet x comes from style size",
                bullet_paint.bounds.x, 16.0f);
    check_float("bullet y comes from style size",
                bullet_paint.bounds.y, 22.0f);
    check_float("bullet width comes from style size",
                bullet_paint.bounds.width, 8.0f);
    check_float("bullet radius comes from style size",
                bullet_paint.radius, 4.0f);
    frame.value.icon_size = 0.0f;
    frame.value.padding_x = 10.0f;
    progress_paint = ProgressPaintFor((Rectangle){10, 20, 100, 20}, 0, 100,
        25, 20.0f, 12.0f, 1.0f, frame, frame, frame);
    check_float("progress label padding falls back when unset",
                progress_paint.layout.label_x, 41.0f);
    check_float("progress label y", progress_paint.layout.label_y, 24.0f);
    frame.value.fields |= StylePaddingX;
    progress_paint = ProgressPaintFor((Rectangle){10, 20, 100, 20}, 0, 100,
        25, 20.0f, 12.0f, 1.0f, frame, frame, frame);
    check_float("progress label padding comes from label style",
                progress_paint.layout.label_x, 45.0f);
    frame.value.padding_x = 0.0f;
    progress_paint = ProgressPaintFor((Rectangle){10, 20, 100, 20}, 0, 100,
        25, 20.0f, 12.0f, 1.0f, frame, frame, frame);
    check_float("progress keeps explicit zero label padding",
                progress_paint.layout.label_x, 35.0f);
    frame.value.fields &= ~StylePaddingX;
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
    if(SliderTrackRole() != 4 || SliderFillRole() != 5 ||
       SliderLabelRole() != 6) {
        fprintf(stderr, "slider role policy changed\n");
        return 1;
    }
    drag_pointer = DragPointerDecisionFor(0, 0, 1, 0, 0, 1, 1, 0);
    if(!drag_pointer.start_active || !drag_pointer.update_delta ||
       drag_pointer.clear_active || drag_pointer.finish_active) {
        fprintf(stderr, "drag pointer start policy changed\n");
        return 1;
    }
    drag_pointer = DragPointerDecisionFor(1, 0, 0, 0, 1, 0, 1, 0);
    if(!drag_pointer.clear_active || drag_pointer.update_delta) {
        fprintf(stderr, "drag pointer capture clear policy changed\n");
        return 1;
    }
    drag_pointer = DragPointerDecisionFor(1, 1, 0, 0, 0, 0, 0, 1);
    if(!drag_pointer.finish_active) {
        fprintf(stderr, "drag pointer release policy changed\n");
        return 1;
    }
    slider_pointer = SliderPointerDecisionFor(0, 1, 0, 0, 1, 1, 0, 1,
                                             1, 0, 0, 0);
    if(!slider_pointer.hovered || !slider_pointer.start_active ||
       !slider_pointer.set_pointer_owner || !slider_pointer.update_value) {
        fprintf(stderr, "vertical slider press policy changed\n");
        return 1;
    }
    slider_pointer = SliderPointerDecisionFor(1, 1, 0, 0, 0, 1, 0, 0,
                                             1, 0, 1, 1);
    if(!slider_pointer.take_horizontal_owner || slider_pointer.cancel_active ||
       !slider_pointer.update_value) {
        fprintf(stderr, "horizontal slider owner policy changed\n");
        return 1;
    }
    slider_pointer = SliderPointerDecisionFor(1, 1, 0, 0, 0, 1, 0, 0,
                                             1, 0, 1, 0);
    if(slider_pointer.take_horizontal_owner || !slider_pointer.cancel_active) {
        fprintf(stderr, "horizontal slider axis cancel policy changed\n");
        return 1;
    }
    slider_pointer = SliderPointerDecisionFor(1, 1, 0, 0, 0, 0, 1, 0,
                                             0, 0, 0, 0);
    if(!slider_pointer.update_value || !slider_pointer.finish_active) {
        fprintf(stderr, "slider release policy changed\n");
        return 1;
    }
    slider_ratio = SliderRatioDecisionFor(0, 0, 1, 0, 0, 1, 1, 0);
    if(!slider_ratio.start_active || !slider_ratio.update_ratio ||
       slider_ratio.clear_active || slider_ratio.finish_active) {
        fprintf(stderr, "slider ratio start policy changed\n");
        return 1;
    }
    slider_ratio = SliderRatioDecisionFor(1, 0, 0, 0, 1, 0, 1, 0);
    if(!slider_ratio.clear_active || slider_ratio.update_ratio) {
        fprintf(stderr, "slider ratio capture clear policy changed\n");
        return 1;
    }
    slider_ratio = SliderRatioDecisionFor(1, 1, 0, 0, 0, 0, 0, 1);
    if(!slider_ratio.finish_active) {
        fprintf(stderr, "slider ratio release policy changed\n");
        return 1;
    }
    if(CheckboxBoxRoleForTone(ButtonToneNeutral) != 9 ||
       CheckboxBoxRoleForTone(ButtonToneAccent) != 10 ||
       CheckboxLabelRole() != 6) {
        fprintf(stderr, "checkbox role policy changed\n");
        return 1;
    }
    frame.value.padding_x = 72.0f;
    frame.value.padding_y = 10.0f;
    frame.value.icon_size = 12.0f;
    thumb.value.icon_size = 18.0f;
    thumb.value.gap = 5.0f;
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 1, 1},
        .ratio = 0.25f,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    check_float("slider minimum length falls back when unset",
                slider_paint.track_bounds.width, 32.0f);
    check_float("slider horizontal track size falls back when unset",
                slider_paint.track_bounds.height, 6.0f);
    check_float("slider thumb size falls back when unset",
                slider_paint.thumb_radius, 11.0f);
    check_float("slider glow expansion falls back when unset",
                slider_paint.glow_radius, 19.0f);
    check_float("slider shadow y falls back in policy",
                slider_paint.thumb_shadow_y, 33.0f);
    check_float("slider shadow radius falls back in policy",
                slider_paint.thumb_shadow_radius, 12.0f);
    check_float("slider highlight x falls back in policy",
                slider_paint.thumb_highlight_x, 15.0f);
    check_float("slider highlight y falls back in policy",
                slider_paint.thumb_highlight_y, 27.0f);
    check_float("slider highlight radius falls back in policy",
                slider_paint.thumb_highlight_radius, 4.95f);
    frame.value.fields |= StylePaddingX | StylePaddingY | StyleIconSize;
    thumb.value.fields |= StyleIconSize | StyleGap;
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 1, 1},
        .ratio = 0.25f,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    check_float("slider minimum length comes from track style",
                slider_paint.track_bounds.width, 72.0f);
    check_float("slider horizontal track size comes from track style",
                slider_paint.track_bounds.height, 10.0f);
    check_float("slider thumb size comes from thumb style",
                slider_paint.thumb_radius, 9.0f);
    check_float("slider glow expansion comes from thumb style",
                slider_paint.glow_radius, 14.0f);
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 1, 1},
        .ratio = 0.25f,
        .vertical = 1,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    check_float("slider vertical track size comes from track style",
                slider_paint.track_bounds.width, 12.0f);
    frame.value.padding_x = 0.0f;
    frame.value.padding_y = 0.0f;
    frame.value.icon_size = 0.0f;
    thumb.value.icon_size = 0.0f;
    thumb.value.gap = 0.0f;
    slider_paint = SliderPaintFor((SliderSpec){
        .bounds = {10, 20, 1, 1},
        .ratio = 0.25f,
        .scale = 1.0f,
        .track = frame,
        .active_track = frame,
        .thumb = thumb
    });
    check_float("slider minimum length protects layout",
                slider_paint.track_bounds.width, 32.0f);
    check_float("slider horizontal track protects layout",
                slider_paint.track_bounds.height, 6.0f);
    check_float("slider thumb size protects layout",
                slider_paint.thumb_radius, 11.0f);
    check_float("slider keeps explicit zero glow expansion",
                slider_paint.glow_radius, 11.0f);
    frame.value.padding_x = 70.0f;
    frame.value.padding_y = 28.0f;
    frame.value.icon_size = 12.0f;
    thumb.value.icon_size = 14.0f;
    thumb.value.gap = 6.0f;
    if(toggle_paint.thumb_fill_color != thumb.value.background) {
        fprintf(stderr, "toggle thumb fill did not come from thumb style\n");
        return 1;
    }
    if(toggle_paint.thumb_edge_color != thumb.value.border) {
        fprintf(stderr, "toggle thumb edge did not come from thumb style\n");
        return 1;
    }
    if(ToggleTrackRoleFor(0, 0) != 4 ||
       ToggleTrackRoleFor(1, 1) != 4 ||
       ToggleTrackRoleFor(1, 0) != 5 ||
       ToggleFillRole() != 5 || ToggleLabelRole() != 6) {
        fprintf(stderr, "toggle role policy changed\n");
        return 1;
    }

    frame.value.gap = 5.0f;
    frame.value.fields &= ~(StylePaddingX | StylePaddingY | StyleGap |
                            StyleIconSize);
    thumb.value.fields &= ~(StyleIconSize | StyleGap);
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 1, 1},
        .enabled = 1,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    check_float("toggle track width falls back when unset",
                toggle_paint.track_bounds.width, 54.0f);
    check_float("toggle track height falls back when unset",
                toggle_paint.track_bounds.height, 32.0f);
    check_float("toggle focus gap falls back when unset",
                toggle_paint.focus_bounds.x, 6.0f);
    check_float("toggle thumb size falls back when unset",
                toggle_paint.thumb_radius, 10.0f);
    check_float("toggle thumb inset falls back when unset",
                toggle_paint.thumb_x, 24.0f);
    check_float("toggle glow radius falls back in policy",
                toggle_paint.thumb_glow_radius, 15.0f);
    check_float("toggle shadow y falls back in policy",
                toggle_paint.thumb_shadow_y, 38.0f);
    check_float("toggle shadow radius falls back in policy",
                toggle_paint.thumb_shadow_radius, 11.0f);
    check_float("toggle highlight x falls back in policy",
                toggle_paint.thumb_highlight_x, 21.0f);
    check_float("toggle highlight y falls back in policy",
                toggle_paint.thumb_highlight_y, 32.0f);
    check_float("toggle highlight radius falls back in policy",
                toggle_paint.thumb_highlight_radius, 5.0f);
    frame.value.fields |= StylePaddingX | StylePaddingY | StyleGap |
                          StyleIconSize;
    thumb.value.fields |= StyleIconSize | StyleGap;
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 1, 1},
        .enabled = 1,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    check_float("toggle track width comes from style",
                toggle_paint.track_bounds.width, 70.0f);
    check_float("toggle track height comes from style",
                toggle_paint.track_bounds.height, 28.0f);
    check_float("toggle focus gap comes from track style",
                toggle_paint.focus_bounds.x, 5.0f);
    check_float("toggle thumb size comes from thumb style",
                toggle_paint.thumb_radius, 7.0f);
    check_float("toggle thumb inset comes from thumb style",
                toggle_paint.thumb_x, 23.0f);

    frame.value.gap = 0.0f;
    thumb.value.gap = 0.0f;
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 100, 30},
        .enabled = 1,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    check_float("toggle keeps explicit zero focus gap",
                toggle_paint.focus_bounds.x, 25.0f);
    check_float("toggle keeps explicit zero thumb inset",
                toggle_paint.thumb_x, 32.0f);

    frame.value.gap = 4.0f;
    thumb.value.gap = 6.0f;
    toggle_paint = TogglePaintFor((ToggleSpec){
        .bounds = {10, 20, 100, 30},
        .checked = 0,
        .enabled = 1,
        .has_labels = 1,
        .off_width = 12,
        .on_width = 12,
        .font = 16,
        .scale = 1.0f,
        .track = frame,
        .active = frame,
        .thumb = thumb
    });
    check_float("toggle active inset comes from active style",
                toggle_paint.active_bounds.x, 14.0f);
    check_float("toggle active height comes from active style",
                toggle_paint.active_bounds.height, 22.0f);

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

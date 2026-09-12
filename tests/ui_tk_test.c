#include "kryon.h"
#include "kry_inject.h"
#include "kryon_test.h"
#include "runtime/list_box.h"
#include "runtime/plot.h"
#include "runtime/canvas_grid.h"
#include "runtime/checkbox.h"
#include "runtime/color_picker.h"
#include "runtime/drag.h"
#include "runtime/input.h"
#include "runtime/fieldset.h"
#include "runtime/multi_select_list.h"
#include "runtime/popup_policy.h"
#include "runtime/progress.h"
#include "runtime/radio.h"
#include "runtime/segmented_control.h"
#include "runtime/selectable.h"
#include "runtime/separator.h"
#include "runtime/slider.h"
#include "runtime/spinbox.h"
#include "runtime/tab_bar.h"
#include "runtime/text_input.h"
#include "theme.h"
#include "ui_inspect.h"
#include "../src/ui/ui_internal.h"
#include "../src/ui/ui_numeric_internal.h"
#include "../src/ui/ui_numeric_input_internal.h"
#include "../src/ui/ui_tree_layout_internal.h"
#include "../src/ui/ui_disabled_internal.h"
#include "../src/ui/dropdown_store.h"
#include "../src/ui/ui_input_clip_internal.h"
#include "../src/ui/ui_popup_input_internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void
check_int(const char *name, int got, int want)
{
    if(got == want)
        return;
    fprintf(stderr, "%s: got %d want %d\n", name, got, want);
    exit(1);
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

static void
check_color(const char *name, Color got, Color want)
{
    if(got.r == want.r && got.g == want.g && got.b == want.b &&
       got.a == want.a)
        return;
    fprintf(stderr, "%s: got #%02X%02X%02X%02X want #%02X%02X%02X%02X\n",
            name, got.r, got.g, got.b, got.a,
            want.r, want.g, want.b, want.a);
    exit(1);
}

static int
test_drag_float(UIFloatDragProps props)
{
    return Drag((DragProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericFloat,
        .float_values = props.values, .value_count = props.value_count,
        .speed = props.speed, .min = props.min, .max = props.max,
        .format = props.format, .disabled = props.disabled});
}

static int
test_drag_int(UIIntDragProps props)
{
    return Drag((DragProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericInt, .int_values = props.values,
        .value_count = props.value_count, .speed = props.speed,
        .min = props.min, .max = props.max, .format = props.format,
        .disabled = props.disabled});
}

static int
test_drag_float_range(UIFloatDragRangeProps props)
{
    return Drag((DragProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericFloat, .mode = DragRange,
        .float_min = props.current_min, .float_max = props.current_max,
        .speed = props.speed, .min = props.min, .max = props.max,
        .format = props.format, .format_max = props.format_max,
        .disabled = props.disabled});
}

static int
test_drag_int_range(UIIntDragRangeProps props)
{
    return Drag((DragProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericInt, .mode = DragRange,
        .int_min = props.current_min, .int_max = props.current_max,
        .speed = props.speed, .min = props.min, .max = props.max,
        .format = props.format, .format_max = props.format_max,
        .disabled = props.disabled});
}

static int
test_slider_float(UIFloatSliderProps props)
{
    return Slider((SliderProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericFloat,
        .float_values = props.values, .value_count = props.value_count,
        .min = props.min, .max = props.max, .format = props.format,
        .disabled = props.disabled});
}

static int
test_slider_int(UIIntSliderProps props)
{
    return Slider((SliderProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericInt, .int_values = props.values,
        .value_count = props.value_count, .min = props.min, .max = props.max,
        .format = props.format, .disabled = props.disabled});
}

static int
test_vslider_int(UIIntSliderProps props)
{
    SliderProps slider = {.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericInt, .int_values = props.values,
        .value_count = props.value_count, .min = props.min, .max = props.max,
        .format = props.format, .disabled = props.disabled, .vertical = 1};
    return Slider(slider);
}

static int
test_input_int(UIIntInputProps props)
{
    return Input((InputProps){.bounds = props.bounds, .id = props.id,
        .label = props.label, .kind = NumericInt, .int_values = props.values,
        .value_count = props.value_count, .step = props.step,
        .step_fast = props.step_fast, .format = props.format,
        .disabled = props.disabled});
}

static void
test_theme_surface_helpers(void)
{
    Color mixed = MixThemeColor((Color){0, 0, 0, 255},
                                (Color){100, 50, 200, 127}, 0.5f);

    check_color("theme mix", mixed, (Color){50, 25, 100, 191});
    check_color("theme mix clamps low",
                MixThemeColor(BLACK, WHITE, -1.0f), BLACK);
    check_color("theme mix clamps high",
                MixThemeColor(BLACK, WHITE, 2.0f), WHITE);
    check_int("black luminance", GetThemeColorLuminance(BLACK), 0);
    check_int("white luminance", GetThemeColorLuminance(WHITE), 255);
    check_int("dark color", IsThemeColorDark((Color){12, 12, 12, 255}), 1);
    check_int("light color", IsThemeColorDark((Color){240, 240, 240, 255}), 0);
    check_color("readable on dark",
                GetThemeReadableText((Color){20, 20, 20, 255}), RAYWHITE);
    check_color("readable on light",
                GetThemeReadableText((Color){240, 240, 240, 255}), BLACK);

    SetThemeSource(THEME_SOURCE_APP);
    SetCurrentTheme(THEME_MONO, 0);
    check_int("surface alt alpha", GetThemeSurfaceAlt().a, 255);
    check_int("theme border alpha", GetThemeBorder().a, 255);
    check_int("muted text alpha", GetThemeMutedText().a, 255);
    check_int("selection alpha", GetThemeSelection().a, 255);
    check_color("button text", GetThemeButtonText(),
                GetThemeReadableText(GetThemeButton()));
}

static void
test_checkbox_paint_geometry_is_stable(void)
{
    Palette palette = DefaultPalette(1);
    Metrics metrics = DefaultMetrics();
    CheckboxSpec spec = {
        .bounds = {10, 20, 160, 30},
        .checked = 0,
        .enabled = 1,
        .hovered = 0,
        .pressed = 0,
        .focused = 0,
        .scale = 1.0f,
        .palette = palette,
        .metrics = metrics
    };
    CheckboxPaint unchecked = CheckboxPaintFor(spec);
    CheckboxPaint checked;
    CheckboxLayout layout = CheckboxLayoutFor(10, 20, 64, 1.0f);
    CheckboxFlagResult flags_on = CheckboxFlagApply(1, 4, true);
    CheckboxFlagResult flags_off = CheckboxFlagApply(5, 4, true);
    CheckboxFlagResult flags_idle = CheckboxFlagApply(5, 4, false);

    spec.checked = 1;
    checked = CheckboxPaintFor(spec);

    check_int("checkbox slot size", CheckboxSlotSize(1.0f), 22);
    check_int("checkbox box size", CheckboxBoxSize(1.0f), 20);
    check_int("checkbox layout width", (int)layout.bounds.width, 96);
    check_int("checkbox layout label x", (int)layout.label_x, 42);
    check_int("checkbox checked keeps box x", (int)checked.box_bounds.x,
              (int)unchecked.box_bounds.x);
    check_int("checkbox checked keeps box y", (int)checked.box_bounds.y,
              (int)unchecked.box_bounds.y);
    check_int("checkbox checked keeps box w", (int)checked.box_bounds.width,
              (int)unchecked.box_bounds.width);
    check_int("checkbox checked keeps box h", (int)checked.box_bounds.height,
              (int)unchecked.box_bounds.height);
    check_int("checkbox state is compact",
              (int)(checked.state_bounds.width - checked.box_bounds.width), 8);
    check_int("checkbox unchecked has no mark", unchecked.show_mark, 0);
    check_int("checkbox checked has mark", checked.show_mark, 1);
    check_int("checkbox flags turns on", (int)flags_on.flags, 5);
    check_int("checkbox flags on checked", flags_on.checked, 1);
    check_int("checkbox flags turns off", (int)flags_off.flags, 1);
    check_int("checkbox flags off unchecked", flags_off.checked, 0);
    check_int("checkbox idle unchanged", (int)flags_idle.flags, 5);
    check_int("checkbox idle no change", flags_idle.changed, 0);
}

static void
test_swatch_policy(void)
{
    Palette palette = DefaultPalette(0);
    Metrics metrics = DefaultMetrics();
    SwatchPaint paint = SwatchPaintFor((SwatchSpec){
        .bounds = {10, 20, 80, 30},
        .color = {20, 40, 60, 128},
        .disabled = 0,
        .hovered = 1,
        .focused = 1,
        .scale = 1.0f,
        .palette = palette,
        .metrics = metrics
    });
    SwatchPaint disabled = SwatchPaintFor((SwatchSpec){
        .bounds = {10, 20, 80, 30},
        .color = {20, 40, 60, 255},
        .disabled = 1,
        .hovered = 1,
        .focused = 1,
        .scale = 1.0f,
        .palette = palette,
        .metrics = metrics
    });

    check_int("color button checker a width", (int)paint.checker_a.width, 40);
    check_int("color button checker b x", (int)paint.checker_b.x, 50);
    check_int("color button label x", (int)paint.label_x, 16);
    check_int("color button swatch alpha", paint.swatch_color.a, 128);
    check_int("color button focus visible", paint.show_focus, 1);
    check_int("color button disabled alpha", disabled.swatch_color.a, 128);
    check_int("color button disabled focus hidden", disabled.show_focus, 0);
}

static void
test_color_picker_policy(void)
{
    Rectangle bounds = {10, 20, 120, 160};
    ColorPickerLayout layout = ColorPickerLayoutFor(bounds, 4, 1.0f);
    Rectangle row = ColorPickerChannelBounds(bounds, 2, 4, 1.0f);
    Color rgba = ColorPickerColorFor(-0.5f, 0.5f, 2.0f, 0.25f, 4);
    Color rgb = ColorPickerColorFor(1.0f, 0.0f, 0.5f, 0.0f, 3);

    check_float("color picker row height", layout.row_height, 30.0f);
    check_float("color picker swatch y", layout.swatch_bounds.y, 144.0f);
    check_float("color picker swatch height", layout.swatch_bounds.height, 36.0f);
    check_float("color picker channel y", row.y, 80.0f);
    check_float("color picker channel height", row.height, 28.0f);
    check_int("color picker clamp low", ColorPickerChannelByte(-1.0f), 0);
    check_int("color picker clamp high", ColorPickerChannelByte(2.0f), 255);
    check_color("color picker rgba", rgba, (Color){0, 128, 255, 64});
    check_color("color picker rgb alpha", rgb, (Color){255, 0, 128, 255});
}

static void
test_button_policy(void)
{
    check_int("action enabled", ButtonActionEnabled(false, false) ? 1 : 0, 1);
    check_int("action own disabled", ButtonActionEnabled(true, false) ? 1 : 0, 0);
    check_int("action parent disabled", ButtonActionEnabled(false, true) ? 1 : 0, 0);
    check_int("arrow left glyph", ButtonArrowGlyph(ARROW_LEFT), '<');
    check_int("arrow right glyph", ButtonArrowGlyph(ARROW_RIGHT), '>');
    check_int("arrow up glyph", ButtonArrowGlyph(ARROW_UP), '^');
    check_int("arrow down glyph", ButtonArrowGlyph(ARROW_DOWN), 'v');
}

static void
test_separator_policy(void)
{
    Rectangle bounds = {10, 20, 100, 30};
    SeparatorLine horizontal = SeparatorLineFor(bounds, 0, 0x11223344);
    SeparatorLine vertical = SeparatorLineFor(bounds, 1, 0x55667788);
    SeparatorLabelPaint text = SeparatorLabelPaintFor(bounds, 40, 1, 14, 1.0f,
                                                      0x01020304, 0x05060708);
    SeparatorLabelPaint no_text = SeparatorLabelPaintFor(bounds, 0, 0, 14,
                                                         1.0f, 0x01020304,
                                                         0x05060708);
    BulletPaint bullet = BulletPaintFor((Rectangle){10, 20, 20, 12},
                                        0xAABBCCDD);

    check_int("separator horizontal y", (int)horizontal.line.y, 35);
    check_int("separator horizontal width", (int)horizontal.line.width, 100);
    check_int("separator vertical x", (int)vertical.line.x, 60);
    check_int("separator vertical height", (int)vertical.line.height, 30);
    check_int("separator text line x", (int)text.line.x, 62);
    check_int("separator text line width", (int)text.line.width, 48);
    check_int("separator text visible", text.show_text, 1);
    check_int("separator no-text line x", (int)no_text.line.x, 10);
    check_int("separator no-text hidden", no_text.show_text, 0);
    check_int("bullet bounds x", (int)bullet.bounds.x, 17);
    check_int("bullet bounds y", (int)bullet.bounds.y, 23);
    check_int("bullet radius", (int)bullet.radius, 3);
}

static void
test_canvas_grid_policy(void)
{
    Rectangle bounds = {10, 20, 25, 25};
    int spacing = CanvasGridSpacing(2, 4);
    CanvasGridLine vertical = CanvasGridVerticalLine(bounds, 2, spacing,
                                                     0x01020304);
    CanvasGridLine horizontal = CanvasGridHorizontalLine(bounds, 1, spacing,
                                                         0x05060708);

    check_int("canvas grid spacing minimum", spacing, 4);
    check_int("canvas grid vertical count",
              CanvasGridLineCount(bounds.width, spacing), 7);
    check_int("canvas grid horizontal count",
              CanvasGridLineCount(bounds.height, spacing), 7);
    check_int("canvas grid vertical x", (int)vertical.bounds.x, 18);
    check_int("canvas grid vertical height", (int)vertical.bounds.height, 25);
    check_int("canvas grid horizontal y", (int)horizontal.bounds.y, 24);
    check_int("canvas grid horizontal width", (int)horizontal.bounds.width, 25);
}

static void
test_fieldset_policy(void)
{
    Rectangle bounds = {10, 20, 120, 80};
    FieldsetPaint paint = FieldsetPaintFor(bounds, 40, 1, 1.0f,
                                               0x01020304, 0x05060708,
                                               0x090A0B0C);
    FieldsetPaint no_title = FieldsetPaintFor(bounds, 40, 0, 1.0f,
                                                  0x01020304, 0x05060708,
                                                  0x090A0B0C);

    check_int("fieldset title bg x", (int)paint.title_background.x, 18);
    check_int("fieldset title bg y", (int)paint.title_background.y, 12);
    check_int("fieldset title bg width", (int)paint.title_background.width, 56);
    check_int("fieldset title text x", (int)paint.title_text.x, 26);
    check_int("fieldset title text y", (int)paint.title_text.y, 11);
    check_int("fieldset show title", paint.show_title, 1);
    check_int("fieldset no title hidden", no_title.show_title, 0);
    check_int("fieldset no title width",
              (int)no_title.title_background.width, 0);
}

static void
test_plot_policy(void)
{
    Rectangle bounds = {10, 20, 120, 60};
    PlotRange range = PlotRangeFor(0.0f, 0.0f, 0.0f, 1.0f);
    PlotMark bar;
    PlotMark line;
    PlotTextPaint text;

    check_int("plot negative offset", PlotOffset(4, -1), 3);
    check_int("plot wrapped offset", PlotOffset(4, 5), 1);
    check_float("plot range min", range.min_value, 0.0f);
    check_float("plot range max", range.max_value, 1.0f);
    check_float("plot normalize clamp", PlotNormalize(2.0f, range), 1.0f);

    bar = PlotHistogramBar(bounds, 2, 4, 0.75f, range, 0x11223344u);
    check_float("plot bar x", bar.bounds.x, 71.0f);
    check_float("plot bar y", bar.bounds.y, 35.0f);
    check_float("plot bar width", bar.bounds.width, 28.0f);
    check_float("plot bar height", bar.bounds.height, 45.0f);
    check_int("plot bar color", (int)bar.color, (int)0x11223344u);

    line = PlotLineSegment(bounds, 2, 4, 0.25f, 0.75f, range, 0x55667788u);
    check_float("plot line x", line.bounds.x, 50.0f);
    check_float("plot line y", line.bounds.y, 65.0f);
    check_float("plot line width", line.bounds.width, 40.0f);
    check_float("plot line height", line.bounds.height, -30.0f);
    check_int("plot line color", (int)line.color, (int)0x55667788u);

    text = PlotTextPaintFor(bounds, 20.0f, 30.0f, 1.0f, 0xaabbccddu,
                            true, true);
    check_float("plot label x", text.label_bounds.x, 16.0f);
    check_float("plot label y", text.label_bounds.y, 24.0f);
    check_float("plot overlay x", text.overlay_bounds.x, 94.0f);
    check_float("plot overlay y", text.overlay_bounds.y, 24.0f);
    check_int("plot label shown", text.show_label ? 1 : 0, 1);
    check_int("plot overlay shown", text.show_overlay ? 1 : 0, 1);
}

static void
test_progress_layout_policy(void)
{
    Rectangle bounds = {10, 20, 100, 10};
    ProgressLayout low = ProgressLayoutFor(bounds, 0, 100, 25, 20.0f, 6.0f);
    ProgressLayout high = ProgressLayoutFor(bounds, 0, 100, 80, 20.0f, 6.0f);
    ProgressLayout clamped = ProgressLayoutFor(bounds, 10, 10, 99, 20.0f, 6.0f);

    check_int("progress low ratio", (int)(low.ratio * 100.0f + 0.5f), 25);
    check_int("progress low fill", (int)low.fill_bounds.width, 25);
    check_int("progress low label after fill", (int)low.label_x, 41);
    check_int("progress low label color", low.label_on_fill, 0);
    check_int("progress high fill", (int)high.fill_bounds.width, 80);
    check_int("progress high label inside fill", (int)high.label_x, 64);
    check_int("progress high label color", high.label_on_fill, 1);
    check_int("progress clamped ratio",
              (int)(clamped.ratio * 100.0f + 0.5f), 100);
    check_int("progress clamped fill", (int)clamped.fill_bounds.width, 100);
}

static void
test_selectable_paint_policy(void)
{
    Rectangle bounds = {10, 20, 120, 28};
    SelectablePaint idle = SelectablePaintFor((SelectableSpec){
        .bounds = bounds,
        .selected = 0,
        .hovered = 0,
        .pressed = 0,
        .disabled = 0,
        .fill_color = 0x11223344,
        .hover_color = 0x55667788,
        .text_color = 0x99AABBCC,
        .disabled_text_color = 0x01020304,
        .label_inset = 8.0f
    });
    SelectablePaint selected = SelectablePaintFor((SelectableSpec){
        .bounds = bounds,
        .selected = 1,
        .hovered = 0,
        .pressed = 0,
        .disabled = 0,
        .fill_color = 0x11223344,
        .hover_color = 0x55667788,
        .text_color = 0x99AABBCC,
        .disabled_text_color = 0x01020304,
        .label_inset = 8.0f
    });
    SelectablePaint hovered = SelectablePaintFor((SelectableSpec){
        .bounds = bounds,
        .selected = 1,
        .hovered = 1,
        .pressed = 0,
        .disabled = 0,
        .fill_color = 0x11223344,
        .hover_color = 0x55667788,
        .text_color = 0x99AABBCC,
        .disabled_text_color = 0x01020304,
        .label_inset = 8.0f
    });
    SelectablePaint disabled = SelectablePaintFor((SelectableSpec){
        .bounds = bounds,
        .selected = 1,
        .hovered = 1,
        .pressed = 0,
        .disabled = 1,
        .fill_color = 0x11223344,
        .hover_color = 0x55667788,
        .text_color = 0x99AABBCC,
        .disabled_text_color = 0x01020304,
        .label_inset = 8.0f
    });

    check_int("selectable idle fill", idle.draw_fill, 0);
    check_int("selectable label inset", (int)idle.label_x, 18);
    check_int("selectable selected fill", selected.draw_fill, 1);
    check_int("selectable selected color", (int)selected.fill_color, 0x11223344);
    check_int("selectable hovered color", (int)hovered.fill_color, 0x55667788);
    check_int("selectable disabled fill", disabled.draw_fill, 0);
    check_int("selectable disabled text", (int)disabled.text_color, 0x01020304);
}

static void
test_radio_paint_policy(void)
{
    Rectangle bounds = {10, 20, 140, 28};
    RadioPaint unchecked = RadioPaintFor((RadioSpec){
        .bounds = bounds,
        .checked = 0,
        .disabled = 0,
        .default_style = 0,
        .selected_amount = 0.0f,
        .scale = 1.0f,
        .text_color = 0x111111FF,
        .icon_color = 0x222222FF,
        .button_color = 0x333333FF,
        .primary_color = 0x444444FF,
        .surface_variant_color = 0x555555FF,
        .disabled_color = 0x666666FF
    });
    RadioPaint checked = RadioPaintFor((RadioSpec){
        .bounds = bounds,
        .checked = 1,
        .disabled = 0,
        .default_style = 0,
        .selected_amount = 1.0f,
        .scale = 1.0f,
        .text_color = 0x111111FF,
        .icon_color = 0x222222FF,
        .button_color = 0x333333FF,
        .primary_color = 0x444444FF,
        .surface_variant_color = 0x555555FF,
        .disabled_color = 0x666666FF
    });
    RadioPaint disabled = RadioPaintFor((RadioSpec){
        .bounds = bounds,
        .checked = 1,
        .disabled = 1,
        .default_style = 0,
        .selected_amount = 1.0f,
        .scale = 1.0f,
        .text_color = 0x111111FF,
        .icon_color = 0x222222FF,
        .button_color = 0x333333FF,
        .primary_color = 0x444444FF,
        .surface_variant_color = 0x555555FF,
        .disabled_color = 0x666666FF
    });

    check_int("radio size", RadioSize(1.0f), 20);
    check_int("radio touch size", RadioTouchSize(1.0f), 40);
    check_int("radio label x", (int)unchecked.label_x, 38);
    check_int("radio unchecked fill", (int)unchecked.fill_radius, 0);
    check_int("radio unchecked ring", (int)unchecked.ring_color, 0x222222FF);
    check_int("radio checked fill", (int)checked.fill_radius, 8);
    check_int("radio disabled ring", (int)disabled.ring_color, 0x333333FF);
    check_int("radio disabled label", (int)disabled.label_color, 0x333333FF);
}

static void
test_list_box_layout_policy(void)
{
    Rectangle bounds = {10, 20, 100, 95};
    ListBoxLayout layout = ListBoxLayoutFor(bounds, 10, 24, 0, 50);
    Rectangle row = ListBoxRowBounds(bounds, 1, layout);
    ListBoxNavigation down = ListBoxNavigate(2, 10, 4, 0, 24,
                                             bounds.height, layout.max_scroll);
    ListBoxNavigation end = ListBoxNavigate(2, 10, 2, 0, 24,
                                            bounds.height, layout.max_scroll);

    check_int("list row height", layout.row_height, 24);
    check_int("list content height", layout.content_height, 240);
    check_int("list max scroll", layout.max_scroll, 145);
    check_int("list first row", layout.first_row, 2);
    check_int("list y offset", layout.y_offset, 2);
    check_int("list visible rows", layout.visible_rows, 3);
    check_int("list row y", (int)row.y, 42);
    check_int("list down selected", down.selected, 3);
    check_int("list down changed", down.changed, 1);
    check_int("list end selected", end.selected, 9);
    check_int("list end scroll", end.scroll, 145);
}

static void
test_multi_select_policy(void)
{
    Rectangle bounds = {10, 20, 120, 90};
    Rectangle row = MultiSelectRowBounds(bounds, 2, 28);
    MultiSelectNavResult down = MultiSelectNavigate(3, 0, 0, 0,
        0, 0, 0, 1, 0, 0);
    MultiSelectNavResult shift_down = MultiSelectNavigate(3, 1, 0, 1,
        0, 0, 0, 1, 0, 0);
    MultiSelectNavResult space = MultiSelectNavigate(3, 2, 0, 0,
        0, 0, 0, 0, 1, 0);

    check_int("multi row default height", MultiSelectRowHeight(0, 1.0f), 28);
    check_float("multi row y", row.y, 76.0f);
    check_float("multi row height", row.height, 28.0f);
    check_int("multi focused fallback", MultiSelectFocusedRow(-1, 2, 3), 2);

    check_int("multi nav down clicked", down.clicked, 1);
    check_int("multi nav down anchor", down.anchor, 1);
    check_int("multi nav down anchor changed", down.anchor_changed ? 1 : 0, 1);
    check_int("multi shift range anchor", shift_down.range_anchor, 1);
    check_int("multi shift clicked", shift_down.clicked, 2);
    check_int("multi shift control forced", shift_down.control ? 1 : 0, 1);
    check_int("multi space clicked", space.clicked, 2);
    check_int("multi space control", space.control ? 1 : 0, 1);

    check_int("multi plain clears other",
        MultiSelectSelectionForRow(0, 1, 1, 3, 0, 0, 0, -1) ? 1 : 0, 0);
    check_int("multi plain selects clicked",
        MultiSelectSelectionForRow(1, 0, 1, 3, 0, 0, 0, -1) ? 1 : 0, 1);
    check_int("multi control toggles",
        MultiSelectSelectionForRow(1, 1, 1, 3, 0, 1, 0, -1) ? 1 : 0, 0);
    check_int("multi shift range keeps row",
        MultiSelectSelectionForRow(2, 0, 2, 3, 0, 0, 1, -1) ? 1 : 0, 1);
    check_int("multi anchor after plain",
              MultiSelectAnchorAfterClick(0, 2, 3, 0, 0, -1), 2);
    check_int("multi anchor after shift",
              MultiSelectAnchorAfterClick(0, 2, 3, 0, 1, -1), 0);
}

static void
test_tab_bar_policy(void)
{
    Rectangle bounds = {10, 20, 240, 32};
    TabBarMetrics metrics = TabBarDefaultMetrics(80, 160, 1.0f);
    int label_width = TabBarTabWidth(42, 1, 0, 0, metrics);
    int close_width = TabBarTabWidth(42, 1, 0, 1, metrics);
    int icon_width = TabBarTabWidth(0, 0, 1, 0, metrics);
    int total = TabBarTotalWidth(label_width + close_width + icon_width,
                                 3, metrics.gap);
    TabBarScroll scroll = TabBarScrollFor(bounds.width, total, 999);
    Rectangle equal_last = TabBarEqualTabBounds(bounds, 3, 2);

    check_int("tab bar height", TabBarPolicyHeight(1.0f), 32);
    check_int("tab label width min", label_width, 80);
    check_int("tab close width", close_width, 82);
    check_int("tab icon width", icon_width, 44);
    check_int("tab total width", total, 214);
    check_int("tab equal tabs", scroll.equal_tabs, 1);
    check_int("tab equal scroll", scroll.scroll, 0);
    check_int("tab equal last x", (int)equal_last.x, 170);
    check_int("tab equal last w", (int)equal_last.width, 80);
}

static void
test_popup_policy(void)
{
    Rectangle bounds = {20, 30, 120, 80};
    Rectangle trigger = {5, 6, 40, 24};
    PopupDecision plain = PopupDecisionFor(0, 0);
    PopupDecision tooltip = PopupDecisionFor(PopupTooltip, 0);
    PopupDecision modal = PopupDecisionFor(PopupModal, 0);
    PopupDecision context = PopupDecisionFor(PopupContext, 0);
    PopupDecision invalid = PopupDecisionFor(PopupTooltip | PopupModal, 0);
    Rectangle modal_input = PopupInputBounds(modal, bounds, 640.0f, 480.0f);

    check_int("popup plain valid", plain.valid, 1);
    check_int("popup plain captures", plain.captures_input, 1);
    check_int("popup plain requires open", plain.requires_open, 1);
    check_int("popup tooltip trigger", tooltip.requires_trigger, 1);
    check_int("popup tooltip captures", tooltip.captures_input, 0);
    check_int("popup modal backdrop", PopupBackdropAlpha(modal), 180);
    check_int("popup context trigger", context.requires_trigger, 1);
    check_int("popup invalid combination", invalid.valid, 0);
    check_int("popup missing open",
              PopupCanBegin(plain, 7, bounds, trigger, 0), 0);
    check_int("popup can begin",
              PopupCanBegin(context, 7, bounds, trigger, 1), 1);
    check_int("popup disabled closes",
              PopupOpenAfterDisabled(plain, 1, 1), 0);
    check_int("popup tooltip ignores open",
              PopupOpenAfterDisabled(tooltip, 1, 1), 1);
    check_int("popup modal input x", (int)modal_input.x, 0);
    check_int("popup modal input w", (int)modal_input.width, 640);
}

static void
test_text_input_policy(void)
{
    TextInputMetrics metrics = TextInputMetricsFor(0, 0, 0, -1,
                                                   16, 10, 8, 6);
    TextFieldScroll scroll = TextFieldScrollFor(20.0f, 100.0f,
                                                metrics.padding_x, 180, 999);

    check_int("text input font default", metrics.font, 16);
    check_int("text input padding x default", metrics.padding_x, 10);
    check_int("text input padding y default", metrics.padding_y, 8);
    check_int("text input line gap default", metrics.line_gap, 6);
    check_int("text input content width",
              TextInputContentWidth(100.0f, metrics.padding_x), 80);
    check_int("text area page rows",
              TextAreaPageRows(72.0f, metrics.font, metrics.line_gap,
                               metrics.padding_y), 2);
    check_int("text field max scroll", scroll.max_scroll, 100);
    check_int("text field scroll clamp", scroll.scroll, 100);
    check_int("text field origin", scroll.text_origin_x, -70);
    check_int("text field reveal left",
              TextFieldRevealScroll(50, 100, 80, 4, 8), 0);
    check_int("text field reveal right",
              TextFieldRevealScroll(0, 100, 80, 120, 8), 48);
}

static void
test_segmented_control_policy(void)
{
    SegmentedMetrics metrics = SegmentedDefaultMetrics(0, 0, 0, 0,
                                                       6, 30, 72, 180);
    int first = SegmentedItemWidth(40, metrics, 20);
    int second = SegmentedItemWidth(100, metrics, 20);
    int next = SegmentedNextRowWidth(first, second, metrics.gap);
    SegmentedRow row = SegmentedRowFor(10.0f, 240.0f, 20, 0, 2, next,
                                       1, metrics);

    check_int("segmented gap default", metrics.gap, 6);
    check_int("segmented row height default", metrics.row_height, 30);
    check_int("segmented item min", first, 72);
    check_int("segmented item measured", second, 120);
    check_int("segmented next width", next, 198);
    check_int("segmented should wrap",
              SegmentedShouldWrap(1, next, next + metrics.gap + second,
                                  240), 1);
    check_int("segmented height", SegmentedHeightForRows(2, 30, 6), 66);
    check_int("segmented row button width", row.button_width, 117);
    check_int("segmented row x", row.x, 10);
}

static void
test_spinbox_policy(void)
{
    Rectangle bounds = {10, 20, 100, 30};
    SpinboxLayout layout = SpinboxLayoutFor(bounds, 28);
    SpinboxStepResult inc = SpinboxStepValue(4, 0, 5, 2, 1, 0);
    SpinboxStepResult dec = SpinboxStepValue(1, 0, 5, 2, -1, 0);
    SpinboxStepResult wrap_inc = SpinboxStepValue(5, 0, 5, 1, 1, 1);
    SpinboxStepResult wrap_dec = SpinboxStepValue(0, 0, 5, 1, -1, 1);

    check_int("spinbox default step", SpinboxEffectiveStep(0), 1);
    check_int("spinbox left width", (int)layout.left.width, 28);
    check_int("spinbox text x", (int)layout.text.x, 38);
    check_int("spinbox text width", (int)layout.text.width, 44);
    check_int("spinbox right x", (int)layout.right.x, 82);
    check_int("spinbox increment clamps", inc.value, 5);
    check_int("spinbox increment changed", inc.changed, 1);
    check_int("spinbox decrement clamps", dec.value, 0);
    check_int("spinbox wrap increment", wrap_inc.value, 0);
    check_int("spinbox wrap decrement", wrap_dec.value, 5);
}

static void
test_semantic_font_sizes_follow_ui_scale(void)
{
    BeginUIFrame(720, 1400, 1.75f);
    check_int("body font at 1.75x", GetFontSize(), 28);
    check_int("small font at 1.75x", GetSmallFontSize(), 25);
    check_int("title font at 1.75x", GetTitleFontSize("Title", 1000), 42);
    check_int("fitted font preserves body", FitFontSize("Day", 1000, Text16, Text8), 28);
    check_int("fitted caption token scales", FitFontSize("", 1000, Text12, Text8), 21);
    {
        int fitted = FitFontSize("Delete Habit", Scale(64),
                                 GetFontSize(), Text8);
        check_int("button label fit stays inside content width",
                  TextWidth("Delete Habit", fitted) <= Scale(64), 1);
    }
    EndUIFrame();
}

static void
test_slider_value_policy(void)
{
    SliderScalarStep float_step;
    SliderWholeStep int_step;

    check_float("slider clamp low", SliderClampRatio(-0.5f), 0.0f);
    check_float("slider clamp high", SliderClampRatio(1.5f), 1.0f);
    check_float("slider float ratio", SliderScalarRatio(0.25f, 0.0f, 1.0f), 0.25f);
    check_float("slider float value clamps", SliderScalarValue(0.0f, 1.0f, 1.25f), 1.0f);
    check_float("slider int ratio", SliderWholeRatio(5, 0, 10), 0.5f);
    check_int("slider int value rounds", SliderWholeValue(0, 10, 0.86f), 9);

    float_step = SliderScalarKeyboardValue(0.25f, 0.0f, 1.0f, 1, 0, 0, 0, 0);
    check_int("slider float keyboard changed", float_step.changed ? 1 : 0, 1);
    check_float("slider float keyboard value", float_step.value, 0.26f);
    float_step = SliderScalarKeyboardValue(0.25f, 0.0f, 1.0f, 1, 0, 0, 1, 0);
    check_float("slider float keyboard slow", float_step.value, 0.251f);
    float_step = SliderScalarKeyboardValue(0.25f, 0.0f, 1.0f, 1, 0, 0, 0, 1);
    check_float("slider float keyboard fast", float_step.value, 0.35f);
    float_step = SliderScalarKeyboardValue(0.25f, 0.0f, 1.0f, 0, 1, 0, 0, 0);
    check_float("slider float keyboard home", float_step.value, 0.0f);

    int_step = SliderWholeKeyboardValue(5, 0, 10, 1, 0, 0, 0, 0);
    check_int("slider int keyboard changed", int_step.changed ? 1 : 0, 1);
    check_int("slider int keyboard value", int_step.value, 6);
    int_step = SliderWholeKeyboardValue(50, 0, 1000, 1, 0, 0, 1, 0);
    check_int("slider int keyboard slow", int_step.value, 51);
    int_step = SliderWholeKeyboardValue(50, 0, 1000, 1, 0, 0, 0, 1);
    check_int("slider int keyboard fast", int_step.value, 150);
    int_step = SliderWholeKeyboardValue(5, 0, 10, 0, 0, 1, 0, 0);
    check_int("slider int keyboard end", int_step.value, 10);
}

static void
test_drag_value_policy(void)
{
    DragScalarStep float_step;
    DragWholeStep int_step;

    check_float("drag default speed", DragEffectiveSpeed(0.0f), 1.0f);
    check_float("drag float clamp", DragScalarClamp(12.0f, 0.0f, 10.0f), 10.0f);
    check_int("drag int clamp", DragWholeClamp(-2, 0, 10), 0);
    check_int("drag int rounded positive",
              DragWholeRoundedDelta(0.49f, 0), 0);
    check_int("drag int forced positive",
              DragWholeRoundedDelta(0.49f, 1), 1);
    check_int("drag int rounded negative",
              DragWholeRoundedDelta(-1.6f, 0), -2);

    float_step = DragScalarKeyboardValue(2.0f, 0.25f, 0.0f, 10.0f,
                                        1, 0, 0, 0, 0);
    check_int("drag float keyboard changed", float_step.changed ? 1 : 0, 1);
    check_float("drag float keyboard value", float_step.value, 2.25f);
    float_step = DragScalarKeyboardValue(2.0f, 0.25f, 0.0f, 10.0f,
                                        1, 0, 0, 0, 1);
    check_float("drag float keyboard fast", float_step.value, 4.5f);
    float_step = DragScalarDeltaValue(2.0f, 5.0f, 0.5f, 0.0f, 4.0f);
    check_float("drag float delta clamps", float_step.value, 4.0f);

    int_step = DragWholeKeyboardValue(3, 0.25f, 0, 10, 1, 0, 0, 0, 0);
    check_int("drag int keyboard minimum step", int_step.value, 4);
    int_step = DragWholeKeyboardValue(3, 2.0f, 0, 10, 1, 0, 0, 0, 1);
    check_int("drag int keyboard fast", int_step.value, 10);
    int_step = DragWholeDeltaValue(3, 0.4f, 1.0f, 0, 10);
    check_int("drag int small delta unchanged", int_step.changed ? 1 : 0, 0);
    int_step = DragWholeDeltaValue(3, -2.0f, 1.0f, 0, 10);
    check_int("drag int delta value", int_step.value, 1);
}

static void
test_input_value_policy(void)
{
    InputScalarStep float_step;
    InputWholeStep int_step;
    InputDoubleStep double_step;

    check_float("input float effective step",
                InputScalarEffectiveStep(0.1f, 1.0f, 0), 0.1f);
    check_float("input float effective fast",
                InputScalarEffectiveStep(0.1f, 1.0f, 1), 1.0f);
    check_int("input int effective step",
              InputWholeEffectiveStep(2, 10, 0), 2);
    check_int("input int effective fast",
              InputWholeEffectiveStep(2, 10, 1), 10);

    float_step = InputScalarStepValue(2.5f, 0.5f, 4.0f, 1, 0);
    check_int("input float step changed", float_step.changed ? 1 : 0, 1);
    check_float("input float step value", float_step.value, 3.0f);
    float_step = InputScalarStepValue(2.5f, 0.5f, 4.0f, -1, 1);
    check_float("input float fast minus", float_step.value, -1.5f);

    int_step = InputWholeStepValue(6, 2, 10, 1, 0);
    check_int("input int step value", int_step.value, 8);
    int_step = InputWholeStepValue(6, 2, 10, -1, 1);
    check_int("input int fast minus", int_step.value, -4);

    double_step = InputDoubleStepValue(2.125, 0.125, 1.0, 1, 0);
    check_int("input double changed", double_step.changed ? 1 : 0, 1);
    check_float("input double step value", (float)double_step.value, 2.25f);
}

static void
test_slider_keyboard_navigation(void)
{
    float floats[2] = {0.25f,0.75f};
    int ints[1] = {5};
    UIFloatSliderProps horizontal = {
        .bounds = {10,10,200,30}, .id = 600, .values = floats,
        .value_count = 2, .min = 0.0f, .max = 1.0f
    };
    UIIntSliderProps vertical = {
        .bounds = {10,60,30,120}, .id = 601, .values = ints,
        .value_count = 1, .min = 0, .max = 10
    };
    int second_focus;
    int inspect_enabled;

    InjectReset();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    inspect_enabled = UIInspectEnabled();
    SetUIInspectEnabled(0);
    InjectMousePosition(35,20);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    check_int("click focuses slider component",GetUIFocus(),600);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    SetUIInspectEnabled(inspect_enabled);

    InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(640,480,1.0f);
    check_int("slider Right changed",test_slider_float(horizontal),1);
    EndUIFrame();
    check_int("slider Right value",(int)(floats[0]*1000.0f+0.5f),260);

    InjectPump();
    InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    check_int("slider Shift fast value",(int)(floats[0]*1000.0f+0.5f),360);
    InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    InjectKey(KEY_LEFT_ALT,1); InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    check_int("slider Alt slow value",(int)(floats[0]*1000.0f+0.5f),361);
    InjectKey(KEY_LEFT_ALT,0); InjectPump();

    InjectKeyTap(KEY_TAB); InjectPump();
    BeginUIFrame(640,480,1.0f); test_slider_float(horizontal); EndUIFrame();
    second_focus = GetUIFocus();
    check_int("slider Tab reaches second component",second_focus != 600,1);
    InjectKeyTap(KEY_LEFT); InjectPump();
    BeginUIFrame(640,480,1.0f);
    check_int("second slider component changed",test_slider_float(horizontal),1);
    EndUIFrame();
    check_int("second slider component value",
              (int)(floats[1]*1000.0f+0.5f),740);

    SetUIFocus(601); InjectKeyTap(KEY_UP); InjectPump();
    BeginUIFrame(640,480,1.0f);
    check_int("vertical slider Up changed",test_vslider_int(vertical),1);
    EndUIFrame();
    check_int("vertical slider Up value",ints[0],6);
    InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(640,480,1.0f); test_vslider_int(vertical); EndUIFrame();
    check_int("vertical slider Down value",ints[0],5);
    InjectKeyTap(KEY_HOME); InjectPump();
    BeginUIFrame(640,480,1.0f); test_vslider_int(vertical); EndUIFrame();
    check_int("vertical slider Home value",ints[0],0);
    InjectKeyTap(KEY_END); InjectPump();
    BeginUIFrame(640,480,1.0f); test_vslider_int(vertical); EndUIFrame();
    check_int("vertical slider End value",ints[0],10);

    vertical.disabled = 1;
    SetUIFocus(601); InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(640,480,1.0f);
    check_int("disabled slider unchanged",test_vslider_int(vertical),0);
    EndUIFrame();
    check_int("disabled slider value",ints[0],10);
}

static void
draw_drag_keyboard(UIFloatDragProps floats, UIIntDragProps ints)
{
    BeginUIFrame(480,240,1);
    (void)test_drag_float(floats);
    (void)test_drag_int(ints);
    EndUIFrame();
}

static void
test_drag_keyboard_navigation(void)
{
    float floats[] = {2.0f,5.0f};
    int ints[] = {2,5};
    UIFloatDragProps fp = {.bounds={10,10,200,30},.id=630,.values=floats,
        .value_count=2,.speed=0.25f,.min=0,.max=10};
    UIIntDragProps ip = {.bounds={10,50,200,30},.id=631,.values=ints,
        .value_count=2,.speed=2,.min=0,.max=10};

    InjectReset(); draw_drag_keyboard(fp,ip);
    SetUIFocus(630); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag float Right",(int)(floats[0]*100),225);
    InjectPump(); InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_RIGHT); InjectPump();
    draw_drag_keyboard(fp,ip);
    check_int("drag float Shift Right",(int)(floats[0]*100),475);
    InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    InjectKeyTap(KEY_TAB); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag float Tab second",GetUIFocus(),ui_numeric_focus_id(630,1,0));
    InjectKeyTap(KEY_HOME); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag float Home",(int)floats[1],0);

    SetUIFocus(631); InjectKeyTap(KEY_RIGHT); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag int Right",ints[0],4);
    InjectKeyTap(KEY_TAB); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag int Tab second",GetUIFocus(),ui_numeric_focus_id(631,1,1));
    InjectKeyTap(KEY_LEFT); InjectPump(); draw_drag_keyboard(fp,ip);
    check_int("drag int second Left",ints[1],3);

    {
        float fmin=2.0f, fmax=8.0f;
        int imin=2, imax=8;
        UIFloatDragRangeProps fr = {.bounds={240,10,200,30},.id=632,
            .current_min=&fmin,.current_max=&fmax,.speed=1,.min=0,.max=10};
        UIIntDragRangeProps ir = {.bounds={240,50,200,30},.id=633,
            .current_min=&imin,.current_max=&imax,.speed=2,.min=0,.max=10};
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        SetUIFocus(632); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag float range min",(int)fmin,3);
        InjectKeyTap(KEY_TAB); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag float range Tab",GetUIFocus(),ui_numeric_focus_id(632,1,0));
        InjectKeyTap(KEY_LEFT); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag float range max",(int)fmax,7);
        SetUIFocus(633); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag int range min",imin,4);
        InjectKeyTap(KEY_TAB); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag int range Tab",GetUIFocus(),ui_numeric_focus_id(633,1,1));
        InjectKeyTap(KEY_LEFT); InjectPump();
        BeginUIFrame(480,240,1); test_drag_float_range(fr); test_drag_int_range(ir); EndUIFrame();
        check_int("drag int range max",imax,6);
        SetUIFocus(632); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(480,240,1); BeginDisabled(1); test_drag_float_range(fr); EndDisabled(); EndUIFrame();
        check_int("disabled drag range",(int)fmin,3);
    }
}

static void
draw_numeric_temporary_inputs(UIFloatDragProps drag, UIIntSliderProps slider)
{
    BeginUIFrame(320,160,1);
    (void)test_drag_float(drag);
    (void)test_slider_int(slider);
    EndUIFrame();
}

static void
test_numeric_ctrl_click_editing(void)
{
    float drag_value = 1.25f;
    int slider_value = 4;
    UIFloatDragProps drag = {.bounds={10,10,140,30},.id=634,
        .values=&drag_value,.value_count=1,.speed=0.1f,.min=0,.max=10};
    UIIntSliderProps slider = {.bounds={10,60,140,30},.id=635,
        .values=&slider_value,.value_count=1,.min=0,.max=10};

    InjectReset();
    ClearTextInputFocus();
    draw_numeric_temporary_inputs(drag,slider);

    InjectMousePosition(30,20);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_A);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectText("7.25");
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    check_int("drag temporary input",(int)(drag_value*100),725);
    InjectKeyTap(KEY_ENTER);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);

    InjectMousePosition(30,20);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    check_int("single click has no temporary input",
              ui_numeric_input_state(3,634,0)->focused,0);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectMousePosition(30,20);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    check_int("double-click opens temporary input",
              ui_numeric_input_state(3,634,0)->focused,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectKeyTap(KEY_ENTER);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);

    InjectMousePosition(30,70);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectKeyTap(KEY_A);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectText("19");
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    check_int("slider temporary input is unclamped",slider_value,19);

    slider.disabled = 1;
    InjectKeyTap(KEY_ENTER);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectMousePosition(30,70);
    InjectKey(KEY_LEFT_CONTROL,1);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    InjectText("8");
    InjectPump();
    draw_numeric_temporary_inputs(drag,slider);
    check_int("disabled slider temporary input",slider_value,19);
}

static void
test_tab_bar_keyboard_navigation(void)
{
    Tab tabs[] = {
        {.label="One"},
        {.label="Disabled",.disabled=1},
        {.label="Three",.closeable=1}
    };
    int selected = 0;
    int closed = -1;
    TabBarProps props = {.bounds={10,10,300,32},.tabs=tabs,.count=3,
        .selected_index=selected,.closed_index=&closed,.id=634};

    InjectReset();
    BeginUIFrame(360,180,1); ui_tab_bar_keyboard_input(props); EndUIFrame();
    SetUIFocus(props.id); InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(360,180,1);
    selected = ui_tab_bar_keyboard_input(props);
    EndUIFrame();
    check_int("tab Right skips disabled",selected,2);
    props.selected_index = selected;

    InjectKeyTap(KEY_DELETE); InjectPump();
    BeginUIFrame(360,180,1);
    check_int("tab Delete does not select",ui_tab_bar_keyboard_input(props),-1);
    EndUIFrame();
    check_int("tab Delete closes selected",closed,2);

    InjectKeyTap(KEY_HOME); InjectPump();
    BeginUIFrame(360,180,1);
    selected = ui_tab_bar_keyboard_input(props);
    EndUIFrame();
    check_int("tab Home",selected,0);

    props.disabled = 1;
    props.selected_index = 0;
    InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(360,180,1); SetUIFocus(props.id);
    check_int("disabled tab ignores keyboard",ui_tab_bar_keyboard_input(props),-1);
    EndUIFrame();
    InjectReset();
}

static void
test_tab_bar_owned_scroll_state(void)
{
    int fallback = 0;
    int *first;
    int *second;

    BeginUIFrame(360,180,1);
    first = ui_tab_bar_owned_scroll(635,&fallback);
    *first = 47;
    second = ui_tab_bar_owned_scroll(636,&fallback);
    check_int("tab bars own independent scroll",*second,0);
    check_int("tab bar scroll persists by id",
              *ui_tab_bar_owned_scroll(635,&fallback),47);
    check_int("anonymous tab bar uses caller fallback",
              ui_tab_bar_owned_scroll(0,&fallback) == &fallback,1);
    EndUIFrame();
}

static void
test_composed_tab_bar_scope(void)
{
    Tab tabs[] = {{.label="One"},{.label="Two"}};
    TabBarProps props = {.bounds={10,10,200,30},.tabs=tabs,.count=2,
        .id=637};
    int selected = 0;
    int visible = -1;

    InjectReset();
    BeginUIFrame(260,140,1);
    check_int("composed tab bar begins",BeginTabBar(props,&selected),1);
    if(BeginTabItem(0)) {
        visible = 0;
        Button((ButtonProps){.bounds={20,60,80,28},.label="First",
                             .id=638});
        EndTabItem();
    }
    check_int("unselected composed tab hidden",BeginTabItem(1),0);
    EndTabBar();
    EndUIFrame();
    check_int("first composed tab content",visible,0);

    SetUIFocus(props.id);
    InjectKeyTap(KEY_RIGHT);
    InjectPump();
    BeginUIFrame(260,140,1);
    check_int("composed tab bar reopens",BeginTabBar(props,&selected),1);
    check_int("old composed tab hidden",BeginTabItem(0),0);
    if(BeginTabItem(1)) {
        visible = 1;
        Checkbox((CheckboxProps){.bounds = {20,60,120,34}, .id = 639,
                 .label = "Second", .value = &visible});
        EndTabItem();
    }
    EndTabBar();
    EndUIFrame();
    check_int("composed tab writes selection",selected,1);
    check_int("selected composed tab content",visible,1);

    check_int("invalid composed tab bar stays closed",
              BeginTabBar((TabBarProps){0},&selected),0);
    InjectReset();
}

static void
test_popup_tab_bar_keyboard_ownership(void)
{
    Tab tabs[] = {{.label="One"},{.label="Two"}};
    TabBarProps props = {.bounds={20,20,180,30},.tabs=tabs,.count=2,
        .selected_index=0,.id=26132};

    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(240,120,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(
            context,26100,(Rectangle){10,10,220,100});
        UIPopupInputToken child = ui_popup_input_begin(
            context,26101,(Rectangle){15,15,200,80});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(props.id);
        check_int("only top popup tab bar handles keyboard",
                  ui_tab_bar_keyboard_input(props),inside ? 1 : -1);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_step_button_keyboard_navigation(void)
{
    int value = 2;
    int input = 4;
    SpinboxProps spin = {.bounds={10,10,120,30},.id=635,.min=0,.max=5,
        .step=1,.value=&value};
    UIIntInputProps field = {.bounds={10,50,160,30},.id=636,.values=&input,
        .value_count=1,.step=2,.step_fast=10};

    InjectReset();
    BeginUIFrame(240,140,1); RenderSpinbox(spin); RenderInputWhole(field); EndUIFrame();
    SetUIFocus(spin.id * 10 + 2); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(240,140,1);
    check_int("spinbox keyboard changed",RenderSpinbox(spin),1);
    RenderInputWhole(field); EndUIFrame();
    check_int("spinbox keyboard increment",value,3);

    {
        UINumericInputState *state = ui_numeric_input_state(1,field.id,0);
        SetUIFocus(state->token + 2); InjectKeyTap(KEY_SPACE); InjectPump();
        BeginUIFrame(240,140,1); RenderSpinbox(spin);
        check_int("numeric step keyboard changed",RenderInputWhole(field),1);
        EndUIFrame();
        check_int("numeric step keyboard increment",input,6);
    }

    spin.disabled = 1;
    SetUIFocus(spin.id * 10 + 2); InjectKeyTap(KEY_SPACE); InjectPump();
    BeginUIFrame(240,140,1);
    check_int("disabled spinbox keyboard",RenderSpinbox(spin),0);
    EndUIFrame();
    check_int("disabled spinbox value",value,3);
    InjectReset();
}

static void
draw_focusable_choices(int *checkbox, int *selected, int *flags,
                       int disable_flags, int *checkbox_activated,
                       int *selectable_activated, int *flags_activated,
                       int *radio_activated)
{
    BeginUIFrame(640,480,1.0f);
    *checkbox_activated = Checkbox((CheckboxProps){
        .bounds = {10,130,120,34}, .id = 609, .label = "Check",
        .value = checkbox
    });
    *selectable_activated = Selectable((SelectableProps){
        .bounds = {10,10,140,28}, .id = 610, .label = "Choice",
        .selected = selected
    });
    BeginDisabled(disable_flags);
    *flags_activated = Checkbox((CheckboxProps){
        .bounds = {10,50,140,28}, .id = 611, .label = "Flag",
        .flags = flags, .flags_value = 4
    });
    EndDisabled();
    *radio_activated = Radio((RadioProps){
        .bounds = {10,90,140,28}, .label = "Radio", .id = 612
    });
    EndUIFrame();
}

static void
test_focusable_choice_keyboard_navigation(void)
{
    int checkbox = 0;
    int selected = 0;
    int flags = 0;
    int checkbox_activated;
    int selectable_activated;
    int flags_activated;
    int radio_activated;

    InjectReset();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);

    SetUIFocus(609); InjectKeyTap(KEY_ENTER); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("checkbox Enter activation",checkbox_activated,1);
    check_int("checkbox Enter state",checkbox,1);

    SetUIFocus(610); InjectKeyTap(KEY_SPACE); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("selectable Space activation",selectable_activated,1);
    check_int("selectable Space state",selected,1);

    SetUIFocus(611); InjectKeyTap(KEY_ENTER); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("checkbox flags Enter activation",flags_activated,1);
    check_int("checkbox flags Enter state",flags,4);

    SetUIFocus(612); InjectKeyTap(KEY_SPACE); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("radio Space activation",radio_activated,612);

    SetUIFocus(610); InjectKeyTap(KEY_TAB); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,0,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("choice Tab traversal",GetUIFocus(),611);

    SetUIFocus(611); InjectKeyTap(KEY_SPACE); InjectPump();
    draw_focusable_choices(&checkbox,&selected,&flags,1,&checkbox_activated,
                           &selectable_activated,&flags_activated,
                           &radio_activated);
    check_int("disabled flags rejects activation",flags_activated,0);
    check_int("disabled flags preserves state",flags,4);
}

static void
test_toggle_keyboard_navigation(void)
{
    int value = 0;
    int activated;

    InjectReset();
    BeginUIFrame(240,120,1);
    (void)Toggle((ToggleProps){.bounds={10,10,120,34},.id=613,.value=&value,.off_label="Off",.on_label="On"});
    (void)Button((ButtonProps){.bounds={10,54,80,28},.id=614,.label="Next"});
    EndUIFrame();

    SetUIFocus(613); InjectKeyTap(KEY_SPACE); InjectPump();
    BeginUIFrame(240,120,1);
    activated = Toggle((ToggleProps){.bounds={10,10,120,34},.id=613,.value=&value,.off_label="Off",.on_label="On"});
    (void)Button((ButtonProps){.bounds={10,54,80,28},.id=614,.label="Next"});
    EndUIFrame();
    check_int("toggle Space activation",activated,1);
    check_int("toggle Space state",value,1);

    SetUIFocus(613); InjectKeyTap(KEY_TAB); InjectPump();
    BeginUIFrame(240,120,1);
    (void)Toggle((ToggleProps){.bounds={10,10,120,34},.id=613,.value=&value,.off_label="Off",.on_label="On"});
    (void)Button((ButtonProps){.bounds={10,54,80,28},.id=614,.label="Next"});
    EndUIFrame();
    check_int("toggle Tab traversal",GetUIFocus(),614);

    SetUIFocus(613); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(240,120,1);
    BeginDisabled(1);
    activated = Toggle((ToggleProps){.bounds={10,10,120,34},.id=613,.value=&value,.off_label="Off",.on_label="On"});
    EndDisabled();
    EndUIFrame();
    check_int("disabled toggle rejects activation",activated,0);
    check_int("disabled toggle preserves state",value,1);
}

static int
draw_multi_select_keyboard(MultiSelectListProps list)
{
    int clicked;
    BeginUIFrame(320,240,1);
    clicked = MultiSelectList(list);
    (void)Button((ButtonProps){.bounds={10,110,80,28},.id=619,.label="Next"});
    EndUIFrame();
    return clicked;
}

static void
test_multi_select_keyboard_navigation(void)
{
    const char *items[] = {"Alpha","Beta","Gamma"};
    int selected[] = {1,0,0};
    int count = 1;
    int anchor = 0;
    MultiSelectListProps list = {
        .bounds={10,10,180,84},.id=618,.items=items,.item_count=3,
        .selected=selected,.selected_count=&count,.anchor=&anchor,.row_height=28
    };

    InjectReset();
    draw_multi_select_keyboard(list);
    SetUIFocus(618); InjectKeyTap(KEY_DOWN); InjectPump();
    check_int("multi Down clicked",draw_multi_select_keyboard(list),1);
    check_int("multi Down anchor",anchor,1);
    check_int("multi Down count",count,1);
    check_int("multi Down selection",selected[1],1);

    InjectPump();
    InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_DOWN); InjectPump();
    check_int("multi Shift Down clicked",draw_multi_select_keyboard(list),2);
    check_int("multi Shift Down count",count,2);
    check_int("multi Shift Down selection",selected[2],1);
    InjectKey(KEY_LEFT_SHIFT,0); InjectPump();

    InjectKeyTap(KEY_SPACE); InjectPump();
    check_int("multi Space clicked",draw_multi_select_keyboard(list),2);
    check_int("multi Space count",count,1);
    check_int("multi Space toggle",selected[2],0);

    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_HOME); InjectPump();
    check_int("multi Control Home cursor",draw_multi_select_keyboard(list),-1);
    check_int("multi Control Home anchor",anchor,0);
    check_int("multi Control Home selection",selected[1],1);
    InjectKey(KEY_LEFT_CONTROL,0); InjectPump();

    InjectKeyTap(KEY_ENTER); InjectPump();
    check_int("multi Enter clicked",draw_multi_select_keyboard(list),0);
    check_int("multi Enter selection",selected[0],1);
    check_int("multi Enter count",count,1);

    InjectKeyTap(KEY_TAB); InjectPump();
    draw_multi_select_keyboard(list);
    check_int("multi Tab traversal",GetUIFocus(),619);

    list.disabled = 1;
    SetUIFocus(618); InjectKeyTap(KEY_SPACE); InjectPump();
    check_int("disabled multi rejects keyboard",draw_multi_select_keyboard(list),-1);
    check_int("disabled multi preserves selection",selected[0],1);
}

static void
test_focusable_image_keyboard_navigation(void)
{
    ImageProps image = {
        .asset_path = "", .bounds = {10,10,40,30}, .tint = WHITE,
        .fit = IMAGE_FIT_CONTAIN
    };

    InjectReset();
    BeginUIFrame(240,180,1);
    RenderInvisibleButton((InvisibleButtonProps){{10,50,40,30},620,0});
    Button((ButtonProps){
        .bounds=image.bounds,.id=621,.image_asset_path=image.asset_path,
        .image_bounds=image.bounds,.image_source=image.source,
        .image_origin=image.origin,.image_rotation=image.rotation,
        .image_tint=image.tint,.image_fit=image.fit,
        .image_background=BLACK
    });
    Button((ButtonProps){
        .bounds={10,90,80,30},.id=622,.label="Color",
        .swatch=true,.swatch_color=RED
    });
    EndUIFrame();

    SetUIFocus(620); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("invisible button Enter activation",
              RenderInvisibleButton((InvisibleButtonProps){{10,50,40,30},620,0}),1);
    EndUIFrame();

    SetUIFocus(621); InjectKeyTap(KEY_SPACE); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("image button Space activation",
              Button((ButtonProps){
                  .bounds=image.bounds,.id=621,
                  .image_asset_path=image.asset_path,
                  .image_bounds=image.bounds,
                  .image_source=image.source,
                  .image_origin=image.origin,
                  .image_rotation=image.rotation,
                  .image_tint=image.tint,
                  .image_fit=image.fit,
                  .image_background=BLACK
              }),1);
    EndUIFrame();

    SetUIFocus(622); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("color button Enter activation",
              Button((ButtonProps){
                  .bounds={10,90,80,30},.id=622,.label="Color",
                  .swatch=true,.swatch_color=RED
              }),1);
    EndUIFrame();
}

static void
test_reorder_uses_item_center_and_header_handle(void)
{
    ReorderItem items[2] = {
        {1, {10, 100, 200, 100}, 0},
        {2, {10, 210, 200, 100}, 0}
    };
    ReorderList list = {
        .id = 811, .bounds = {0, 0, 300, 500},
        .items = items, .item_count = 2,
        .handle_width = 200, .handle_height = 40,
        .drag_threshold = 5
    };
    ReorderListResult result;

    InjectReset();
    InjectMousePosition(50, 170);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateReorderList(list);
    EndUIFrame();
    check_int("reorder ignores item body below handle", result.active, 0);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();

    InjectReset();
    InjectMousePosition(50, 120);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateReorderList(list);
    EndUIFrame();
    check_int("reorder captures header", result.active, 1);

    InjectMousePosition(50, 240);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateReorderList(list);
    EndUIFrame();
    check_int("reorder drag active", result.dragging, 1);
    check_int("reorder target follows lifted center", result.target_index, 1);

    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();
    BeginUIFrame(300, 500, 1.0f);
    result = UpdateReorderList(list);
    EndUIFrame();
}

static void
test_menu_bar_switches_while_popup_captures_input(void)
{
    static const MenuItem file_items[] = {
        {MenuCommand, "Open", "Ctrl+O", 101, 0, 0, NULL, 0}
    };
    static const MenuItem edit_items[] = {
        {MenuCommand, "Copy", "Ctrl+C", 201, 0, 0, NULL, 0}
    };
    static const Menu menus[] = {
        {{0, 0, 0, 0}, "File", file_items, 1},
        {{0, 0, 0, 0}, "Edit", edit_items, 1}
    };
    Rectangle bounds = {0, 0, 240, 28};
    int open_index = 0;
    int font;
    int edit_x;
    MenuBarResult result;

    InjectReset();
    BeginUIFrame(640, 480, 1.0f);
    font = GetFontSize();
    edit_x = Scale(4) + TextWidth("File", font) + Scale(24) +
             Scale(2) + Scale(8);
    EndUIFrame();

    InjectTap((float)edit_x, 14.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    PushUIInputCapture((Rectangle){0, 28, 180, 64}, 1);
    result = MenuBar(700, bounds, menus, 2, &open_index);
    EndUIFrame();

    check_int("menu bar switches over popup capture", open_index, 1);
    check_int("menu bar result switches over popup capture",
              result.open_index, 1);
}

static void
test_menu_keyboard_navigation(void)
{
    static const MenuItem child[] = {
        {MenuCommand,"Child",NULL,23,0,0,NULL,0}
    };
    static const MenuItem file[] = {
        {MenuCommand,"Open",NULL,21,0,0,NULL,0},
        {MenuSeparator,NULL,NULL,0,0,0,NULL,0},
        {MenuCommand,"Disabled",NULL,22,1,0,NULL,0},
        {MenuSubmenu,"More",NULL,24,0,0,child,1}
    };
    static const MenuItem edit[] = {
        {MenuCommand,"Copy",NULL,31,0,0,NULL,0}
    };
    static const Menu menus[] = {
        {{0,0,0,0},"File",file,4}, {{0,0,0,0},"Edit",edit,1}
    };
    Rectangle bounds = {0,0,360,30};
    int open = -1;
    MenuBarResult result;

    InjectReset(); InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    result = MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    check_int("menu Down opens",open,0);
    check_int("menu Down open result",result.open_index,0);

    InjectKeyTap(KEY_END); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectPump(); BeginUIFrame(640,480,1);
    result = MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    check_int("submenu Enter activates",result.activated_id,23);
    check_int("submenu activation closes",open,-1);

    InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    result = MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    check_int("menu Right then Down opens next",result.open_index,1);
    InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectPump(); BeginUIFrame(640,480,1);
    result = MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    check_int("second menu Enter activates",result.activated_id,31);

    InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    InjectKeyTap(KEY_ESCAPE); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(300);
    MenuBar(300,bounds,menus,2,&open); EndUIFrame();
    check_int("menu Escape closes",open,-1);
    InjectReset();
}

static void
test_popup_menu_keyboard_navigation(void)
{
    static const MenuItem items[] = {
        {MenuCommand,"Disabled",NULL,41,1,0,NULL,0},
        {MenuSeparator,NULL,NULL,0,0,0,NULL,0},
        {MenuCommand,"Run",NULL,42,0,0,NULL,0}
    };
    int activated;

    InjectReset(); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(400);
    activated = PopupMenu(400,20,20,items,3); EndUIFrame();
    check_int("popup Enter skips disabled",activated,42);

    InjectReset(); InjectKeyTap(KEY_ENTER); InjectPump();
    BeginUIFrame(640,480,1); SetUIFocus(999);
    activated = PopupMenu(400,20,20,items,3); EndUIFrame();
    check_int("unfocused popup rejects Enter",activated,0);
    InjectReset();
}

static void
test_popup_menu_keyboard_ownership(void)
{
    static const MenuItem items[] = {
        {MenuCommand,"Run",NULL,25710,0,0,NULL,0}
    };
    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(KEY_ENTER); InjectPump();
        BeginUIFrame(320,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(
            context,25700,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(
            context,25701,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25711);
        check_int("only top popup menu handles keyboard",
                  PopupMenu(25711,10,10,items,1),inside ? 25710 : 0);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    for(int inside = 0; inside < 2; inside++) {
        UIPopupInput *context;
        UIPopupInput *previous;
        UIPopupInputToken parent;
        UIPopupInputToken child;
        int expected_focus = inside ? 0 : 25711;

        InjectReset();
        InjectKeyTap(KEY_ESCAPE);
        InjectPump();
        BeginUIFrame(320,240,1);
        context = ui_popup_input_create();
        ui_popup_input_frame(context);
        previous = ui_popup_input_bind(context);
        parent = ui_popup_input_begin(
            context,25700,(Rectangle){180,180,40,40});
        child = ui_popup_input_begin(
            context,25701,(Rectangle){190,190,20,20});
        if(!inside)
            ui_popup_input_end(child);
        SetUIFocus(25711);
        (void)PopupMenu(25711,10,10,items,1);
        check_int("only top popup menu handles Escape",
                  GetUIFocus(),expected_focus);
        if(inside)
            ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_circle_click_uses_ui_release_path(void)
{
    int hover = 0;
    int clicked = 0;
    int second_clicked = 0;

    InjectReset();
    InjectTap(100.0f, 100.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    clicked = UIHandleCircleClick((Vector2){100.0f, 100.0f}, 32.0f, 0, &hover);
    second_clicked =
        UIHandleClick((Rectangle){80.0f, 80.0f, 40.0f, 40.0f}, 0, NULL);
    EndUIFrame();

    check_int("circle click inside", clicked, 1);
    check_int("circle click hover", hover, UIHoverEffectsEnabled() ? 1 : 0);
    check_int("circle click consumes release", second_clicked, 0);

    InjectReset();
    InjectTap(160.0f, 100.0f);
    InjectPump();
    InjectPump();

    BeginUIFrame(640, 480, 1.0f);
    clicked = UIHandleCircleClick((Vector2){100.0f, 100.0f}, 32.0f, 0, NULL);
    EndUIFrame();

    check_int("circle click outside", clicked, 0);
}

static void
test_icon_button_activation(void)
{
    IconActionSpec button = {.bounds = {10, 10, 44, 44}, .icon_size = 24};

    for(int disabled = 0; disabled <= 1; disabled++) {
        button.disabled = disabled;
        InjectReset();
        InjectTap(30, 30);
        InjectPump();
        BeginUIFrame(220, 100, 1.0f);
        check_int("icon action press", RenderIconAction(button), 0);
        EndUIFrame();
        InjectPump();
        BeginUIFrame(220, 100, 1.0f);
        check_int("icon action release", RenderIconAction(button), !disabled);
        check_int("icon action release consumed", RenderIconAction(button), 0);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_nested_disabled_scope(void)
{
    ButtonProps button = {.bounds={10,10,80,28},.label="Blocked",
                          .font=14,.id=145};
    Color text = GetThemeText();

    BeginDisabled(1);
    check_int("disabled scope dims", GetThemeText().a < text.a, 1);
    BeginDisabled(0);
    check_int("disabled false nested in true", GetThemeText().a < text.a, 1);
    EndDisabled();
    check_int("disabled outer remains", GetThemeText().a < text.a, 1);
    EndDisabled();
    check_color("disabled scope restores theme", GetThemeText(), text);

    InjectReset();
    InjectTap(30, 20);
    InjectPump();
    BeginUIFrame(220, 100, 1.0f);
    BeginDisabled(1);
    check_int("disabled button press frame", Button(button), 0);
    EndDisabled();
    EndUIFrame();
    InjectPump();
    BeginUIFrame(220, 100, 1.0f);
    BeginDisabled(1);
    check_int("disabled button release frame", Button(button), 0);
    EndDisabled();
    EndUIFrame();
}

static void
test_disabled_scalar_cancels_gesture(void)
{
    for(int slider = 0; slider < 2; slider++) {
        for(int scope = 0; scope < 2; scope++) {
            float value = 25.0f;
            float before = value;
            UIFloatDragProps drag = {0};
            UIFloatSliderProps slide = {0};
            drag.bounds = slide.bounds = (Rectangle){10, 10, 100, 24};
            drag.id = 982;
            slide.id = 981;
            drag.values = slide.values = &value;
            drag.value_count = slide.value_count = 1;
            drag.max = slide.max = 100;
            drag.speed = 1;
            InjectReset();
            for(int step = 0; step < 4; step++) {
                InjectMousePosition((float)(35 + step * 15), 20);
                if(step == 0)
                    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
                if(step == 3)
                    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
                InjectPump();
                BeginUIFrame(220, 100, 1.0f);
                if(scope)
                    BeginDisabled(step == 1);
                drag.disabled = slide.disabled = !scope && step == 1;
                if(slider)
                    (void)test_slider_float(slide);
                else
                    (void)test_drag_float(drag);
                if(scope)
                    EndDisabled();
                EndUIFrame();
                if(step == 0)
                    before = value;
                else
                    check_int("disabled scalar cancels held gesture", value == before, 1);
            }
        }
    }
}

static void
test_deep_disabled_scopes(void)
{
    int keyboard = SetUIKeyboardInputEnabled(1);
    for(int outer = 0; outer < 2; outer++) {
        BeginDisabled(outer);
        for(int depth = 0; depth < 130; depth++)
            BeginDisabled(depth == 100);
        check_int("deep disabled keyboard", UIKeyboardInputEnabled(), 0);
        for(int depth = 129; depth >= 0; depth--) {
            EndDisabled();
            check_int("deep disabled unwind", UIKeyboardInputEnabled(),
                      !outer && depth <= 100);
        }
        EndDisabled();
        check_int("deep disabled restored", UIKeyboardInputEnabled(), 1);
    }
    SetUIKeyboardInputEnabled(keyboard);
}

static void
test_collapsible_composes_children(void)
{
    bool open = false;
    int actions = 0;
    CollapsibleProps section = {.bounds = {10, 10, 180, 200}, .label = "Details", .open = &open};
    ButtonProps child = {.bounds={10,46,100,28},.label="Child",
                         .font=14,.id=983};
    int ys[] = {20, 50, 20, 50};

    InjectReset();
    for(int click = 0; click < 4; click++) {
        InjectTap(20, (float)ys[click]);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240, 300, 1.0f);
            (void)Collapsible(section);
            if(open && Button(child))
                actions++;
            EndUIFrame();
        }
        check_int("collapsible open state", open, click < 2);
        check_int("collapsible child actions", actions, click > 0);
    }
}

static void
test_tree_header_modes(void)
{
    bool open = false;
    CollapsibleProps p = {.bounds = {10,10,180,32}, .label = "Node", .open = &open,
                          .tree = 1, .depth = 2, .selected = 1, .id = 993};
    InjectReset();
    for(int mode = 0; mode < 4; mode++) {
        p.leaf = mode == 0;
        p.disabled = mode == 1;
        InjectTap(mode == 2 ? 20 : 60,20);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1.0f);
            Collapsible(p);
            EndUIFrame();
        }
        check_int("tree leaf/disabled/indent/open",open,mode == 3);
    }
}

static void
test_closeable_collapsible(void)
{
    bool open = false;
    bool visible = true;
    CollapsibleProps p = {.bounds = {10,10,180,32}, .label = "Closeable",
                          .open = &open, .id = 9961, .visible = &visible};
    int changed = 0;

    InjectReset();
    InjectTap(180,20);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        changed |= Collapsible(p);
        EndUIFrame();
    }
    check_int("collapsible close changed", changed, 1);
    check_int("collapsible close visible", visible, 0);
    check_int("collapsible close preserves open", open, 0);

    InjectTap(20,20);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        Collapsible(p);
        EndUIFrame();
    }
    check_int("hidden collapsible ignores input", open, 0);

    visible = true;
    p.disabled = 1;
    InjectTap(180,20);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        Collapsible(p);
        EndUIFrame();
    }
    check_int("disabled collapsible cannot close", visible, 1);
}

static void
test_tree_header_keyboard_gates(void)
{
    bool open = false;
    CollapsibleProps p = {.bounds = {10,10,180,32}, .open = &open, .tree = 1, .id = 994};
    InjectReset();
    for(int mode = 0; mode < 4; mode++) {
        p.leaf = mode == 0;
        p.disabled = mode == 1;
        SetUIFocus(994);
        InjectKeyTap(KEY_RIGHT);
        for(int frame = 0; frame < 2; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1.0f);
            BeginDisabled(mode == 2);
            Collapsible(p);
            EndDisabled();
            EndUIFrame();
        }
        check_int("tree keyboard gates",open,mode == 3);
    }
}

static void
test_dropdown_popup_lifecycle(void)
{
    const char *options[] = {"One", "Two"};
    int selected = 0;
    DropdownProps p = {.bounds = {10,10,160,28}, .id = 996, .options = options, .option_count = 2, .selected_index = &selected};
    for(int mode = 0; mode < 3; mode++) {
        InjectReset();
        p.disabled = 0;
        InjectTap(20,20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1.0f); Dropdown(p); EndUIFrame();
        }
        check_int("dropdown opened capture",UIInputCapturesClick((Vector2){20,70}),1);
        int background_scroll = 0;
        InjectMousePosition(20,70);
        InjectWheel(-1);
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        BeginScroll((Rectangle){10,40,180,120},400,&background_scroll);
        EndScroll();
        Dropdown(p);
        EndUIFrame();
        check_int("popup owns wheel before owner declaration",background_scroll,0);
        p.disabled = mode == 0;
        InjectTap(20,75);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1.0f);
            BeginDisabled(mode == 1);
            if(mode != 2) Dropdown(p);
            EndDisabled(); EndUIFrame();
        }
        check_int("dropdown lifecycle selection",selected,0);
        check_int("dropdown released capture",UIInputCapturesClick((Vector2){20,70}),0);
        p.disabled = 0;
        BeginUIFrame(240,240,1.0f); Dropdown(p); EndUIFrame();
        check_int("dropdown stays closed",UIInputCapturesClick((Vector2){20,70}),0);
    }
}

static void
test_dropdown_store_isolation(void)
{
    const char *options[] = {"One", "Two"};
    DropdownStore *first = dropdown_store_new();
    DropdownStore *second = dropdown_store_new();
    DropdownStore *frame_store;
    int selected = 0;

    InjectReset();
    InjectTap(20,20);
    for(int frame = 0; frame < 3; frame++) {
        InjectPump();
        BeginUIFrame(240,240,1.0f);
        frame_store = dropdown_store_swap(first);
        Dropdown((DropdownProps){.id = 9961, .bounds = {10, 10, 160, 28},
            .options = options, .option_count = 2, .selected_index = &selected});
        ui_dropdown_overlays();
        dropdown_store_swap(frame_store);
        EndUIFrame();
    }
    frame_store = dropdown_store_swap(first);
    check_int("first dropdown store owns popup",
              dropdown_captures((Vector2){20,70}),1);

    dropdown_store_swap(second);
    check_int("second dropdown store does not inherit popup",
              dropdown_captures((Vector2){20,70}),0);
    Dropdown((DropdownProps){.id = 9961, .bounds = {10, 10, 160, 28},
            .options = options, .option_count = 2, .selected_index = &selected});
    check_int("same ID remains closed in second store",
              dropdown_captures((Vector2){20,70}),0);

    dropdown_store_swap(first);
    check_int("first dropdown store restores popup",
              dropdown_captures((Vector2){20,70}),1);
    dropdown_store_swap(frame_store);

    dropdown_store_free(second);
    dropdown_store_free(first);
}

static void
test_many_dropdown_identities(void)
{
    const char *options[] = {"One", "Two"};
    int selected[41] = {0};
    InjectReset();
    for(int phase = 0; phase < 4; phase++) {
        if(phase < 3) InjectTap(20, phase == 1 ? 75 : 20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            for(int i = phase == 3 ? 1 : 0; i < 41; i++) {
                Dropdown((DropdownProps){.bounds = {i ? 300 : 10,10,160,28},
                    .id = 20000+i, .options = options, .option_count = 2,
                    .selected_index = &selected[i]});
            }
            EndUIFrame();
        }
        if(phase == 0 || phase == 2)
            check_int("41 combos keep first owner's capture",UIInputCapturesClick((Vector2){20,70}),1);
        if(phase == 3)
            check_int("missing first dropdown releases capture",UIInputCapturesClick((Vector2){20,70}),0);
        if(phase == 1)
            check_int("41 combos keep first owner's selection",selected[0],1);
        for(int i = 1; i < 41; i++)
            check_int("41 combos preserve independent selection",selected[i],0);
    }
    InjectReset();
    BeginUIFrame(240,240,1);
    EndUIFrame();
    check_int("retired dropdown releases capture",UIInputCapturesClick((Vector2){20,70}),0);
}

static void
test_large_dropdown_options(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    int selected = 0;
    InjectReset();
    for(int phase = 0; phase < 2; phase++) {
        InjectTap(20, phase == 0 ? 20 : 46 + 130*28 + 10);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,6000,1);
            Dropdown((DropdownProps){.bounds = {10,10,160,28}, .id = 21000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
    }
    check_int("dropdown selects beyond old 128 option limit",selected,130);
    InjectReset();
    BeginUIFrame(240,240,1);
    EndUIFrame();
}

static void
test_dropdown_keyboard_navigation(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    int selected = 0;
    const int keys[] = {0, KEY_END, KEY_UP, KEY_ENTER, 0, KEY_HOME, KEY_ESCAPE, 0, KEY_HOME, KEY_DOWN, KEY_ENTER};
    InjectReset();
    for(int step = 0; step < 11; step++) {
        if(keys[step] == 0) InjectTap(20,20);
        else InjectKeyTap(keys[step]);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            Dropdown((DropdownProps){.bounds = {10,10,160,28}, .id = 23000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
        check_int("dropdown keyboard commits only on Enter",selected,step < 3 ? 0 : step < 10 ? 129 : 1);
    }
    for(int step = 0; step < 3; step++) {
        if(step == 0) InjectTap(20,20);
        else if(step == 1) InjectKeyTap(KEY_END);
        else InjectTap(20,200);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump();
            BeginUIFrame(240,240,1);
            Dropdown((DropdownProps){.bounds = {10,10,160,28}, .id = 23000,
                .options = options, .option_count = 131, .selected_index = &selected});
            EndUIFrame();
        }
    }
    check_int("keyboard End reveals last option for pointer selection",selected,130);
    InjectReset();
}

static void
test_popup_preedit_cancellation(void)
{
    for(int cause = 0; cause < 3; cause++) {
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextComposition();
        for(int frame = 0; frame < 3; frame++) {
            if(frame == 0) SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"ni",2,0);
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            BeginTree(Key("popup-preedit-cancellation"));
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 1 && cause == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(frame == 1 && cause == 2 ? 0 : 26010);
            TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=26010,
                .read_only=frame == 1 && cause == 1});
            ui_popup_input_end(parent);
            UIEvent event;
            while(NextEvent(&event)) {}
            EndTree();
            int composition_events = 0;
            while(NextEvent(&event))
                if(event.kind == UI_EVENT_COMPOSITION_CHANGED) composition_events++;
            check_int("preedit starts then cancels without revival",composition_events,frame < 2 ? 1 : 0);
            check_int("preedit cancellation leaves committed text intact",strcmp(text,"a"),0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ui_popup_input_destroy(context);
    }
    ClearTextComposition(); InjectReset();
}

static void
test_popup_composition_dismissal_replay(void)
{
    for(int read_only = 0; read_only < 2; read_only++)
    for(int area = 0; area < 2; area++) {
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextComposition();
        for(int frame = 0; frame < 3; frame++) {
            if(frame != 1) {
                SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"ni",2,0);
                SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,"x",1,0);
            }
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            BeginTree(Key("popup-composition-replay"));
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(26000); focused = 1;
            if(area)
                TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=26000,.read_only=read_only});
            else
                TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=26000,.read_only=read_only});
            ui_popup_input_end(parent);
            EndTree();
            check_int("IME commit respects popup ownership and read-only state",
                strcmp(text,frame == 2 && !read_only ? "ax" : "a"),0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ui_popup_input_destroy(context);
    }
    ClearTextComposition(); InjectReset();
}

static void
test_popup_text_dismissal_replay(void)
{
    for(int close_same_frame = 0; close_same_frame < 2; close_same_frame++)
    for(int area = 0; area < 2; area++)
    for(int queued = 0; queued < 3; queued++) {
        if(area && queued == 2) continue;
        UIPopupInput *context = ui_popup_input_create();
        char text[32] = "a";
        int cursor = 1, focused = 1, commit = 0;
        InjectReset(); ClearTextInputFocus();
        for(int frame = 0; frame < 3; frame++) {
            if(frame != 1) {
                if(queued == 2) {
                    QueueTextInputBackspace(); QueueTextInputEnter();
                } else if(queued) QueueTextInputCodepoint('x');
                else InjectText("x");
            }
            InjectPump(); BeginUIFrame(240,240,1);
            ui_popup_input_frame(context);
            UIPopupInput *previous = ui_popup_input_bind(context);
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                ui_popup_input_end(child);
            } else ui_popup_input_close(context,1);
            SetUIFocus(25900); focused = 1;
            if(area)
                TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.focus_id=25900});
            else
                TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                    .cursor_position=&cursor,.focused=&focused,.commit_pressed=&commit,.focus_id=25900});
            ui_popup_input_end(parent);
            check_int("dismissal does not replay blocked editor text",
                strcmp(text,frame == 2 ? (queued == 2 ? "" : "ax") : "a"),0);
            if(queued == 2) check_int("dismissal does not replay queued Enter",commit,frame == 2);
            if(close_same_frame && frame == 0) ui_popup_input_close(context,0);
            EndUIFrame();
            ui_popup_input_finish(context);
            ui_popup_input_bind(previous);
        }
        ClearTextInputFocus();
        ui_popup_input_destroy(context);
    }
    InjectReset();
}

static void
test_popup_tab_missing_owner(void)
{
    UIPopupInput *context = ui_popup_input_create();
    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        if(frame) InjectKeyTap(KEY_TAB);
        InjectPump(); BeginUIFrame(240,240,1);
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        RegisterUIFocus(25800,(Rectangle){0});
        if(frame < 2) {
            UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
            RegisterUIFocus(25810,(Rectangle){0});
            if(frame == 0) {
                UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
                RegisterUIFocus(25820,(Rectangle){0});
                SetUIFocus(25820);
                ui_popup_input_end(child);
            }
            ui_popup_input_end(parent);
        }
        EndUIFocus();
        check_int("missing popup owner releases Tab in the same frame",GetUIFocus(),
            frame == 0 ? 25820 : frame == 1 ? 25810 : 25800);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        EndUIFrame();
        InjectPump();
    }
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_button_keyboard_ownership(void)
{
    const int keys[] = {KEY_ENTER,KEY_SPACE};
    for(int key = 0; key < 2; key++)
    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(keys[key]); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocusTextInputActive(0);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        BeginTree(Key("popup-button-keyboard"));
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25700);
        int activated = Button((ButtonProps){.bounds={10,10,120,28},.label="Action",.id=25700});
        check_int("only top popup button activates from keyboard",activated,inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        UIEvent event;
        while(NextEvent(&event)) {}
        EndTree();
        int clicks = 0;
        while(NextEvent(&event)) if(event.kind == UI_EVENT_CLICK) clicks++;
        check_int("deferred keyboard clicks preserve popup ownership",clicks,inside);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_choice_keyboard_ownership(void)
{
    for(int inside = 0; inside < 2; inside++) {
        int selected = 0;
        InjectReset(); InjectKeyTap(KEY_SPACE); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(
            context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(
            context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25705);
        int activated = Selectable((SelectableProps){
            .bounds={10,10,120,28},.label="Choice",.id=25705,
            .selected=&selected
        });
        check_int("only top popup choice activates from keyboard",
                  activated,inside);
        check_int("blocked popup choice preserves state",selected,inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_multi_select_keyboard_ownership(void)
{
    const char *items[] = {"Alpha","Beta"};
    for(int inside = 0; inside < 2; inside++) {
        int selected[] = {1,0};
        int count = 1;
        int anchor = 0;
        InjectReset(); InjectKeyTap(KEY_DOWN); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(
            context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(
            context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25706);
        int clicked = MultiSelectList((MultiSelectListProps){
            .bounds={10,10,120,56},.id=25706,.items=items,.item_count=2,
            .selected=selected,.selected_count=&count,.anchor=&anchor,.row_height=28
        });
        check_int("only top popup multi-select navigates",clicked,inside ? 1 : -1);
        check_int("blocked popup multi-select preserves anchor",anchor,inside ? 1 : 0);
        check_int("blocked popup multi-select preserves selection",selected[1],inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_drag_keyboard_ownership(void)
{
    for(int inside = 0; inside < 2; inside++) {
        float value = 1.0f;
        InjectReset(); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(
            context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(
            context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25707);
        int changed = test_drag_float((UIFloatDragProps){
            .bounds={10,10,120,28},.id=25707,.values=&value,.value_count=1,
            .speed=1,.min=0,.max=10
        });
        check_int("only top popup drag changes from keyboard",changed,inside);
        check_int("blocked popup drag preserves value",(int)value,inside ? 2 : 1);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_tab_ownership(void)
{
    const int start[] = {25620,25621,25620,25600,25620,25620};
    const int want[] = {25621,25621,25621,25621,25611,25600};
    for(int mode = 0; mode < 6; mode++) {
        InjectReset();
        if(mode == 2 || mode == 3) InjectKey(KEY_LEFT_SHIFT,1);
        InjectKeyTap(KEY_TAB); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocus(start[mode]);
        RegisterUIFocus(25600,(Rectangle){0});
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        RegisterUIFocus(25610,(Rectangle){0});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){20,20,60,60});
        RegisterUIFocus(25620,(Rectangle){0});
        RegisterUIFocus(25621,(Rectangle){0});
        RegisterUIFocus(25620,(Rectangle){0});
        ui_popup_input_end(child);
        RegisterUIFocus(25611,(Rectangle){0});
        ui_popup_input_end(parent);
        RegisterUIFocus(25601,(Rectangle){0});
        if(mode == 4) ui_popup_input_close(context,1);
        if(mode == 5) ui_popup_input_close(context,0);
        EndUIFocus();
        check_int("popup Tab ownership and wraparound",GetUIFocus(),want[mode]);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_popup_text_keyboard_ownership(void)
{
    for(int retained = 0; retained < 2; retained++)
    for(int area = 0; area < 2; area++)
    for(int inside = 0; inside < 2; inside++) {
        char text[32] = "a";
        int cursor = 1, focused = 1;
        InjectReset(); ClearTextInputFocus(); InjectText("x"); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        if(retained) BeginTree(Key("popup-editor-keyboard"));
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(25500);
        if(area)
            TextArea((TextAreaProps){.bounds={10,10,120,80},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=25500});
        else
            TextField((TextFieldProps){.bounds={10,10,120,28},.text=text,.text_size=sizeof(text),
                .cursor_position=&cursor,.focused=&focused,.focus_id=25500});
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        if(retained) EndTree();
        check_int("only top popup editor receives typing",strcmp(text,inside ? "ax" : "a"),0);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        ClearTextInputFocus();
        EndUIFrame();
    }
    InjectReset();
}

static void
test_text_area_page_navigation(void)
{
    char text[64] = "a0\nb1\nc2\nd3\ne4\nf5";
    int cursor = 4;
    int focused = 1;
    TextAreaProps area = {
        .bounds = {10,10,160,60}, .text = text, .text_size = sizeof(text),
        .cursor_position = &cursor, .focused = &focused,
        .focus_id = 25510, .font = Text16
    };

    InjectReset();
    SetUIFocus(area.focus_id);
    InjectKeyTap(KEY_DOWN); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea Down advances one line",cursor > 4,1);

    int before_page = cursor;
    InjectKeyTap(KEY_PAGE_DOWN); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea PageDown moves more than one line",cursor >= before_page + 4,1);

    before_page = cursor;
    InjectKeyTap(KEY_PAGE_UP); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea PageUp moves more than one line",cursor <= before_page - 4,1);

    int selection_start = 0;
    int selection_end = 0;
    before_page = cursor;
    InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_PAGE_DOWN); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    check_int("TextArea Shift+PageDown keeps anchor",
              GetTextAreaSelection(area.focus_id, &selection_start,
                                   &selection_end),1);
    check_int("TextArea Shift+PageDown selection start",
              selection_start,before_page);
    check_int("TextArea Shift+PageDown selection end",selection_end,cursor);

    InjectKeyTap(KEY_LEFT); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea Left collapses selection",cursor,before_page);
    check_int("TextArea collapsed selection is empty",
              GetTextAreaSelection(area.focus_id, NULL, NULL),0);

    InjectKeyTap(KEY_HOME); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea Home moves to line start",cursor,6);
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_END); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    check_int("TextArea Ctrl+End moves to buffer end",cursor,(int)strlen(text));

    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_LEFT); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea Ctrl+Left moves by word",cursor,15);
    InjectPump();
    InjectKey(KEY_LEFT_SHIFT,1); InjectKeyTap(KEY_LEFT); InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea Ctrl+Shift+Left moves by separator",cursor,14);
    check_int("TextArea Ctrl+Shift+Left keeps anchor",
              GetTextAreaSelection(area.focus_id, &selection_start,
                                   &selection_end),1);
    check_int("TextArea word selection start",selection_start,14);
    check_int("TextArea word selection end",selection_end,15);
    InjectKey(KEY_LEFT_SHIFT,0); InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    ClearTextInputFocus();
    InjectReset();
}

static void
test_secure_text_field_word_navigation(void)
{
    char text[32] = "alpha beta";
    int cursor = 5;
    int focused = 1;
    TextFieldProps field = {
        .bounds = {10,10,180,32}, .text = text, .text_size = sizeof(text),
        .cursor_position = &cursor, .focused = &focused,
        .focus_id = 25511, .secure = 1
    };

    InjectReset();
    SetUIFocus(field.focus_id);
    InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_LEFT); InjectPump();
    BeginUIFrame(240,100,1); TextField(field); EndUIFrame();
    check_int("secure TextField Ctrl+Left hides word boundaries",cursor,0);

    InjectPump(); InjectKeyTap(KEY_RIGHT); InjectPump();
    BeginUIFrame(240,100,1); TextField(field); EndUIFrame();
    check_int("secure TextField Ctrl+Right hides word boundaries",
              cursor,(int)strlen(text));

    InjectPump(); InjectKeyTap(KEY_BACKSPACE); InjectPump();
    BeginUIFrame(240,100,1); TextField(field); EndUIFrame();
    check_int("secure TextField Ctrl+Backspace clears opaque span",
              strcmp(text,""),0);
    InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    ClearTextInputFocus();
    InjectReset();
}

static void
test_immediate_text_composition(void)
{
    char field_text[64] = "ab";
    char area_text[64] = "aZZb";
    int field_cursor = 1;
    int area_cursor = 3;
    int field_focused = 1;
    int area_focused = 1;
    const char *preedit = NULL;
    int preedit_cursor = 0;
    int preedit_selection = 0;
    TextFieldProps field = {
        .bounds = {10,10,180,32}, .text = field_text,
        .text_size = sizeof(field_text), .cursor_position = &field_cursor,
        .focused = &field_focused, .focus_id = 25520
    };
    TextAreaProps area = {
        .bounds = {10,50,180,80}, .text = area_text,
        .text_size = sizeof(area_text), .cursor_position = &area_cursor,
        .focused = &area_focused, .focus_id = 25521
    };

    InjectReset();
    ClearTextInputFocus();
    field_focused = 1;
    SetUIFocus(field.focus_id);
    SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,
                          "\xE6\x97\xA5\xE6\x9C\xAC", 3, 0);
    BeginUIFrame(240,160,1);
    check_int("immediate TextField preedit is not committed",
              TextField(field), 0);
    EndUIFrame();
    check_int("immediate TextField preserves committed buffer",
              strcmp(field_text,"ab"), 0);
    check_int("immediate TextField owns shared preedit",
              ui_text_composition_get(&field_focused, &preedit,
                                      &preedit_cursor,
                                      &preedit_selection), 1);
    check_int("immediate TextField preedit UTF-8 cursor",
              preedit_cursor, 3);

    SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,
                          "\xE6\x97\xA5\xE6\x9C\xAC", 6, 0);
    BeginUIFrame(240,160,1);
    check_int("immediate TextField commit reports change",
              TextField(field), 1);
    EndUIFrame();
    check_int("immediate TextField commits UTF-8 at caret",
              strcmp(field_text,"a\xE6\x97\xA5\xE6\x9C\xAC" "b"), 0);
    check_int("immediate TextField clears preedit after commit",
              ui_text_composition_get(&field_focused, NULL, NULL, NULL), 0);

    ClearTextInputFocus();
    area_focused = 1;
    SetUIFocus(area.focus_id);
    SetTextAreaSelection(area.focus_id, 1, 3);
    SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,
                          "\xE3\x81\xAB", 3, 0);
    BeginUIFrame(240,160,1);
    check_int("immediate TextArea preedit is not committed",
              TextArea(area), 0);
    EndUIFrame();
    check_int("immediate TextArea preserves selected text during preedit",
              strcmp(area_text,"aZZb"), 0);

    SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,
                          "\xE6\x97\xA5\xE6\x9C\xAC", 6, 0);
    BeginUIFrame(240,160,1);
    check_int("immediate TextArea commit reports change",
              TextArea(area), 1);
    EndUIFrame();
    check_int("immediate TextArea composition replaces selection",
              strcmp(area_text,"a\xE6\x97\xA5\xE6\x9C\xAC" "b"), 0);

    SubmitTextComposition(KRY_TEXT_COMPOSITION_UPDATE,"blocked",7,0);
    BeginUIFrame(240,160,1);
    TextArea(area);
    EndUIFrame();
    area.read_only = 1;
    SubmitTextComposition(KRY_TEXT_COMPOSITION_COMMIT,"x",1,0);
    BeginUIFrame(240,160,1);
    check_int("read-only immediate editor ignores IME commit",
              TextArea(area), 0);
    EndUIFrame();
    check_int("read-only immediate editor cancels preedit",
              ui_text_composition_get(&area_focused, NULL, NULL, NULL), 0);
    check_int("read-only immediate editor does not mutate text",
              strcmp(area_text,"a\xE6\x97\xA5\xE6\x9C\xAC" "b"), 0);

    ClearTextInputFocus();
    InjectReset();
}

static void
test_text_area_wheel_scroll(void)
{
    char text[256] =
        "a0\nb1\nc2\nd3\ne4\nf5\ng6\nh7\ni8\nj9\nk10\nl11\nm12\nn13";
    int cursor = 0;
    int focused = 0;
    int scroll = 0;
    TextAreaProps area = {
        .bounds = {10,10,160,60}, .text = text, .text_size = sizeof(text),
        .cursor_position = &cursor, .focused = &focused,
        .scroll_y = &scroll, .focus_id = 25511, .font = Text16
    };

    InjectReset();
    InjectMousePosition(40, 40);
    InjectWheel(-1);
    InjectPump();
    BeginUIFrame(240,160,1); TextArea(area); EndUIFrame();
    check_int("TextArea wheel scrolls down", scroll > 0, 1);
    InjectReset();
}

static void
test_composed_popup_children_scope(void)
{
    bool open = true;
    char text[16] = "edit";
    int cursor = 4;
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup ordinary children"));
    Button((ButtonProps){.bounds={10,10,80,24},.label="Background",.id=26999});
    check_int("open composed popup returns true",
        BeginPopup((PopupProps){.bounds={10,40,120,80},.id=27000,.open=&open}),1);
    Column((ColumnProps){.bounds={12,72,100,60},.gap=3});
    Button((ButtonProps){.bounds={0,0,90,24},.label="Action",.id=27001});
    TextField((TextFieldProps){.bounds={0,0,90,24},.text=text,.text_size=sizeof(text),
        .cursor_position=&cursor,.focus_id=27002});
    End();
    EndPopup();
    Button((ButtonProps){.bounds={120,10,80,24},.label="After",.id=27003});
    EndTree();
    int count = 0, action = 0, field = 0, after = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) {
        if(nodes[i].id == 27001) {
            action++;
            check_int("composed layout detached from owner",
                nodes[nodes[i].parent].parent,0);
            check_int("composed child x",(int)nodes[i].bounds.x,12);
        } else if(nodes[i].id == 27002) field++;
        else if(nodes[i].id == 27003) after++;
    }
    check_int("ordinary button retained in composed popup",action,1);
    check_int("ordinary field retained in composed popup",field,1);
    check_int("parent declarations resume after popup",after,1);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup explicit close"));
    check_int("composed popup reopens from caller state",
        BeginPopup((PopupProps){.bounds={10,40,120,80},.id=27000,.open=&open}),1);
    ClosePopup();
    check_int("ClosePopup updates caller state",open,0);
    EndPopup();
    EndTree();
    EndUIFrame();
    BeginUIFrame(240,180,1);
    check_int("closed composed popup stays closed",
        BeginPopup((PopupProps){.bounds={10,40,120,80},.id=27000,.open=&open}),0);
    EndUIFrame();
}

static void
test_composed_popup_scope(void)
{
    bool open = true;
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup ordinary children"));
    check_int("open composed popup returns true",
        BeginPopup((PopupProps){.bounds={20,30,140,100},.id=29000,.open=&open}),1);
    Column((ColumnProps){.bounds={28,40,120,70},.gap=4});
    Button((ButtonProps){.bounds={0,0,100,28},.label="Apply",.id=29001});
    End();
    EndPopup();
    Button((ButtonProps){.bounds={160,30,70,28},.label="After",.id=29002});
    EndTree();
    int count = 0, child = 0, after = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) {
        if(nodes[i].id == 29001) {
            child++;
            check_int("composed popup child x",(int)nodes[i].bounds.x,28);
        } else if(nodes[i].id == 29002) after++;
    }
    check_int("ordinary button retained in composed popup",child,1);
    check_int("parent resumes after composed popup",after,1);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    BeginTree(Key("composed popup explicit close"));
    check_int("composed popup reopens from caller state",
        BeginPopup((PopupProps){.bounds={20,30,140,100},.id=29000,.open=&open}),1);
    ClosePopup();
    check_int("ClosePopup updates caller state",open,0);
    EndPopup();
    EndTree();
    EndUIFrame();

    BeginUIFrame(240,180,1);
    check_int("invalid composed popup stays closed",
        BeginPopup((PopupProps){.bounds={20,30,0,100},.id=29000,.open=&open}),0);
    EndUIFrame();
}

static void
test_composed_tooltip_scope(void)
{
    InjectReset();
    InjectMousePosition(30,25);
    InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed tooltip arbitrary children"));
    check_int("hovered tooltip popup opens",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29300,
            .trigger={20,20,80,30},.flags=PopupTooltip}),1);
    Column((ColumnProps){.bounds={88,58,114,54},.gap=4});
    Text((TextProps){.text="arbitrary tooltip",.font=Text14,
        .color=BLACK,.wrap=TextWrapNone});
    Button((ButtonProps){.bounds={0,0,90,24},.label="detail",.id=29301});
    End();
    check_int("tooltip does not capture popup input",
        ui_popup_input_current_captures((Vector2){90,60}),0);
    EndPopup();
    EndTree();
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) if(nodes[i].id == 29301) child++;
    check_int("ordinary child retained in tooltip",child,1);
    EndUIFrame();

    InjectMousePosition(230,170);
    InjectPump();
    BeginUIFrame(240,180,1);
    check_int("tooltip closes outside trigger",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29300,
            .trigger={20,20,80,30},.flags=PopupTooltip}),0);
    EndUIFrame();
}

static void
test_composed_modal_scope(void)
{
    bool open = true;
    InjectReset();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed modal arbitrary children"));
    check_int("open composed modal returns true",
        BeginPopup((PopupProps){.bounds={40,30,120,90},.id=29400,
            .open=&open,.flags=PopupModal}),1);
    Column((ColumnProps){.bounds={48,38,104,70},.gap=4});
    Text((TextProps){.text="arbitrary modal",.font=Text14,
        .color=BLACK,.wrap=TextWrapNone});
    Button((ButtonProps){.bounds={0,0,90,24},.label="confirm",.id=29401});
    End();
    EndPopup();
    Button((ButtonProps){.bounds={190,145,45,30},.label="Behind",.id=29402});
    EndTree();
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++) if(nodes[i].id == 29401) child++;
    check_int("ordinary child retained in modal",child,1);
    check_int("outside declaration leaves modal open",open,1);
    EndUIFrame();

    InjectKeyTap(KEY_ESCAPE);
    InjectPump();
    BeginUIFrame(240,180,1);
    check_int("Escape closes composed modal",
        BeginPopup((PopupProps){.bounds={40,30,120,90},.id=29400,
            .open=&open,.flags=PopupModal}),0);
    check_int("Escape updates modal caller state",open,0);
    EndUIFrame();
    InjectReset();
}

static void
test_composed_context_popup_scope(void)
{
    bool open = false;
    InjectReset();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,1); InjectPump();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,0); InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("composed context popup arbitrary children"));
    check_int("right release opens composed context popup",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext}),1);
    Button((ButtonProps){.bounds={88,58,100,24},.label="context child",
        .id=29501});
    EndPopup();
    EndTree();
    check_int("context popup updates caller open state",open,1);
    int count = 0, child = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    for(int i = 0; i < count; i++)
        if(nodes[i].id == 29501) child++;
    check_int("ordinary child retained in context popup",child,1);
    EndUIFrame();

    InjectTap(220,160); InjectPump(); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("outside release dismisses context popup",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext}),0);
    check_int("context popup dismissal updates caller",open,0);
    EndUIFrame();

    InjectReset();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,1); InjectPump();
    InjectMousePosition(30,25);
    InjectMouseButton(MOUSE_BUTTON_RIGHT,0); InjectPump();
    BeginUIFrame(240,180,1);
    check_int("disabled context popup stays closed",
        BeginPopup((PopupProps){.bounds={80,50,130,70},.id=29500,
            .open=&open,.trigger={20,20,80,30},.flags=PopupContext,
            .disabled=1}),0);
    EndUIFrame();
    InjectReset();
}

static void
test_composed_popup_focus_lifecycle(void)
{
    bool parent_open = true, child_open = false;
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous;
    InjectReset();
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    SetUIFocus(29600);
    BeginTree(Key("composed popup focus acquisition"));
    check_int("focus parent popup opens",
        BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
            .open=&parent_open}),1);
    BeginDisabled(1);
    Button((ButtonProps){.bounds={30,30,100,24},.label="Disabled",.id=29612});
    EndDisabled();
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    EndPopup();
    EndTree();
    check_int("parent popup acquires first child focus",GetUIFocus(),29611);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    child_open = true;
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed nested popup focus"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    BeginPopup((PopupProps){.bounds={50,60,130,80},.id=29620,
        .open=&child_open});
    Button((ButtonProps){.bounds={60,70,100,24},.label="Child",.id=29621});
    EndPopup();
    EndPopup();
    EndTree();
    check_int("nested popup acquires first child focus",GetUIFocus(),29621);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed nested popup focus restore"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    BeginPopup((PopupProps){.bounds={50,60,130,80},.id=29620,
        .open=&child_open});
    Button((ButtonProps){.bounds={60,70,100,24},.label="Child",.id=29621});
    ClosePopup();
    check_int("nested popup restores parent focus",GetUIFocus(),29611);
    EndPopup();
    EndPopup();
    EndTree();
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed parent popup focus restore"));
    BeginPopup((PopupProps){.bounds={20,20,180,130},.id=29610,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,100,24},.label="Parent",.id=29611});
    ClosePopup();
    check_int("parent popup restores background focus",GetUIFocus(),29600);
    EndPopup();
    EndTree();
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    parent_open = true;
    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    SetUIFocus(29700);
    BeginTree(Key("composed missing popup owner focus"));
    BeginPopup((PopupProps){.bounds={20,20,120,80},.id=29710,
        .open=&parent_open});
    Button((ButtonProps){.bounds={30,30,90,24},.label="Popup",.id=29711});
    EndPopup();
    EndTree();
    check_int("popup before missing owner has child focus",GetUIFocus(),29711);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    BeginTree(Key("composed missing popup owner restore"));
    EndTree();
    ui_popup_input_finish(context);
    check_int("missing popup owner restores background focus",GetUIFocus(),29700);
    ui_popup_input_bind(previous);
    EndUIFrame();
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_active_drag_ownership(void)
{
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous;
    UIPopupInputToken owner;
    float drag_value = 10.0f;
    float background_value = 10.0f;
    float slider_value = 0.0f;
    int split = 50;
    UIFloatDragProps drag = {.bounds={30,30,80,24},.id=401,
        .values=&drag_value,.value_count=1,.speed=1.0f,.min=0,.max=500};
    UIFloatSliderProps slider = {.bounds={30,30,80,24},.id=411,
        .values=&slider_value,.value_count=1,.min=0,.max=100};
    UIFloatDragProps background_drag = {.bounds={30,30,80,24},.id=391,
        .values=&background_value,.value_count=1,.speed=1.0f,.min=0,.max=500};
    const char *columns[] = {"A","B"};
    const char *cells[] = {"a","b"};
    TableRow rows[] = {{cells,2,NULL,NULL}};
    int widths[] = {70,70};
    TableViewProps table = {0};
    PanedViewProps panes = {{30,30,100,80},416,1,&split,20,20};

    table.bounds = (Rectangle){25,25,140,90};
    table.id = 421;
    table.columns = columns;
    table.column_count = 2;
    table.rows = rows;
    table.row_count = 1;
    table.column_widths = widths;
    table.resizable = 1;
    table.min_column_width = 32;

    InjectReset();
    InjectMousePosition(40,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)test_drag_float(background_drag);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(80,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,390,(Rectangle){20,20,120,100});
    ui_popup_input_end(owner);
    check_int("new popup cancels background drag",test_drag_float(background_drag),0);
    check_int("new popup blocks background drag mutation",(int)background_value,10);
    ui_popup_input_close(context,390);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();

    InjectReset();
    InjectMousePosition(40,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,400,(Rectangle){20,20,120,100});
    (void)test_drag_float(drag);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(200,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,400,(Rectangle){20,20,120,100});
    check_int("popup drag continues outside bounds",test_drag_float(drag),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup drag outside value",(int)drag_value,170);

    InjectMousePosition(230,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    check_int("missing popup cancels active drag",test_drag_float(drag),0);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup drag does not mutate background",(int)drag_value,170);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(50,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,410,(Rectangle){20,20,120,100});
    (void)test_slider_float(slider);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(90,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,410,(Rectangle){20,20,120,100});
    (void)test_slider_float(slider);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup slider follows owned drag",(int)slider_value,75);

    InjectMousePosition(30,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)test_slider_float(slider);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup slider does not mutate background",(int)slider_value,75);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(80,40);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,415,(Rectangle){20,20,120,100});
    (void)PanedView(panes);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(110,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,415,(Rectangle){20,20,120,100});
    check_int("popup splitter follows owned drag",PanedView(panes),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup splitter value",split,80);

    InjectMousePosition(60,40);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)PanedView(panes);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup splitter does not mutate background",split,80);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    InjectMousePosition(93,35);
    InjectMouseButton(MOUSE_BUTTON_LEFT,1);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,420,(Rectangle){20,20,160,110});
    (void)TableView(table);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();

    InjectMousePosition(123,35);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    owner = ui_popup_input_begin(context,420,(Rectangle){20,20,160,110});
    check_int("popup table resize follows owned drag",TableView(table),1);
    ui_popup_input_end(owner);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("popup table resize width",widths[0],100);

    InjectMousePosition(153,35);
    InjectPump();
    BeginUIFrame(300,200,1);
    ui_popup_input_frame(context);
    previous = ui_popup_input_bind(context);
    (void)TableView(table);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    EndUIFrame();
    check_int("dismissed popup resize does not mutate background",widths[0],100);

    InjectMouseButton(MOUSE_BUTTON_LEFT,0);
    InjectPump();
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_dropdown_keyboard_ownership(void)
{
    const char *options[] = {"One","Two"};
    for(int inside = 0; inside < 2; inside++) {
        InjectReset(); InjectKeyTap(KEY_SPACE); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,0,(Rectangle){180,180,40,40});
        UIPopupInputToken child = ui_popup_input_begin(context,1,(Rectangle){190,190,20,20});
        if(!inside) ui_popup_input_end(child);
        check_int("keyboard capture is independent of pointer bounds",ui_popup_input_keyboard_captures(),!inside);
        int selected = 0;
        SetUIFocus(25400);
        Dropdown((DropdownProps){.bounds={10,10,100,28},.id=25400,
            .options=options,.option_count=2,.selected_index=&selected});
        check_int("only top popup may open a focused dropdown",dropdown_captures((Vector2){20,60}),inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_close(context,1);
        check_int("child dismissal restores parent keyboard",ui_popup_input_keyboard_captures(),0);
        ui_popup_input_end(parent);
        check_int("parent still captures background keyboard",ui_popup_input_keyboard_captures(),1);
        ui_popup_input_close(context,0);
        check_int("branch dismissal restores background keyboard",ui_popup_input_keyboard_captures(),0);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        dropdown_close(25400);
        EndUIFrame();
    }
    for(int inside = 0; inside < 2; inside++) {
        UIPopupInput *context;
        UIPopupInput *previous;
        UIPopupInputToken parent;
        UIPopupInputToken child;
        int selected = 0;

        InjectReset();
        InjectKeyTap(KEY_SPACE);
        InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocus(25400);
        (void)Dropdown((DropdownProps){
            .bounds={10,10,100,28},
            .id=25400,
            .options=options,
            .option_count=2,
            .selected_index=&selected
        });
        EndUIFrame();
        check_int("dropdown opens before Escape ownership test",
                  dropdown_captures((Vector2){20,50}),1);

        InjectKeyTap(KEY_ESCAPE);
        InjectPump();
        BeginUIFrame(240,240,1);
        context = ui_popup_input_create();
        ui_popup_input_frame(context);
        previous = ui_popup_input_bind(context);
        parent = ui_popup_input_begin(
            context,25700,(Rectangle){180,180,40,40});
        child = ui_popup_input_begin(
            context,25701,(Rectangle){190,190,20,20});
        if(!inside)
            ui_popup_input_end(child);
        SetUIFocus(25400);
        (void)Dropdown((DropdownProps){
            .bounds={10,10,100,28},
            .id=25400,
            .options=options,
            .option_count=2,
            .selected_index=&selected
        });
        if(inside)
            ui_popup_input_end(child);
        ui_popup_input_end(parent);
        RenderFrameOverlays();
        check_int("obscured dropdown ignores Escape",
                  dropdown_captures((Vector2){20,50}),!inside);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
        dropdown_close(25400);
    }
    InjectReset();
}

static void
test_popup_accelerator_keyboard_ownership(void)
{
    Accelerator copy = {KEY_C,1,0,0,91};
    Accelerator commands[] = {{KEY_X,1,0,0,90}, {KEY_C,1,0,0,91}};
    InjectReset(); InjectKey(KEY_LEFT_CONTROL,1); InjectKeyTap(KEY_C); InjectPump();
    BeginUIFrame(240,240,1);
    UIPopupInput *context = ui_popup_input_create();
    ui_popup_input_frame(context);
    UIPopupInput *previous = ui_popup_input_bind(context);
    UIPopupInputToken parent = ui_popup_input_begin(context,26000,(Rectangle){10,10,120,100});
    UIPopupInputToken child = ui_popup_input_begin(context,26001,(Rectangle){20,20,80,60});

    ui_popup_input_end(child);
    check_int("parent accelerator blocked behind child",AcceleratorPressed(copy),0);
    child = ui_popup_input_begin(context,26001,(Rectangle){20,20,80,60});
    check_int("top popup accelerator dispatch",DispatchAccelerators(commands,2),91);
    ui_popup_input_end(child);
    ui_popup_input_close(context,26001);
    check_int("parent accelerator restored after child close",AcceleratorPressed(copy),91);
    ui_popup_input_end(parent);
    check_int("background accelerator blocked behind parent",AcceleratorPressed(copy),0);
    ui_popup_input_close(context,26000);
    check_int("background accelerator restored after popup close",AcceleratorPressed(copy),91);
    BeginDisabled(1);
    check_int("disabled accelerator blocked",AcceleratorPressed(copy),0);
    EndDisabled();

    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(context);
    EndUIFrame();
    InjectKey(KEY_LEFT_CONTROL,0);
    InjectReset();
}

static void
test_popup_collapsible_keyboard_ownership(void)
{
    for(int inside = 0; inside < 2; inside++) {
        bool open = false;
        InjectReset(); InjectKeyTap(KEY_RIGHT); InjectPump();
        BeginUIFrame(240,240,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,26100,(Rectangle){10,10,120,100});
        UIPopupInputToken child = ui_popup_input_begin(context,26101,(Rectangle){20,20,80,60});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(26110);
        Collapsible((CollapsibleProps){.bounds={20,20,80,28},.id=26110,
                    .label="Node",.open=&open,.tree=1});
        check_int("only top popup collapsible handles keyboard",open,inside);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_retained_popup_pointer_focus(void)
{
    for(int blocked = 0; blocked < 2; blocked++) {
        InjectReset(); InjectTap(60,60); InjectPump();
        BeginUIFrame(240,240,1);
        SetUIFocus(0);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        PushUIInputCapture((Rectangle){0,0,blocked ? 5 : 240,240},1);
        BeginTree(Key("retained-popup-pointer-focus"));
        UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
        Row((RowProps){.bounds={50,50,80,24}});
        Button((ButtonProps){.bounds={0,0,40,24},.id=25301,.label="Child"});
        End();
        ui_popup_input_end(inner);
        Button((ButtonProps){.bounds={50,50,40,24},.id=25302,.label="Parent"});
        ui_popup_input_end(outer);
        Button((ButtonProps){.bounds={50,50,40,24},.id=25303,.label="Background"});
        check_int("immediate popup focus preserves modal blocking",GetUIFocus(),blocked ? 0 : 25301);
        /* Exercise the deferred focus pass independently of the immediate
         * button check; the popup scopes are already closed here. */
        SetUIFocus(0);
        UIEvent event;
        while(NextEvent(&event)) {}
        EndTree();
        check_int("deferred popup focus preserves modal blocking",GetUIFocus(),blocked ? 0 : 25301);
        int clicks = 0;
        while(NextEvent(&event)) {
            if(event.kind != UI_EVENT_CLICK) continue;
            check_int("deferred click belongs to popup child",(int)event.key,25301);
            clicks++;
        }
        check_int("modal blocker prevents deferred popup clicks",clicks,blocked ? 0 : 1);
        ClearUIInputCaptures();
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_card_props_retained_input(void)
{
    UIEvent event;
    int clicks;
    int count;
    const UIWidgetNode *nodes;
    NodeId card;

    InjectReset();
    InjectTap(30,30);
    InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("passive-card-does-not-activate"));
    check_int("passive card ignores click",
        Card((CardProps){.bounds={10,10,80,60},.id=27601}),0);
    EndTree();
    clicks = 0;
    while(NextEvent(&event)) if(event.kind == UI_EVENT_CLICK) clicks++;
    check_int("passive card posts no click events",clicks,0);
    EndUIFrame();

    InjectReset();
    InjectTap(30,30);
    InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("clickable-card-activates"));
    check_int("clickable card press waits for release",
        Card((CardProps){.bounds={10,10,80,60},.id=27602,.clickable=true}),0);
    EndTree();
    EndUIFrame();
    InjectPump();
    BeginUIFrame(240,180,1);
    BeginTree(Key("clickable-card-activates"));
    check_int("clickable card activates on single click release",
        Card((CardProps){.bounds={10,10,80,60},.id=27602,.clickable=true}),1);
    EndTree();
    clicks = 0;
    while(NextEvent(&event)) {
        if(event.kind != UI_EVENT_CLICK) continue;
        check_int("clickable card event key",(int)event.key,27602);
        clicks++;
    }
    check_int("clickable card posts one event",clicks,1);
    EndUIFrame();

    BeginUIFrame(240,180,1);
    BeginTree(Key("card-content-scope"));
    card = BeginCard((CardProps){.bounds={10,10,120,80}});
    Text((TextProps){.text="Inside",.font=Text16});
    End();
    EndTree();
    nodes = GetTreeNodes(&count);
    check_int("card tree node count",count,3);
    check_int("card node kind",nodes[1].kind,UI_WIDGET_CARD_NODE);
    check_int("card child parent",nodes[2].parent,card);
    EndUIFrame();
}

static void
test_retained_popup_input_ownership(void)
{
    InjectReset();
    BeginUIFrame(240,240,1);
    BeginTree(Key("retained-popup-input"));
    Button((ButtonProps){.bounds={50,50,40,24},.id=25200,.label="Before"});
    UIPopupInput *context = ui_popup_input_create();
    ui_popup_input_frame(context);
    UIPopupInput *previous = ui_popup_input_bind(context);
    UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
    UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
    Button((ButtonProps){.bounds={50,50,40,24},.id=25201,.label="Child"});
    ui_popup_input_end(inner);
    Button((ButtonProps){.bounds={50,50,40,24},.id=25202,.label="Later parent"});
    ui_popup_input_end(outer);
    Button((ButtonProps){.bounds={50,50,40,24},.id=25203,.label="After"});
    EndTree();
    const UIWidgetNode *node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained child beats later parent and background",node ? node->id : -1,25201);
    NodeId child_hit = HitTestNode((Vector2){60,60});
    BeginTree(Key("replacement-popup-tree"));
    check_int("pending declaration preserves committed popup ownership",
        HitTestNode((Vector2){60,60}),child_hit);
    check_int("deferred hit test does not reopen input scope",ui_popup_input_snapshot().order,0);
    ui_popup_input_close(context,1);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained parent receives input after child closes",node ? node->id : -1,25202);
    ui_popup_input_close(context,0);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("retained background receives input after branch closes",node ? node->id : -1,25203);
    ui_popup_input_finish(context);
    ui_popup_input_frame(context);
    check_int("previous-frame retained owners reject input",HitTestNode((Vector2){60,60}) <= 1,1);
    ui_popup_input_finish(context);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(context);
    node = GetNode(HitTestNode((Vector2){60,60}));
    check_int("destroyed registry snapshots reject input safely",node ? node->id : -1,25200);
    EndTree();
    EndUIFrame();
}

static void
test_nested_popup_input_ownership(void)
{
    UIPopupInput *context = ui_popup_input_create();
    UIPopupInput *previous = ui_popup_input_bind(context);
    int background = 0, parent = 0, child = 0;
    InjectReset();
    for(int frame = 0; frame < 4; frame++) {
        if(frame == 1) InjectTap(60,60);
        InjectPump(); BeginUIFrame(240,240,1);
        ui_popup_input_frame(context);
        background += Button((ButtonProps){.bounds={50,50,40,24},.id=25100,.label="Before"});
        UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
        parent += Button((ButtonProps){.bounds={50,50,40,24},.id=25101,.label="Parent"});
        UIPopupInputToken inner = ui_popup_input_begin(context,1,(Rectangle){50,50,60,60});
        child += Button((ButtonProps){.bounds={50,50,40,24},.id=25102,.label="Child"});
        ui_popup_input_end(inner);
        ui_popup_input_end(outer);
        background += Button((ButtonProps){.bounds={50,50,40,24},.id=25103,.label="After"});
        ui_popup_input_finish(context);
        EndUIFrame();
    }
    check_int("popup background controls blocked",background,0);
    check_int("popup parent cannot steal child input",parent,0);
    check_int("ordinary popup child button activates",child,1);
    ui_popup_input_frame(context);
    UIPopupInputToken outer = ui_popup_input_begin(context,0,(Rectangle){10,10,120,120});
    check_int("previous-frame child remains above parent",ui_popup_input_captures(context,(Vector2){60,60}),1);
    ui_popup_input_close(context,1);
    check_int("closing child restores parent input",ui_popup_input_captures(context,(Vector2){60,60}),0);
    ui_popup_input_end(outer);
    ui_popup_input_finish(context);
    UIPopupInput *other = ui_popup_input_create();
    ui_popup_input_bind(other);
    check_int("popup capture remains context-local",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_bind(context);
    check_int("rebinding restores popup capture",ui_popup_input_current_captures((Vector2){60,60}),1);
    ui_popup_input_frame(context);
    ui_popup_input_finish(context);
    check_int("missing popup owner retired",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_frame(context);
    outer = ui_popup_input_begin(context,0,(Rectangle){0,0,100,100});
    UIPopupInputToken nested = ui_popup_input_begin(context,1,(Rectangle){0,0,100,100});
    ui_popup_input_end(nested); ui_popup_input_end(outer);
    UIPopupInputToken sibling = ui_popup_input_begin(context,2,(Rectangle){0,0,100,100});
    check_int("later sibling beats earlier nested branch",ui_popup_input_captures(context,(Vector2){60,60}),0);
    nested = ui_popup_input_begin(context,3,(Rectangle){0,0,100,100});
    ui_popup_input_end(nested); ui_popup_input_end(sibling);
    ui_popup_input_finish(context);
    ui_popup_input_frame(context);
    outer = ui_popup_input_begin(context,0,(Rectangle){0,0,100,100});
    ui_popup_input_close(context,1);
    check_int("reordered root beats other branch descendants",ui_popup_input_captures(context,(Vector2){60,60}),0);
    nested = ui_popup_input_begin(context,1,(Rectangle){0,0,100,100});
    ui_popup_input_close(context,0);
    check_int("closing parent disables active child",ui_popup_input_captures(context,(Vector2){60,60}),1);
    ui_popup_input_end(nested); ui_popup_input_end(outer);
    ui_popup_input_finish(context);
    check_int("closed and missing branches retired",ui_popup_input_current_captures((Vector2){60,60}),0);
    ui_popup_input_bind(previous);
    ui_popup_input_destroy(other);
    ui_popup_input_destroy(context);
    InjectReset();
}

static void
test_popup_input_clip_restoration(void)
{
    for(int blocked = 0; blocked < 2; blocked++) {
        int popup_actions = 0, parent_actions = 0;
        InjectReset(); InjectTap(20,20);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            BeginTree(Key("popup input clip"));
            PushUIInputCapture((Rectangle){0,0,blocked ? 5 : 100,100},1);
            BeginScroll((Rectangle){0,0,1,1},100,NULL);
            UIInputClipScope scope = ui_input_clip_suspend();
            EndScroll(); /* Cannot pop the suspended owner's scroll scope. */
            popup_actions += Button((ButtonProps){.bounds={10,10,40,20},.id=25004,.label="Popup"});
            ui_input_clip_resume(scope);
            parent_actions += Button((ButtonProps){.bounds={10,10,40,20},.id=25005,.label="Clipped"});
            EndScroll();
            EndTree(); ClearUIInputCaptures(); EndUIFrame();
        }
        check_int("popup input escapes owner clip but respects capture",popup_actions,!blocked);
        check_int("owner input clip restored",parent_actions,0);
    }
    InjectReset();
}

static void
test_popup_disabled_restoration(void)
{
    for(int disabled = 0; disabled < 2; disabled++) {
        BeginDisabled(disabled);
        UIDisabledScope outer = ui_disabled_suspend();
        EndDisabled();
        check_int("popup cannot end parent disabled scope",UIContentDisabled(),disabled);
        BeginDisabled(1);
        UIDisabledScope inner = ui_disabled_suspend();
        BeginDisabled(0); EndDisabled();
        check_int("nested popup inherits disabled",UIContentDisabled(),1);
        ui_disabled_resume(inner);
        EndDisabled();
        check_int("popup child disabling restored",UIContentDisabled(),disabled);
        ui_disabled_resume(outer);
        check_int("popup parent disabling restored",UIContentDisabled(),disabled);
        EndDisabled();
        check_int("parent disabled scope remains balanced",UIContentDisabled(),0);
    }
}

static void
test_popup_layout_restoration(void)
{
    BeginTree(Key("popup layout restoration"));
    NodeId parent = Row((RowProps){.bounds={10,10,200,20},.gap=5});
    Rect(0,0,20,20,RED,BLANK);
    UITreeLayoutScope scope = ui_tree_layout_suspend();
    NodeId popup = Row((RowProps){.bounds={0,0,100,20},.gap=3});
    Rect(0,0,30,20,BLUE,BLANK);
    End();
    ui_tree_layout_resume(scope);
    Rect(0,0,20,20,GREEN,BLANK);
    End();
    EndTree();
    int count = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    check_int("isolated popup tree count",count,6);
    if(count != 6) return;
    check_int("popup attached to screen root",nodes[popup].parent,0);
    check_int("popup child layout origin",(int)nodes[4].bounds.x,0);
    check_int("parent child before popup",(int)nodes[2].bounds.x,10);
    check_int("parent child after popup",(int)nodes[5].bounds.x,35);
    check_int("parent relationship restored",nodes[5].parent,parent);
}

static void
test_dropdown_horizontal_viewport(void)
{
    const Rectangle bounds[] = {{-20,10,160,28},{200,10,160,28},{10,10,400,28}};
    const char *options[] = {"One","Two"};
    for(int i = 0; i < 3; i++) {
        int selected = 0;
        InjectReset(); InjectKeyTap(KEY_SPACE);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1); SetUIFocus(25002);
            Dropdown((DropdownProps){.bounds=bounds[i],.id=25002,.options=options,.option_count=2,.selected_index=&selected});
            EndUIFrame();
        }
        int x = i == 1 ? 92 : 12;
        check_int("shifted popup captures row",dropdown_captures((Vector2){x,80}),1);
        check_int("popup left edge bounded",dropdown_captures((Vector2){-1,80}),0);
        check_int("popup right edge bounded",dropdown_captures((Vector2){241,80}),0);
        InjectTap(x,80);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            Dropdown((DropdownProps){.bounds=bounds[i],.id=25002,.options=options,.option_count=2,.selected_index=&selected});
            EndUIFrame();
        }
        check_int("shifted popup selected second row",selected,1);
        InjectReset(); BeginUIFrame(240,240,1); EndUIFrame();
    }
}

static void
test_dropdown_scrollbar_dismissal(void)
{
    const char *options[131];
    for(int i = 0; i < 131; i++) options[i] = "item";
    for(int mode = 0; mode < 3; mode++) {
        int selected = 0;
        InjectReset();
        InjectKeyTap(KEY_SPACE);
        for(int frame = 0; frame < 3; frame++) {
            InjectPump(); BeginUIFrame(240,240,1);
            SetUIFocus(25001);
            Dropdown((DropdownProps){.bounds={10,10,160,28},.id=25001,
                .options=options,.option_count=131,.selected_index=&selected});
            EndUIFrame();
        }
        InjectMousePosition(166,50); InjectMouseButton(MOUSE_BUTTON_LEFT,1);
        InjectPump(); BeginUIFrame(240,240,1);
        Dropdown((DropdownProps){.bounds={10,10,160,28},.id=25001,
            .options=options,.option_count=131,.selected_index=&selected});
        EndUIFrame();
        check_int("dropdown scrollbar acquired drag",g_ui_pointer_owner,UI_POINTER_OWNER_SCROLL);
        if(mode == 0) InjectKeyTap(KEY_ESCAPE);
        InjectPump(); BeginUIFrame(240,240,1);
        if(mode != 2)
            Dropdown((DropdownProps){.bounds={10,10,160,28},.id=25001,
                .options=options,.option_count=131,.selected_index=&selected,.disabled=mode==1});
        EndUIFrame();
        check_int("dismissed dropdown scrollbar released drag",g_ui_pointer_owner,UI_POINTER_OWNER_NONE);
        check_int("dismissed dropdown scrollbar released capture",dropdown_captures((Vector2){20,70}),0);
        InjectReset(); BeginUIFrame(240,240,1); EndUIFrame();
    }
}

static void
test_dropdown_keyboard_open(void)
{
    const char *options[] = {"One", "Two"};
    const int keys[] = {KEY_ENTER, KEY_KP_ENTER, KEY_SPACE, KEY_DOWN};
    for(int key = 0; key < 4; key++) {
        for(int mode = 0; mode < 3; mode++) {
            int selected = 1;
            InjectReset();
            InjectKeyTap(keys[key]);
            for(int frame = 0; frame < 3; frame++) {
                InjectPump();
                BeginUIFrame(240,240,1);
                SetUIFocus(24000);
                BeginDisabled(mode == 2);
                Dropdown((DropdownProps){.bounds = {10,10,160,28}, .id = 24000,
                    .options = options, .option_count = 2, .selected_index = &selected,
                    .disabled = mode == 1});
                EndDisabled();
                EndUIFrame();
            }
            check_int("focused dropdown keyboard opening",UIInputCapturesClick((Vector2){20,70}),mode == 0);
            check_int("opening key does not commit or move selection",selected,1);
            InjectReset();
            BeginUIFrame(240,240,1);
            EndUIFrame();
        }
    }
}

static void
test_custom_table_cell_scope(void)
{
    const char *columns[] = {"A","B"};
    TableRow rows[3] = {{0}};
    int order[] = {1,0}, scroll = 20, actions = 0;
    TableViewProps p = {.bounds = {10,10,200,90}, .columns = columns, .column_count = 2,
                       .rows = rows, .row_count = 3, .column_order = order, .row_height = 30,
                       .freeze_rows = 1, .scroll_offset = &scroll, .custom_cells = 1};
    InjectReset(); InjectTap(120,75);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump(); BeginUIFrame(300,200,1.0f); TableView(p);
        Rectangle cell = BeginTableCell(p,1,0);
        check_int("custom cell reordered x",(int)cell.x,110);
        check_int("custom cell scrolling y",(int)cell.y,50);
        check_int("custom cell frozen clip",UIInputCapturesClick((Vector2){120,60}),1);
        if(Button((ButtonProps){.bounds = cell,.label = "Child",.id = 1000})) actions++;
        EndTableCell();
        p.disabled = 1; BeginTableCell(p,0,1);
        check_int("custom cell disabled",UIContentDisabled(),1);
        EndTableCell(); p.disabled = 0;
        check_int("custom cell disabled restored",UIContentDisabled(),0);
        EndUIFrame();
    }
    check_int("custom cell child action",actions,1);
}

static void
test_retained_scope_clip(void)
{
    InjectReset();
    BeginUIFrame(200,120,1.0f);
    BeginTree(Key("retained-scope-clip"));
    BeginScroll((Rectangle){10,10,50,30},30,NULL);
    Row((RowProps){.bounds = {10,10,100,30}});
    Button((ButtonProps){.bounds = {0,0,100,30},.label = "Clipped",.id = 1005});
    End(); EndScroll(); EndTree();
    NodeId inside = HitTestNode((Vector2){20,20});
    const UIWidgetNode *node = GetNode(inside);
    check_int("retained cell hit",node != NULL ? node->id : -1,1005);
    check_int("retained clip captured",node->has_input_clip,1);
    check_int("retained clip width",(int)node->input_clip.width,50);
    NodeId outside = HitTestNode((Vector2){80,20});
    check_int("retained clip rejects outside",outside == inside,0);
    EndUIFrame();
}

static void
test_list_box_scope(void)
{
    for(int disabled = 0; disabled < 2; disabled++) {
        int offset = 0;
        InjectReset(); InjectMousePosition(30,30); InjectWheel(-1); InjectPump();
        BeginUIFrame(200,150,1.0f);
        BeginDisabled(disabled);
        Rectangle content = BeginScroll((Rectangle){21,21,118,78}, Scale(100), &offset);
        check_int("list scope scroll",offset,disabled ? 0 : 22);
        check_int("list scope content width",(int)content.width,108);
        check_int("list scope content y",(int)content.y,21-offset);
        check_int("list scope disabled",UIContentDisabled(),disabled);
        EndScroll();
        EndDisabled();
        check_int("list scope restored",UIContentDisabled(),0);
        EndUIFrame();
    }
}

static void
test_list_box_keyboard_navigation(void)
{
    const char *items[] = {"0","1","2","3","4","5","6","7"};
    int selected = 0, offset = 0;
    ListBoxProps list = {
        .bounds={20,20,120,48}, .id=26130, .items=items, .item_count=8,
        .selected_index=&selected, .scroll_offset=&offset, .row_height=24
    };

    InjectReset(); InjectKeyTap(KEY_END); InjectPump();
    BeginUIFrame(200,120,1); SetUIFocus(list.id);
    check_int("list End changed",RenderListBox(list),1); EndUIFrame();
    check_int("list End selection",selected,7);
    check_int("list End reveal",offset,144);

    InjectKeyTap(KEY_UP); InjectPump();
    BeginUIFrame(200,120,1); SetUIFocus(list.id);
    check_int("list Up changed",RenderListBox(list),1); EndUIFrame();
    check_int("list Up selection",selected,6);
    check_int("list Up retains viewport",offset,144);

    InjectKeyTap(KEY_HOME); InjectPump();
    BeginUIFrame(200,120,1); SetUIFocus(list.id);
    check_int("list Home changed",RenderListBox(list),1); EndUIFrame();
    check_int("list Home selection",selected,0);
    check_int("list Home reveal",offset,0);

    list.disabled = 1;
    InjectKeyTap(KEY_END); InjectPump();
    BeginUIFrame(200,120,1); SetUIFocus(list.id);
    check_int("disabled list rejects End",RenderListBox(list),0); EndUIFrame();
    check_int("disabled list selection",selected,0);
    list.disabled = 0;
    selected = -1;
    InjectReset(); InjectPump();
    BeginUIFrame(200,120,1); SetUIFocus(list.id);
    check_int("idle list unchanged",RenderListBox(list),0); EndUIFrame();
    check_int("idle list keeps no selection",selected,-1);
    InjectReset();
}

static void
test_popup_list_box_keyboard_ownership(void)
{
    const char *items[] = {"a","b"};
    for(int inside = 0; inside < 2; inside++) {
        int selected = 0, offset = 0;
        ListBoxProps list = {
            .bounds={20,20,100,48}, .id=26131, .items=items, .item_count=2,
            .selected_index=&selected, .scroll_offset=&offset, .row_height=24
        };
        InjectReset(); InjectKeyTap(KEY_DOWN); InjectPump();
        BeginUIFrame(200,120,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,26100,(Rectangle){10,10,140,100});
        UIPopupInputToken child = ui_popup_input_begin(context,26101,(Rectangle){15,15,120,80});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(list.id);
        RenderListBox(list);
        check_int("only top popup list handles keyboard",selected,inside ? 1 : 0);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_scroll_scope(void)
{
    int offset = 0;
    Rectangle content;
    InjectReset();
    InjectMousePosition(30, 30);
    InjectWheel(-1);
    InjectPump();
    BeginUIFrame(220, 220, 1.0f);
    content = BeginScroll((Rectangle){10,10,100,60}, 200, &offset);
    check_int("scroll offset", offset, 42);
    check_int("scroll content y", (int)content.y, -32);
    check_int("scroll clipped input", UIInputCapturesClick((Vector2){20,90}), 1);
    (void)BeginScroll((Rectangle){20,30,100,60}, 100, NULL);
    check_int("nested scroll clips to parent", UIInputCapturesClick((Vector2){115,40}), 1);
    EndScroll();
    EndScroll();
    check_int("scroll restores input", UIInputCapturesClick((Vector2){20,90}), 0);
    EndUIFrame();
}

static void
test_scroll_thumb_drag(void)
{
    int offset = 0;
    InjectReset();
    for(int frame = 0; frame < 4; frame++) {
        InjectMousePosition(105, frame == 0 || frame == 3 ? 20 : 110);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(220,220,1.0f);
        Rectangle content = BeginScroll((Rectangle){10,10,100,60},200,&offset);
        check_int("scrollbar reserves width", (int)content.width, 90);
        check_int("scrollbar excludes child input", UIInputCapturesClick((Vector2){105,20}), 1);
        EndScroll();
        EndUIFrame();
        check_int("scroll thumb drag and release", offset, frame == 0 ? 0 : 140);
    }
}

static void
test_table_column_resize(void)
{
    const char *columns[] = {"A", "B", "C"};
    const char *cells[] = {"a", "b", "c"};
    TableRow rows[] = {{cells, 3, NULL, NULL}};
    int widths[] = {100, 100, 100};
    TableViewProps table = {0};

    table.bounds = (Rectangle){10, 10, 300, 110};
    table.id = 143;
    table.columns = columns;
    table.column_count = 3;
    table.rows = rows;
    table.row_count = 1;
    table.column_widths = widths;
    table.row_height = 24;
    table.resizable = 1;
    table.min_column_width = 48;

    InjectReset();
    InjectMousePosition(108, 20);
    InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();

    InjectMousePosition(138, 20);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    check_int("table resize changed", TableView(table), 1);
    EndUIFrame();
    check_int("table resized width", widths[0], 130);

    InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
    InjectPump();
    BeginUIFrame(340, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
}

static void
test_table_frozen_rows_hit_testing(void)
{
    const char *columns[] = {"Name"};
    const char *cells0[] = {"row 0"};
    const char *cells1[] = {"row 1"};
    const char *cells2[] = {"row 2"};
    const char *cells3[] = {"row 3"};
    const char *cells4[] = {"row 4"};
    TableRow rows[] = {
        {cells0, 1, NULL, NULL}, {cells1, 1, NULL, NULL},
        {cells2, 1, NULL, NULL}, {cells3, 1, NULL, NULL},
        {cells4, 1, NULL, NULL}
    };
    int selected_row = -1;
    int selected_column = -1;
    int scroll = 36;
    TableViewProps table = {0};

    table.bounds = (Rectangle){10, 10, 180, 102};
    table.id = 144;
    table.columns = columns;
    table.column_count = 1;
    table.rows = rows;
    table.row_count = 5;
    table.selected_row = &selected_row;
    table.selected_column = &selected_column;
    table.scroll_offset = &scroll;
    table.row_height = 24;
    table.freeze_rows = 1;

    InjectReset();
    InjectTap(30, 45);
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    check_int("table frozen row hit", selected_row, 0);

    InjectTap(30, 70);
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    InjectPump();
    BeginUIFrame(300, 180, 1.0f);
    (void)TableView(table);
    EndUIFrame();
    check_int("table scrolled row hit", selected_row, 2);

    InjectTap(30, 115);
    for(int frame = 0; frame < 2; frame++) {
        InjectPump();
        BeginUIFrame(300, 180, 1.0f);
        (void)TableView(table);
        EndUIFrame();
    }
    check_int("table clipped row ignores click below body", selected_row, 2);
}

static void
test_table_keyboard_navigation(void)
{
    const char *columns[] = {"A", "B", "C"};
    const char *cells[] = {"a", "b", "c"};
    TableRow rows[] = {
        {cells,3,NULL,NULL}, {cells,3,NULL,NULL}, {cells,3,NULL,NULL},
        {cells,3,NULL,NULL}, {cells,3,NULL,NULL}, {cells,3,NULL,NULL}
    };
    int order[] = {2,0,1};
    int selected_row = 0, selected_column = 2;
    int activated_row = -1, activated_column = -1, scroll = 0;
    TableViewProps table = {
        .bounds={10,10,180,70}, .id=145, .columns=columns, .column_count=3,
        .rows=rows, .row_count=6, .selected_row=&selected_row,
        .selected_column=&selected_column, .activated_row=&activated_row,
        .activated_column=&activated_column, .scroll_offset=&scroll,
        .row_height=20, .column_order=order
    };

    InjectReset(); InjectKey(KEY_RIGHT,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145);
    int changed = TableView(table); EndUIFrame();
    InjectKey(KEY_RIGHT,0); InjectPump();
    check_int("table keyboard right changed",changed,1);
    check_int("table keyboard follows display order",selected_column,0);

    InjectKey(KEY_TAB,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_TAB,0); InjectPump();
    check_int("table tab advances within row",selected_column,1);
    check_int("table tab retains table focus",GetUIFocus(),145);

    InjectKey(KEY_LEFT_SHIFT,1); InjectKey(KEY_TAB,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_TAB,0); InjectKey(KEY_LEFT_SHIFT,0); InjectPump();
    check_int("table shift tab reverses within row",selected_column,0);
    check_int("table shift tab retains table focus",GetUIFocus(),145);

    InjectKey(KEY_DOWN,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_DOWN,0); InjectPump();
    check_int("table keyboard down",selected_row,1);

    InjectKey(KEY_F2,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    check_int("table keyboard activated row",activated_row,1);
    check_int("table keyboard activated column",activated_column,0);
    InjectKey(KEY_F2,0); InjectPump();

    for(int row = 2; row < 6; row++) {
        InjectKey(KEY_DOWN,1); InjectPump();
        BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
        InjectKey(KEY_DOWN,0); InjectPump();
        check_int("table keyboard advances each row",selected_row,row);
    }
    check_int("table keyboard reaches final row",selected_row,5);
    check_int("table keyboard scrolls selection",scroll,80);

    table.disabled = 1;
    InjectKey(KEY_UP,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_UP,0); InjectPump();
    check_int("disabled table blocks keyboard",selected_row,5);
    table.disabled = 0;

    InjectKey(KEY_ESCAPE,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_ESCAPE,0); InjectPump();
    check_int("table escape clears row",selected_row,-1);
    check_int("table escape clears column",selected_column,-1);

    selected_row = 0;
    selected_column = 1;
    InjectKey(KEY_LEFT_CONTROL,1); InjectKey(KEY_C,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_C,0); InjectPump();
    check_int("table cell copy",strcmp(GetUIClipboardTextValue(),"b"),0);

    selected_column = -1;
    InjectKey(KEY_C,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_C,0); InjectPump();
    check_int("table row copy",strcmp(GetUIClipboardTextValue(),"a\tb\tc"),0);

    selected_row = -1;
    selected_column = 2;
    InjectKey(KEY_C,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_C,0); InjectPump();
    check_int("table column copy",strcmp(GetUIClipboardTextValue(),"c\nc\nc\nc\nc\nc"),0);

    table.copy_text = "editable-id";
    InjectKey(KEY_C,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145); TableView(table); EndUIFrame();
    InjectKey(KEY_C,0); InjectPump();
    check_int("table copy override",strcmp(GetUIClipboardTextValue(),"editable-id"),0);

    const char *pasted_text = NULL;
    int pasted_row = -1, pasted_column = -1;
    table.pasted_text = &pasted_text;
    table.pasted_row = &pasted_row;
    table.pasted_column = &pasted_column;
    selected_row = 1;
    selected_column = 0;
    SetUIClipboardTextValue("new\tvalues");
    InjectKey(KEY_V,1); InjectPump();
    BeginUIFrame(240,160,1); SetUIFocus(145);
    check_int("table paste changed",TableView(table),1); EndUIFrame();
    InjectKey(KEY_V,0); InjectKey(KEY_LEFT_CONTROL,0); InjectPump();
    check_int("table paste text",strcmp(pasted_text,"new\tvalues"),0);
    check_int("table paste row",pasted_row,1);
    check_int("table paste column",pasted_column,0);
    InjectReset();
}

static void
test_popup_table_keyboard_ownership(void)
{
    const char *columns[] = {"A"};
    const char *cells[] = {"a"};
    TableRow rows[] = {{cells,1,NULL,NULL},{cells,1,NULL,NULL}};

    for(int inside = 0; inside < 2; inside++) {
        int selected_row = 0, selected_column = 0;
        TableViewProps table = {
            .bounds={20,20,100,80}, .id=26120, .columns=columns,
            .column_count=1, .rows=rows, .row_count=2,
            .selected_row=&selected_row, .selected_column=&selected_column
        };
        InjectReset(); InjectKeyTap(KEY_DOWN); InjectPump();
        BeginUIFrame(240,160,1);
        UIPopupInput *context = ui_popup_input_create();
        ui_popup_input_frame(context);
        UIPopupInput *previous = ui_popup_input_bind(context);
        UIPopupInputToken parent = ui_popup_input_begin(context,26100,(Rectangle){10,10,140,120});
        UIPopupInputToken child = ui_popup_input_begin(context,26101,(Rectangle){15,15,120,100});
        if(!inside) ui_popup_input_end(child);
        SetUIFocus(26120);
        TableView(table);
        check_int("only top popup table handles keyboard",selected_row,inside ? 1 : 0);
        if(inside) ui_popup_input_end(child);
        ui_popup_input_end(parent);
        ui_popup_input_finish(context);
        ui_popup_input_bind(previous);
        ui_popup_input_destroy(context);
        EndUIFrame();
    }
    InjectReset();
}

static void
test_paned_drag_outside_handle(void)
{
    UIFrameState saved = SaveUIFrameState();
    int split = 90;
    PanedViewProps panes = {{10,10,240,80}, 9450, 1, &split, 40, 40};
    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        InjectMousePosition(frame == 0 ? 100 : 190, 30);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(640,480,1);
        PanedView(panes);
        EndUIFrame();
    }
    check_int("paned drag follows pointer outside original handle", split, 180);
    InjectReset();
    RestoreUIFrameState(saved);
}

static void
test_drag_drop_accepts_dragged_release(void)
{
    UIFrameState saved = SaveUIFrameState();
    int payload = 42, output = 0, accepted = 0;
    DragDropSourceProps source = {{10,10,80,40}, 9401, "integer", &payload, sizeof(payload), 0};
    DragDropTargetProps target = {{150,10,80,40}, 9402, "integer", &output, sizeof(output), &accepted, 0};

    InjectReset();
    for(int frame = 0; frame < 3; frame++) {
        InjectMousePosition(frame == 0 ? 30 : 180, 30);
        if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
        if(frame == 2) InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
        InjectPump();
        BeginUIFrame(640, 480, 1.0f);
        if(frame == 2) g_ui_pointer_dragging = 1;
        DragDropSource(source);
        DragDropTarget(target);
        EndUIFrame();
    }
    check_int("dragged release copies payload", output, payload);
    check_int("dragged release reports copied size", accepted, sizeof(payload));
    InjectReset();
    RestoreUIFrameState(saved);
}

static void
test_control_style_resolution(void)
{
    Style base;
    ControlStyle control;
    Style got;

    memset(&base, 0, sizeof(base));
    memset(&control, 0, sizeof(control));
    base.fields = StyleBackground | StyleForeground | StyleRadius;
    base.background = (Color){10, 20, 30, 255};
    base.foreground = (Color){240, 240, 240, 255};
    base.radius = 8.0f;
    control.normal.fields = StyleBackground | StyleRadius;
    control.normal.background = BLANK;
    control.normal.radius = 0.0f;
    control.hover.fields = StyleForeground | StyleContentOffset;
    control.hover.foreground = (Color){1, 2, 3, 4};
    control.hover.content_offset = (Vector2){2, 3};

    got = ResolveControlStyle(base, control, ButtonStateHover);
    check_color("style transparent background", got.background, BLANK);
    check_color("style state foreground", got.foreground,
                (Color){1, 2, 3, 4});
    check_int("style zero radius", (int)got.radius, 0);
    check_int("style content offset x", (int)got.content_offset.x, 2);
    check_int("style content offset y", (int)got.content_offset.y, 3);

    Theme original = GetTheme();
    Theme custom = original;
    custom.colors.surface = (Color){12, 23, 34, 0};
    custom.colors.surface_raised = (Color){45, 56, 67, 89};
    custom.colors.border = (Color){23, 45, 67, 0};
    custom.colors.text_muted = (Color){34, 56, 78, 0};
    custom.colors.selection = (Color){45, 67, 89, 128};
    custom.colors.on_accent = (Color){56, 78, 90, 0};
    custom.colors.icon = (Color){67, 89, 101, 0};
    SetTheme(custom);
    check_color("theme preserves transparent border", GetThemeBorder(), custom.colors.border);
    check_color("theme preserves muted text", GetThemeMutedText(), custom.colors.text_muted);
    check_color("theme preserves selection", GetThemeSelection(), custom.colors.selection);
    check_color("theme preserves button text", GetThemeButtonText(), custom.colors.on_accent);
    check_color("theme preserves icon", GetThemeIcon(), custom.colors.icon);
    ButtonProps button = {.tone = ButtonToneNeutral, .emphasis = ButtonEmphasisFilled};
    got = ResolveButtonStyle(button, ButtonStateNormal);
    check_color("button uses declared raised surface", got.background, custom.colors.surface_raised);
    button.emphasis = ButtonEmphasisOutline;
    got = ResolveButtonStyle(button, ButtonStateNormal);
    check_color("button preserves transparent theme surface", got.background, custom.colors.surface);
    Theme light = ThemeDefaultLight();
    SetTheme(light);
    button.tone = ButtonToneAccent;
    button.emphasis = ButtonEmphasisFilled;
    got = ResolveButtonStyle(button, ButtonStateFocus);
    check_color("light focus blue ink", got.foreground, light.colors.link);
    check_int("light focus pale face", got.background.r >= 220 && got.background.g >= 230, 1);
    got = ResolveButtonStyle(button, ButtonStateHover);
    check_color("light hover blue ink", got.foreground, light.colors.link);
    SetTheme(original);
    ClearThemeMetricsOverride();
}

int
main(void)
{
    check_int("default control style", GetThemeStyle(), THEME_STYLE_DEFAULT);
    SetThemeMode(THEME_MODE_LIGHT);
    check_color("default light background", GetThemeBackground(), ThemeDefaultLight().colors.background);
    check_color("default light border", GetThemeBorder(), ThemeDefaultLight().colors.border);
    check_color("default light muted text", GetThemeMutedText(), ThemeDefaultLight().colors.text_muted);
    check_color("default light selection", GetThemeSelection(), ThemeDefaultLight().colors.selection);
    check_color("default light button text", GetThemeButtonText(), ThemeDefaultLight().colors.on_accent);
    check_color("default light icon", GetThemeIcon(), ThemeDefaultLight().colors.icon);
    SetThemeMode(THEME_MODE_DARK);
    check_color("default dark background", GetThemeBackground(), ThemeDefaultDark().colors.background);
    check_color("default dark border", GetThemeBorder(), ThemeDefaultDark().colors.border);
    check_color("default dark muted text", GetThemeMutedText(), ThemeDefaultDark().colors.text_muted);
    check_color("default dark selection", GetThemeSelection(), ThemeDefaultDark().colors.selection);
    check_color("default dark button text", GetThemeButtonText(), ThemeDefaultDark().colors.on_accent);
    check_color("default dark icon", GetThemeIcon(), ThemeDefaultDark().colors.icon);
    SetThemeMode(THEME_MODE_LIGHT);
#if !defined(KRYON_BACKEND_TERMI)
    check_int("motion enabled by default", UITransitionCuesEnabled(), 1);
    SetUITransitionCuesEnabled(0);
    check_int("motion opt-out", UITransitionCuesEnabled(), 0);
    SetUITransitionCuesEnabled(1);
    check_int("motion re-enabled", UITransitionCuesEnabled(), 1);
#endif
    test_control_style_resolution();
    {
        int value = 10;
        BeginUIFrame(220,120,1);
        BeginTree(Key("numeric origin layout"));
        Row((RowProps){.bounds = {0,0,220,24}});
        test_input_int((UIIntInputProps){.bounds = {0,0,120,24}, .id = 872,
            .values = &value, .value_count = 1, .step = 1});
        Button((ButtonProps){.bounds = {0,0,40,24}, .id = 873, .label = "next"});
        End();
        EndTree();
        EndUIFrame();
        int count = 0, paints = 0, next = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        for(int i = 0; i < count; i++) {
            if(nodes[i].kind == UI_WIDGET_TEXT_INPUT_PAINT_NODE) {
                paints++;
                check_int("numeric origin paint x", (int)nodes[i].bounds.x, 0);
                check_int("numeric origin parent", nodes[nodes[i].parent].id, 872);
            }
            if(nodes[i].id == 873) {
                next++;
                check_int("numeric consumes one row slot", (int)nodes[i].bounds.x, 120);
            }
        }
        check_int("numeric origin paint count", paints, 1);
        check_int("numeric next sibling count", next, 1);
    }
    {
        int value = 10;
        InjectReset();
        for(int frame = 0; frame < 4; frame++) {
            InjectMousePosition(100, 40);
            if(frame == 0) InjectMouseButton(MOUSE_BUTTON_LEFT, 1);
            if(frame == 1) {
                InjectMouseButton(MOUSE_BUTTON_LEFT, 0);
                InjectText("5");
            }
            if(frame == 2) InjectText("9");
            InjectPump();
            BeginUIFrame(220,120,1);
            BeginTree(Key("numeric typing"));
            Row((RowProps){.bounds = {20,30,120,24}});
            BeginDisabled(frame == 2);
            int changed = test_input_int((UIIntInputProps){.bounds = {0,0,120,24}, .id = 871,
                .values = &value, .value_count = 1});
            check_int("numeric typing returns during declaration", changed, frame == 1);
            EndDisabled();
            End();
            EndTree();
            EndUIFrame();
            check_int("headless numeric typing and disabled discard", value, frame == 0 ? 10 : 105);
        }
        InjectReset();
    }
    {
        int value = 10;
        const int expected[] = {12, 10, 15, 15};
        for(int scenario = 0; scenario < 4; scenario++) {
            InjectReset();
            for(int frame = 0; frame < 2; frame++) {
                InjectMousePosition(scenario == 1 ? 104 : 128, 40);
                InjectMouseButton(MOUSE_BUTTON_LEFT, frame == 0);
                InjectKey(KEY_LEFT_SHIFT, scenario == 2);
                InjectPump();
                BeginUIFrame(220,120,1);
                BeginTree(Key("numeric steps"));
                Row((RowProps){.bounds = {20,30,120,24}});
                BeginDisabled(scenario == 3);
                test_input_int((UIIntInputProps){.bounds = {0,0,120,24}, .id = 870,
                    .values = &value, .value_count = 1, .step = 2, .step_fast = 5});
                EndDisabled();
                End();
                EndTree();
                EndUIFrame();
            }
            check_int("headless numeric step lifecycle", value, expected[scenario]);
        }
        InjectReset();
    }
    UINumericInputState *editors[129];
    {
        int identities[396];
        int identity_count = 0;
        for(int kind = 0; kind < 3; kind++) {
            for(int id = -1; id <= 2; id++) {
                for(int component = 0; component < 33; component++) {
                    UINumericInputState *state = ui_numeric_input_state(kind, id, component);
                    check_int("numeric exact identity", state == ui_numeric_input_state(kind,id,component), 1);
                    for(int previous = 0; previous < identity_count; previous++) {
                        int delta = state->token-identities[previous];
                        check_int("numeric field/step IDs disjoint", delta >= 3 || delta <= -3, 1);
                    }
                    identities[identity_count++] = state->token;
                }
            }
        }
        check_int("numeric identities tested", identity_count, 396);
    }
    for(int i = 0; i < 129; i++) {
        editors[i] = ui_numeric_input_state(0, 1 + i*128, 0);
        snprintf(editors[i]->text, sizeof(editors[i]->text), "edit-%d", i);
        editors[i]->cursor = i % 8;
        editors[i]->focused = 1;
    }
    for(int i = 0; i < 129; i++) {
        char expected[64];
        UINumericInputState *state = ui_numeric_input_state(0, 1 + i*128, 0);
        snprintf(expected, sizeof(expected), "edit-%d", i);
        check_int("numeric editor stable address", state == editors[i], 1);
        check_int("numeric editor text isolation", strcmp(state->text, expected), 0);
        check_int("numeric editor cursor isolation", state->cursor, i % 8);
        check_int("numeric editor focus isolation", state->focused, 1);
    }
    test_paned_drag_outside_handle();
    test_drag_drop_accepts_dragged_release();
    Rectangle parent = {10, 20, 200, 120};
    FrameBox frame;
    GridFrame grid;
    Rectangle r;
    Rectangle hits[3] = {
        {0, 0, 20, 20},
        {10, 10, 20, 20},
        {100, 100, 10, 10}
    };

    SetUIScale(1.0f);
    test_theme_surface_helpers();
    test_semantic_font_sizes_follow_ui_scale();
    test_circle_click_uses_ui_release_path();

    SetThemeStyle(THEME_STYLE_CLASSIC);
    check_int("classic style", GetThemeStyle(), THEME_STYLE_CLASSIC);
    check_int("classic effective style", GetEffectiveThemeStyle(), THEME_STYLE_CLASSIC);
    check_int("retro bevel", GetThemeMetrics().bevel_enabled, 1);

    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_classic",
            "theme_style_default", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_DEFAULT);
    check_int("default style", GetThemeStyle(), THEME_STYLE_DEFAULT);
    check_int("default effective style", GetEffectiveThemeStyle(), THEME_STYLE_DEFAULT);
    check_int("material bevel", GetThemeMetrics().bevel_enabled, 0);
    check_int("material touch target", GetThemeMetrics().touch_target_min, 48);

    SetThemeStyle((ThemeStyle)3);
    check_int("out-of-range style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    check_int("out-of-range effective style", GetEffectiveThemeStyle(),
              GetDefaultPlatformThemeStyle());
    check_int("theme count", THEME_COUNT, THEME_SWEET + 1);
    check_int("out-of-range theme normalizes", NormalizeTheme(THEME_COUNT),
              THEME_MONO);
    /* Theme-section locale keys must resolve to real strings (the
     * settings picker wires these as fallbacks). */
    {
        static const char *keys[] = {
            "theme_style_label", "theme_style_system", "theme_style_classic",
            "theme_style_default", "theme_label",
            "theme_app", "theme_system", "theme_mode_label",
            "theme_follow_device", "theme_light", "theme_dark",
            "theme_color_label", "theme_picker_title"
        };
        size_t i;

        for(i = 0; i < sizeof(keys) / sizeof(keys[0]); i++) {
            const char *text = GetLocaleText(keys[i]);

            if(text == NULL || text[0] == '\0' || strcmp(text, keys[i]) == 0) {
                fprintf(stderr, "locale key unresolved: %s\n", keys[i]);
                return 1;
            }
        }
    }

    SetThemeStyle(THEME_STYLE_DEFAULT);

    SetThemeStyle((ThemeStyle)99);
    check_int("invalid style clamps", GetThemeStyle(), THEME_STYLE_SYSTEM);
    SetThemeStyle(THEME_STYLE_SYSTEM);
#if defined(ANDROID_BUILD) && ANDROID_BUILD
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_DEFAULT);
#elif defined(PLATFORM_ANDROID) || defined(__ANDROID__) || defined(ANDROID)
    check_int("android default style", GetEffectiveThemeStyle(), THEME_STYLE_DEFAULT);
#else
    check_int("host default style", GetEffectiveThemeStyle(), THEME_STYLE_DEFAULT);
#endif

    frame = BeginFrameBox(parent, 10, 10, 4);
    r = FramePack(&frame, SideTop, 30);
    check_int("pack x", (int)r.x, 20);
    check_int("pack y", (int)r.y, 30);
    check_int("pack width", (int)r.width, 180);
    check_int("pack height", (int)r.height, 30);

    grid = (GridFrame){parent, 2, 2, 10, 10, 0, 0};
    r = GridCell(grid, 1, 1, 1, 1);
    check_int("grid x", (int)r.x, 115);
    check_int("grid y", (int)r.y, 85);
    check_int("grid width", (int)r.width, 95);
    check_int("grid height", (int)r.height, 55);

    check_int("topmost hit", CanvasHitTest((Vector2){15, 15}, hits, 3), 1);
    check_int("miss", CanvasHitTest((Vector2){80, 80}, hits, 3), -1);
    test_numeric_ctrl_click_editing();
    test_menu_bar_switches_while_popup_captures_input();
    test_menu_keyboard_navigation();
    test_popup_menu_keyboard_navigation();
    test_popup_menu_keyboard_ownership();
    test_icon_button_activation();
    test_nested_disabled_scope();
    test_disabled_scalar_cancels_gesture();
    test_focusable_choice_keyboard_navigation();
    test_toggle_keyboard_navigation();
    test_multi_select_keyboard_navigation();
    test_focusable_image_keyboard_navigation();
    test_deep_disabled_scopes();
    test_collapsible_composes_children();
    test_tree_header_modes();
    test_closeable_collapsible();
    test_tree_header_keyboard_gates();
    test_dropdown_popup_lifecycle();
    test_dropdown_store_isolation();
    test_many_dropdown_identities();
    test_large_dropdown_options();
    test_dropdown_keyboard_navigation();
    test_dropdown_keyboard_open();
    test_dropdown_scrollbar_dismissal();
    test_dropdown_horizontal_viewport();
    test_popup_layout_restoration();
    test_popup_disabled_restoration();
    test_popup_input_clip_restoration();
    test_nested_popup_input_ownership();
    test_retained_popup_input_ownership();
    test_retained_popup_pointer_focus();
    test_popup_dropdown_keyboard_ownership();
    test_popup_accelerator_keyboard_ownership();
    test_popup_collapsible_keyboard_ownership();
    test_composed_popup_children_scope();
    test_composed_popup_scope();
    test_composed_tooltip_scope();
    test_composed_modal_scope();
    test_composed_context_popup_scope();
    test_composed_popup_focus_lifecycle();
    test_popup_active_drag_ownership();
    test_card_props_retained_input();
    test_popup_text_keyboard_ownership();
    test_text_area_page_navigation();
    test_text_area_wheel_scroll();
    test_secure_text_field_word_navigation();
    test_immediate_text_composition();
    test_popup_tab_ownership();
    test_popup_button_keyboard_ownership();
    test_popup_choice_keyboard_ownership();
    test_popup_multi_select_keyboard_ownership();
    test_popup_drag_keyboard_ownership();
    test_popup_tab_missing_owner();
    test_popup_text_dismissal_replay();
    test_popup_composition_dismissal_replay();
    test_popup_preedit_cancellation();
    test_custom_table_cell_scope();
    test_retained_scope_clip();
    test_list_box_scope();
    test_list_box_keyboard_navigation();
    test_popup_list_box_keyboard_ownership();
    test_scroll_scope();
    test_scroll_thumb_drag();
    test_table_frozen_rows_hit_testing();
    test_table_column_resize();
    test_table_keyboard_navigation();
    test_popup_table_keyboard_ownership();

    {
        int sx = 10;
        int sy = 20;
        float zoom = 2.0f;
        Canvas canvas = {{40, 50, 200, 100}, &sx, &sy, &zoom};
        Vector2 p = CanvasToScreen(canvas, (Vector2){50, 70});
        Rectangle rr = CanvasRectToScreen(canvas, (Rectangle){50, 70, 20, 10});
        check_int("canvas screen x", (int)p.x, 40);
        check_int("canvas screen y", (int)p.y, 50);
        check_int("canvas rect w", (int)rr.width, 40);
        check_int("canvas rect h", (int)rr.height, 20);
    }

    {
        Camera2D camera = {0};
        UIWidget widget;
        UIInspectNode node;
        UIInspectSelection selection;
        int token;

        SetUIInspectEnabled(1);
        BeginUIInspectFrame(".");
        SetUIInspectCanvasBounds((Rectangle){40, 50, 200, 120});
        camera.offset = (Vector2){40, 50};
        camera.zoom = 2.0f;
        token = PushUIInspectTransform(camera);
        BeginUIInspectFrame(NULL);
        widget = BeginUIWidget("test", "inspect-transform",
                               (Rectangle){10, 20, 30, 15}, 0);
        EndUIWidget(&widget);
        check_int("inspect transformed count", UIInspectWidgetCount(), 1);
        check_int("inspect node count", UIInspectNodeCount(), 1);
        check_int("inspect find @name",
                  UIInspectFindNode("@inspect-transform", &node), 1);
        check_int("inspect find role", KryTFind("role=test", &node), 1);
        check_int("inspect node line default", node.source_line, 0);
        check_int("inspect transformed hit",
                  UIInspectSelectAt((Vector2){65, 95}), 1);
        selection = UIInspectGetSelection();
        check_int("inspect selected x", (int)selection.bounds.x, 10);
        check_int("inspect transformed miss",
                  UIInspectSelectAt((Vector2){20, 20}), 0);
        PopUIInspectTransform(token);
    }

    test_reorder_uses_item_center_and_header_handle();
    test_checkbox_paint_geometry_is_stable();
    test_swatch_policy();
    test_color_picker_policy();
    test_button_policy();
    test_separator_policy();
    test_canvas_grid_policy();
    test_fieldset_policy();
    test_plot_policy();
    test_progress_layout_policy();
    test_selectable_paint_policy();
    test_radio_paint_policy();
    test_list_box_layout_policy();
    test_multi_select_policy();
    test_tab_bar_policy();
    test_popup_policy();
    test_text_input_policy();
    test_segmented_control_policy();
    test_spinbox_policy();
    test_slider_value_policy();
    test_drag_value_policy();
    test_input_value_policy();
    test_slider_keyboard_navigation();
    test_drag_keyboard_navigation();
    test_tab_bar_keyboard_navigation();
    test_tab_bar_owned_scroll_state();
    test_composed_tab_bar_scope();
    test_popup_tab_bar_keyboard_ownership();
    test_step_button_keyboard_navigation();
    return 0;
}

#include "ui_internal.h"
#include "ui_image_internal.h"
#include "ui_popup_input_internal.h"
#include "ui_style_internal.h"
#include "ui_paint_internal.h"
#include "theme.h"
#include "ui_color.h"
#include "ui_style_sheet.h"
#include "runtime/button.h"
#include "runtime/segmented_control.h"
#include "runtime/style.h"
#include "runtime/surface.h"

static StyleData
ui_minimal_control_style_data(void)
{
    return (StyleData){
        .fields = (uint32_t)(StyleOpacity | StyleFontSize | StyleIconSize |
                             StyleMaterial),
        .opacity = 1.0f,
        .font_size = 16.0f,
        .icon_size = 20.0f,
        .material = MaterialFlat
    };
}

static StyleData
ui_resolve_minimal_control_role_state(ButtonProps button, ButtonState state,
                                      int style_kind, int role,
                                      ControlStyle override)
{
    StyleFacts facts = StyleControlRoleFacts(
        style_kind != 0 ? style_kind : StyleKindButton(), button.id,
        button.class_name,
        role, (int)button.tone, (int)button.emphasis, (int)button.size, state);
    StyleData value = ResolveActiveStyle(ui_minimal_control_style_data(),
                                         facts, state);
    return ResolveValues(value, ui_pack_style_states(override), state);
}

static StyleFrame
ui_resolve_minimal_control_role_frame(ButtonProps button, ButtonState state,
                                      int automatic, float h, float p, float f,
                                      int style_kind, int role)
{
    StateFlags flags = ResolveFlags((int)button.state, button.disabled,
                                    button.loading, button.selected);
    StyleFrame frame = {0};

    frame.value = ui_resolve_minimal_control_role_state(button, state,
                                                       style_kind, role,
                                                       (ControlStyle){0});
    frame.fill = FillState(frame.value.fields, frame.value.background,
                           frame.value.background_end);
    if(automatic && !flags.disabled && !flags.loading && !flags.selected &&
       (h > 0.0f || p > 0.0f || f > 0.0f)) {
        StyleData normal = state == ButtonStateNormal
            ? frame.value
            : ui_resolve_minimal_control_role_state(button, ButtonStateNormal,
                                                    style_kind, role,
                                                    (ControlStyle){0});
        StyleData hover = h > 0.0f
            ? ui_resolve_minimal_control_role_state(button, ButtonStateHover,
                                                    style_kind, role,
                                                    (ControlStyle){0})
            : normal;
        StyleData press = p > 0.0f
            ? ui_resolve_minimal_control_role_state(button, ButtonStatePressed,
                                                    style_kind, role,
                                                    (ControlStyle){0})
            : normal;
        StyleData focus = f > 0.0f
            ? ui_resolve_minimal_control_role_state(button, ButtonStateFocus,
                                                    style_kind, role,
                                                    (ControlStyle){0})
            : normal;
        frame = TransitionFrame(frame.value, normal, hover, press, focus,
                                h, p, f);
    }
    return frame;
}

StyleFrame
ui_resolve_button_spec_frame(ButtonSpec button, ButtonState state,
                             int automatic, float h, float p, float f,
                             int style_kind)
{
    StateFlags flags = ResolveFlags((int)button.props.state,
                                    button.props.disabled,
                                    button.props.loading,
                                    button.props.selected);
    StyleFrame frame = {0};
    int kind = style_kind != 0 ? style_kind : StyleKindButton();

    frame.value = ui_resolve_minimal_control_role_state(
        button.props, state, kind, StyleAny(), button.style);
    frame.fill = FillState(frame.value.fields, frame.value.background,
                           frame.value.background_end);
    if(automatic && !flags.disabled && !flags.loading && !flags.selected &&
       (h > 0.0f || p > 0.0f || f > 0.0f)) {
        StyleData normal = state == ButtonStateNormal ? frame.value
            : ui_resolve_minimal_control_role_state(
                button.props, ButtonStateNormal, kind, StyleAny(),
                button.style);
        StyleData hover = h > 0.0f
            ? ui_resolve_minimal_control_role_state(
                button.props, ButtonStateHover, kind, StyleAny(),
                button.style)
            : normal;
        StyleData press = p > 0.0f
            ? ui_resolve_minimal_control_role_state(
                button.props, ButtonStatePressed, kind, StyleAny(),
                button.style)
            : normal;
        StyleData focus = f > 0.0f
            ? ui_resolve_minimal_control_role_state(
                button.props, ButtonStateFocus, kind, StyleAny(),
                button.style)
            : normal;
        frame = TransitionFrame(frame.value, normal, hover, press, focus,
                                h, p, f);
    }
    return frame;
}

static StyleFrame
ui_resolve_minimal_control_frame(ButtonProps button, ButtonState state,
                                 int automatic, float h, float p, float f,
                                 int style_kind)
{
    return ui_resolve_minimal_control_role_frame(button, state, automatic,
                                                h, p, f, style_kind,
                                                StyleAny());
}

static void
ui_draw_button_content(const ButtonSpec *button, Rectangle bounds,
                       int font, Color color)
{
    ButtonProps props = button->props;
    StyleData paint = {.icon_size = button->paint.icon_size, .gap = button->paint.gap,
                       .offset_x = button->paint.content_offset.x, .offset_y = button->paint.content_offset.y};
    ContentDrawing content = PaintContent(props, bounds, paint, font,
        TextWidth(props.label != NULL ? props.label : "", font), ColorToInt(color),
        ColorToInt(ui_app_style().background), (float)Scale(1000) / 1000.0f,
        GetTime() * 1000.0, button->disclosure);
    ui_draw(content.mark);
    ui_draw(content.label);
}

static int
ui_button_has_image(ButtonProps props)
{
    return (props.image_asset_path != NULL && props.image_asset_path[0] != '\0') ||
           props.image_bounds.width > 0.0f ||
           props.image_bounds.height > 0.0f;
}

static void
ui_draw_button_image(ButtonProps props, Rectangle fallback_bounds, Color tint)
{
    ImageProps image;

    if(!ui_button_has_image(props))
        return;
    memset(&image, 0, sizeof(image));
    image.asset_path = props.image_asset_path;
    image.bounds = props.image_bounds.width > 0.0f ||
                         props.image_bounds.height > 0.0f
                     ? props.image_bounds : fallback_bounds;
    image.source = props.image_source;
    image.origin = props.image_origin;
    image.rotation = props.image_rotation;
    image.fit = (ImageFit)props.image_fit;
    ImageTextureTinted(LoadImageTexture(image.asset_path), image,
                       tint.a != 0 ? tint : WHITE);
}

static void
ui_draw_button_swatch(ButtonProps props, Rectangle bounds)
{
    SwatchPaint paint;
    ButtonState state;
    StyleFrame face;

    if(!props.swatch || !IsWindowReady())
        return;
    Vector2 mouse = ui_mouse_world();
    int hovered = CheckCollisionPointRec(mouse, bounds) &&
                  !InputCapturesClick(mouse) &&
                  HoverEffectsEnabled();
    int focused = IsFocusActive(props.id) &&
                  !ui_popup_input_focus_captures(props.id);
    state = props.disabled ? ButtonStateDisabled :
            (focused ? ButtonStateFocus :
             (hovered ? ButtonStateHover : ButtonStateNormal));
    face = ui_control_style_frame_kind(props, state, 0, 0, 0, 0,
                                       StyleKindButton());
    paint = SwatchPaintFor((SwatchSpec){
        .bounds = bounds,
        .color = props.swatch_color,
        .disabled = props.disabled,
        .hovered = hovered,
        .focused = focused,
        .scale = (float)Scale(1000) / 1000.0f,
        .face = face
    });
    DrawRectangleRec(paint.checker_base, GetColor(paint.checker_base_color));
    DrawRectangleRec(paint.checker_a, GetColor(paint.checker_alt_color));
    DrawRectangleRec(paint.checker_b, GetColor(paint.checker_alt_color));
    DrawRectangleRec(paint.swatch, paint.swatch_color);
    DrawRectangleLinesEx(paint.bounds, paint.border_width,
                         GetColor(paint.border_color));
}

static int
ui_termi_backend(void)
{
#if defined(KRYON_BACKEND_TERMI)
    return 1;
#else
    return 0;
#endif
}

static void
ui_draw_termi_button_outline(Rectangle bounds, Color border, int hovered,
                             int pressed, int disabled,
                             ButtonFallbackPolicy policy)
{
    Color outline = border;

    (void)hovered;
    (void)pressed;
    (void)disabled;
    if(policy.outline_white)
        outline = WHITE;
    else if(policy.outline_adjust < 0)
        outline = DarkenColor(outline, -policy.outline_adjust);
    else
        outline = LightenColor(outline, policy.outline_adjust);
    outline.a = 255;
    DrawRectangleLinesEx(bounds, policy.outline_width, outline);
}


static int
ui_render_button(ButtonSpec button, int handle_input, int paint,
                 int retained_hovered, int retained_pressed, Color *foreground)
{
    char editor_id[96];
    Widget widget;
    int hovered;
    int focused;
    Style normal_style = ui_unpack_style(ui_resolve_button_spec_frame(button,
        button.props.disabled ? ButtonStateDisabled : ButtonStateNormal,
        0, 0, 0, 0,
        button.style_kind != 0 ? button.style_kind : StyleKindButton()).value);
    int font = ResolveFont(0, StyleFontValue(normal_style.fields,
                                             normal_style.font_size),
                           GetFontSize());
    Style hover_style = ui_unpack_style(ui_resolve_button_spec_frame(button,
        ButtonStateHover, 0, 0, 0, 0,
        button.style_kind != 0 ? button.style_kind : StyleKindButton()).value);
    const char *typeface = normal_style.typeface;
    int typeface_token = 0;
    Color background = button.paint.background.a != 0 ? button.paint.background : normal_style.background;
    Color hover_background = button.hover_background.a != 0 ? button.hover_background : hover_style.background;
    Color text = button.paint.foreground.a != 0 ? button.paint.foreground : normal_style.foreground;
    Color border;
    float radius;
    int cues = TransitionCuesEnabled();
    ButtonFallbackPolicy fallback_policy;
    Color draw_background;
    Color draw_border;
    float hover_amount = 0.0f;
    InteractionMotion motion = {0};
    Rectangle draw_bounds;
    int termi_button = ui_termi_backend();
    int default_controls = ui_default_style() && !termi_button;

    memset(&widget, 0, sizeof(widget));
    if(handle_input) {
        widget = BeginWidget("Button",
                               ui_inspect_control_id(editor_id,
                                                     sizeof(editor_id),
                                                     "Button",
                                                     button.props.id,
                                                     button.props.label),
                               button.props.bounds,
                               WidgetFlagMovable |
                               WidgetFlagResizable);
        button.props.bounds = widget.bounds;
        WidgetSetAction(&widget, button.props.label);
    }
    ButtonProps props = button.props;
    ButtonInput input;
    if(handle_input) {
        input = ReadButtonInput(props.bounds, props.id, (int)props.state,
            props.disabled, props.loading, props.selected);
    } else {
        Activation sample = {
            .hovered = retained_hovered,
            .pressed = retained_pressed,
            .focused = button.props.id > 0 && IsFocusActive(button.props.id)
        };
        if(sample.focused && IsFocusActivatePressed(button.props.id))
            sample.pressed = true;
        input = ResolveButtonInput((int)props.state, props.disabled,
            props.loading, props.selected, sample);
    }
    button.props.disabled = input.flags.disabled;
    button.props.loading = input.flags.loading;
    button.props.selected = input.flags.selected;
    hovered = input.interaction.hovered;
    retained_pressed = input.interaction.pressed;
    focused = input.interaction.focused;
    fallback_policy = ButtonFallbackPolicyFor(hovered != 0,
                                             retained_pressed != 0,
                                             button.props.disabled,
                                             cues != 0,
                                             termi_button != 0);
    border = button.paint.border.a != 0 ? button.paint.border
             : LightenColor(background,
                            fallback_policy.fallback_border_lighten);
    radius = button.paint.radius > 0.0f ? button.paint.radius
             : fallback_policy.radius;
    draw_bounds = button.props.bounds;
    if(button.props.invisible) {
        if(focused)
            SetFocusTextInputActive(0);
        if(handle_input)
            EndWidget(&widget);
        return handle_input ? input.activated : 0;
    }
    if(!paint || !IsWindowReady()) {
        if(focused)
            SetFocusTextInputActive(0);
        if(handle_input)
            EndWidget(&widget);
        return handle_input
            ? input.activated : 0;
    }

    if(default_controls) {
        unsigned int key = 2166136261u;
        const char *label = button.props.label != NULL ? button.props.label : "";
        ThemeMetrics metrics;
        StyleFrame appearance;
        int style_font;

        key = (key ^ (unsigned int)button.props.id) * 16777619u;
        if(button.props.id == 0) {
            key = (key ^ (unsigned int)(int)button.props.bounds.x) * 16777619u;
            key = (key ^ (unsigned int)(int)button.props.bounds.y) * 16777619u;
            while(*label != '\0')
                key = (key ^ (unsigned char)*label++) * 16777619u;
        }
        props = button.props;
        props.disabled = input.flags.disabled;
        props.loading = input.flags.loading;
        props.selected = input.flags.selected;
        metrics = GetThemeMetrics();
        motion = AdvanceButtonMotion(key, (int)props.state, input, cues,
            GetFrameTime() * 1000.0f, metrics.transition_normal_ms, metrics.transition_fast_ms);
        appearance = ui_resolve_button_spec_frame(button,
            input.interaction.state, (int)props.state == ButtonStateAuto,
            motion.hover.value, motion.press.value, motion.focus.value,
            button.style_kind != 0 ? button.style_kind : StyleKindButton());
        style_font = StyleFontValue(appearance.value.fields,
                                    appearance.value.font_size);
        if(style_font > 0)
            style_font = (int)((float)style_font * GetScale() + 0.5f);
        ButtonFrame frame = BuildFrame(props, input, appearance, motion,
            button.surface_bounds, ColorToInt(ui_app_style().background),
            GetScale(), style_font, GetFontSize());
        frame.appearance = ui_style_apply_effects_frame(frame.appearance);
        frame.material.value = frame.appearance.value;
        frame.material.fill = ui_style_apply_effects_fill(frame.material.fill);
        if(motion.active || (button.props.loading && !button.props.disabled))
            InvalidateTree(INVALIDATE_PAINT);
        text = GetColor(frame.foreground);
        if(foreground != NULL)
            *foreground = text;
        if(focused)
            SetFocusTextInputActive(0);
        int typeface_token = PushTextFont(frame.appearance.value.typeface.data);
        PaintButton(frame,
                    TextWidth(frame.props.label != NULL ? frame.props.label : "",
                              frame.font),
                    GetTime() * 1000.0, button.disclosure,
                    ui_surface_painter, ui_painter);
        ui_draw_button_swatch(frame.props, frame.props.bounds);
        ui_draw_button_image(frame.props, frame.props.bounds, text);
        PopTextFont(typeface_token);
        if(handle_input)
            EndWidget(&widget);
        return handle_input ? input.activated : 0;
    }

    hover_amount = hovered ? 1.0f : 0.0f;

    if(button.props.disabled) {
        background.a = background.a > fallback_policy.disabled_background_alpha
            ? fallback_policy.disabled_background_alpha : background.a;
        text.a = text.a > fallback_policy.disabled_foreground_alpha
            ? fallback_policy.disabled_foreground_alpha : text.a;
    }
    draw_background = ColorLerp(background, hover_background, hover_amount);
    draw_border = ColorLerp(border,
                            LightenColor(hover_background,
                                fallback_policy.hover_border_lighten),
                            hover_amount);
    if(termi_button && !button.props.disabled && retained_pressed)
        draw_background = DarkenColor(draw_background,
                                      fallback_policy.pressed_background_darken);
    if(cues && hovered)
        draw_background = LightenColor(draw_background,
                                       fallback_policy.hover_background_lighten);
    if(termi_button)
        draw_border = hovered
            ? LightenColor(hover_background,
                           fallback_policy.termi_hover_border_lighten)
            : LightenColor(background,
                           fallback_policy.termi_border_lighten);

    ui_draw_control_background(draw_bounds, draw_background, draw_border, radius);
    if(termi_button)
        ui_draw_termi_button_outline(draw_bounds, draw_border, hovered,
                                     retained_pressed,
                                     button.props.disabled,
                                     fallback_policy);
    if(focused) {
        SetFocusTextInputActive(0);
        RenderFocus(draw_bounds);
    }

    if(foreground != NULL)
        *foreground = text;
    ui_draw_button_swatch(button.props, draw_bounds);
    typeface_token = PushTextFont(typeface);
    ui_draw_button_content(&button, draw_bounds, font, text);
    PopTextFont(typeface_token);
    ui_draw_button_image(button.props, draw_bounds, text);
    if(handle_input)
        EndWidget(&widget);
    return handle_input
        ? input.activated : 0;
}

int
ui_button_render(ButtonSpec button)
{
    return ui_render_button(button, 1, 1, 0, 0, NULL);
}

int
HandleButton(ButtonSpec button)
{
    return ui_render_button(button, 1, 0, 0, 0, NULL);
}

Color
ui_paint_button(ButtonSpec button, int hovered, int pressed)
{
    Color foreground = button.paint.foreground;
    (void)ui_render_button(button, 0, 1, hovered, pressed, &foreground);
    return foreground;
}

int
RenderIconAction(IconActionSpec button)
{
    ButtonSpec spec = {0};
    float scale = (float)Scale(1000) / 1000.0f;
    StyleFrame frame = {0};
    IconActionMetrics metrics;
    int icon_size;

    if(scale <= 0.0f)
        scale = 1.0f;
    metrics = IconActionMetricsFor(button.bounds, button.icon_size,
                                   button.icon_padding, scale, frame);
    icon_size = metrics.icon_size;

    spec.props.bounds = button.bounds;
    spec.props.id = button.focus_id;
    spec.props.disabled = button.disabled;
    spec.props.icon = button.icon;
    spec.props.icon_type = button.icon_type;
    spec.props.icon_only = 1;
    spec.paint.icon_size = IconActionStyleIconSize(icon_size, scale);
    spec.props.tone = ButtonToneNeutral;
    spec.props.emphasis = button.background.a != 0
        ? ButtonEmphasisSoft : ButtonEmphasisGhost;
    spec.style_resolved = 1;
    spec.style.normal.fields = StyleIconSize;
    spec.style.normal.icon_size = spec.paint.icon_size;
    if(button.background.a != 0) {
        spec.style.normal.fields |= StyleBackground;
        spec.style.normal.background = button.background;
    }
    if(button.hover_background.a != 0) {
        spec.style.hover.fields |= StyleBackground;
        spec.style.hover.background = button.hover_background;
    }
    if(button.icon_color.a != 0) {
        spec.style.normal.fields |= StyleForeground;
        spec.style.normal.foreground = button.icon_color;
    }
    if(button.border.a != 0) {
        spec.style.normal.fields |= StyleBorder;
        spec.style.normal.border = button.border;
    }
    if(button.radius > 0.0f) {
        spec.style.normal.fields |= StyleRadius;
        spec.style.normal.radius = IconActionStyleRadius(button.radius,
                                                         button.bounds, scale);
    }
    spec.paint.background = button.background;
    spec.hover_background = button.hover_background;
    spec.paint.foreground = button.icon_color;
    spec.paint.border = button.border;
    spec.paint.radius = button.radius;
    return ui_button_render(spec);
}

int
ui_text_button_render(int x, int y, const char *label, int *hover)
{
    Vector2 mouse_world = ui_mouse_world();
    StyleFrame frame = ui_resolve_minimal_control_frame(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindButton());
    int font = ResolveFont(0, StyleFontValue(frame.value.fields,
                                             frame.value.font_size),
                           GetFontSize());
    const char *text = label != NULL ? label : "";
    TextButtonMetrics metrics = TextButtonMetricsFor(
        (float)Scale(1000) / 1000.0f, frame);
    int w = (int)TextWidth(text, font) + metrics.padding_x * 2;
    int h = TextLineHeight(font) + metrics.padding_y * 2;
    Rectangle bounds;
    int hovered;
    ButtonSpec spec;

    if(w < metrics.min_width)
        w = metrics.min_width;
    if(h < metrics.min_height)
        h = metrics.min_height;
    x = x - w / 2;
    bounds.x = (float)x;
    bounds.y = (float)y;
    bounds.width = (float)w;
    bounds.height = (float)h;
    hovered = CheckCollisionPointRec(mouse_world, bounds) &&
              !InputCapturesClick(mouse_world) &&
              HoverEffectsEnabled();
    if(hover != NULL)
        *hover = hovered;
    memset(&spec, 0, sizeof(spec));
    spec.props.bounds = bounds;
    spec.props.label = text;
    spec.style_resolved = 1;
    spec.style_kind = StyleKindButton();
    return ui_button_render(spec);
}

Style
ui_resolve_button_style_kind(ButtonProps button, ButtonState state, int style_kind)
{
    StyleFrame frame;

    frame = ui_resolve_minimal_control_frame(button, state, 0, 0, 0, 0,
        style_kind != 0 ? style_kind : StyleKindButton());
    return ui_unpack_style(ui_style_apply_effects_frame(frame).value);
}

Style
ResolveButtonStyle(ButtonProps button, ButtonState state)
{
    return ui_resolve_button_style_kind(button, state, StyleKindButton());
}

StyleFrame
ui_control_style_frame_kind(ButtonProps button, ButtonState state,
                            int automatic, float h, float p, float f,
                            int style_kind)
{
    return ui_style_apply_effects_frame(ui_resolve_minimal_control_frame(
        button, state, automatic, h, p, f,
        style_kind != 0 ? style_kind : StyleKindButton()));
}

StyleFrame
ui_control_style_frame_role_kind(ButtonProps button, ButtonState state,
                                 int automatic, float h, float p, float f,
                                 int style_kind, int role)
{
    return ui_style_apply_effects_frame(ui_resolve_minimal_control_role_frame(
        button, state, automatic, h, p, f,
        style_kind != 0 ? style_kind : StyleKindButton(), role));
}

StyleFrame
ui_button_style_frame(ButtonProps button, ButtonState state,
                      int automatic, float h, float p, float f)
{
    return ui_control_style_frame_kind(button, state, automatic, h, p, f,
                                       StyleKindButton());
}

static void
segmented_control_style_frame(SegmentedControlProps control, StyleFrame *frame)
{
    if(frame == NULL)
        return;
    *frame = ui_control_style_frame_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium,
                      .class_name = control.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f,
        StyleKindSegmentedControl());
}

static int
segmented_control_font_from_frame(StyleFrame frame)
{
    return ResolveFont(0, StyleFontValue(frame.value.fields,
                                         frame.value.font_size),
                       GetSmallFontSize());
}

static StyleFrame
segmented_item_style_frame(SegmentedControlProps control)
{
    return ui_control_style_frame_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisSoft,
                      .size = ControlSizeMedium,
                      .class_name = control.class_name},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindSegment());
}

static SegmentedMetrics
segmented_control_metrics(SegmentedControlProps control)
{
    StyleFrame control_frame;

    segmented_control_style_frame(control, &control_frame);
    return SegmentedDefaultMetrics(control.height, control.min_item_width,
                                   control.max_item_width,
                                   (float)Scale(1000) / 1000.0f,
                                   control_frame,
                                   segmented_item_style_frame(control));
}

static int
segmented_item_width(SegmentedControlProps control,
                     const SegmentOption *option, int font)
{
    SegmentedMetrics metrics;
    int label_w = TextWidth(option != NULL && option->label != NULL
                                ? option->label
                                : "",
                            font);

    metrics = segmented_control_metrics((SegmentedControlProps){
        .class_name = control.class_name,
        .min_item_width = control.min_item_width,
        .max_item_width = control.max_item_width
    });
    return SegmentedItemWidth(label_w, metrics);
}

int
GetSegmentedControlHeight(SegmentedControlProps control)
{
    StyleFrame frame;
    int font;
    SegmentedMetrics metrics;
    int row_w = 0;
    int rows = 1;

    frame = segmented_item_style_frame(control);
    font = segmented_control_font_from_frame(frame);
    metrics = segmented_control_metrics(control);

    if(control.options == NULL || control.option_count <= 0 ||
       metrics.row_height <= 0)
        return 0;
    if(control.bounds.width <= 0)
        return metrics.row_height;
    if(!control.wrap)
        return metrics.row_height;

    for(int i = 0; i < control.option_count; i++) {
        int item_w = segmented_item_width(control, &control.options[i], font);
        int next_w = SegmentedNextRowWidth(row_w, item_w, metrics.gap);

        if(SegmentedShouldWrap(control.wrap != 0, row_w, next_w,
                               (int)control.bounds.width)) {
            rows++;
            row_w = item_w;
        } else {
            row_w = next_w;
        }
    }

    return SegmentedHeightForRows(rows, metrics.row_height, metrics.gap);
}

SegmentedControlResult
SegmentedControl(SegmentedControlProps control)
{
    SegmentedControlResult result;
    StyleFrame frame;
    int font;
    SegmentedMetrics metrics;
    int row_start = 0;
    int row_w = 0;
    int row_count = 0;
    int y = (int)control.bounds.y;
    int selected = control.selected_index != NULL ? *control.selected_index : -1;

    memset(&result, 0, sizeof(result));
    frame = segmented_item_style_frame(control);
    font = segmented_control_font_from_frame(frame);
    metrics = segmented_control_metrics(control);
    result.selected_index = selected;
    result.clicked_index = -1;
    result.height = GetSegmentedControlHeight(control);

    if(control.options == NULL || control.option_count <= 0 ||
       control.bounds.width <= 0 || metrics.row_height <= 0)
        return result;

    for(int i = 0; i <= control.option_count; i++) {
        int end_row = i == control.option_count;
        int item_w = 0;
        int next_w;

        if(!end_row)
            item_w = segmented_item_width(control, &control.options[i], font);
        next_w = SegmentedNextRowWidth(row_w, item_w, metrics.gap);

        if(!end_row &&
           !SegmentedShouldWrap(control.wrap != 0, row_w, next_w,
                                (int)control.bounds.width)) {
            row_w = next_w;
            row_count++;
            continue;
        }

        if(row_count > 0) {
            SegmentedRow row = SegmentedRowFor(
                control.bounds.x, control.bounds.width, y, row_start,
                row_count, row_w, control.wrap != 0, metrics);
            int button_w = row.button_width;
            int x = row.x;

            for(int j = 0; j < row_count; j++) {
                int item_index = row_start + j;
                const SegmentOption *option = &control.options[item_index];
                int focus_id = SegmentedFocusIdFor(control.id, item_index);
                ButtonSpec button;
                int selected_item = item_index == selected;

                memset(&button, 0, sizeof(button));
                button.props.bounds = (Rectangle){(float)x, (float)y,
                                            (float)button_w,
                                            (float)metrics.row_height};
                button.props.label = option->label;
                button.style.normal.font_size = (float)font;
                button.style.normal.fields |= StyleFontSize;
                button.props.id = focus_id;
                button.props.disabled = option->disabled;
                button.props.tone = selected_item
                    ? ButtonToneAccent : ButtonToneNeutral;
                button.props.emphasis = ButtonEmphasisSoft;
                button.props.selected = selected_item;
                button.props.class_name = control.class_name;
                button.style_kind = StyleKindSegment();
                button.style_resolved = 1;
                if(ui_button_render(button)) {
                    result.clicked_index = item_index;
                    if(control.selected_index != NULL &&
                       *control.selected_index != item_index) {
                        *control.selected_index = item_index;
                        result.changed = 1;
                    }
                    result.selected_index = item_index;
                }
                x += button_w + metrics.gap;
            }
            y += metrics.row_height + metrics.gap;
        }

        row_start = i;
        row_w = item_w;
        row_count = end_row ? 0 : 1;
    }

    return result;
}

int
RenderButtonInfoIndicator(int center_x, int center_y, int diameter)
{
    Vector2 mouse_world = ui_mouse_world();
    float scale = (float)Scale(1000) / 1000.0f;
    StyleFrame frame = ui_control_style_frame_kind(
        (ButtonProps){.tone = ButtonToneNeutral,
                      .emphasis = ButtonEmphasisGhost,
                      .size = ControlSizeSmall,
                      .pill = 1},
        ButtonStateNormal, 0, 0.0f, 0.0f, 0.0f, StyleKindButton());
    InfoIndicatorMetrics metrics =
        InfoIndicatorMetricsFor(diameter, scale, frame);
    int radius;
    int active = 0;
    int hover = 0;
    Rectangle hit;
    Color fill;
    Color stroke;
    Color text;
    int font;

    diameter = metrics.diameter;
    radius = diameter / 2;
    hit = ui_centered_min_hit_rect(center_x - radius, center_y - radius,
                                  diameter, diameter,
                                  metrics.min_touch, metrics.min_touch);

    active = CheckCollisionPointRec(mouse_world, hit) && !InputCapturesClick(mouse_world);
    if(active) {
        hover = HoverEffectsEnabled();
        MarkClickable();
    }

    {
        ButtonProps props = {.tone = ButtonToneNeutral,
                             .emphasis = ButtonEmphasisGhost,
                             .size = ControlSizeSmall,
                             .pill = 1};
        ButtonState state = hover ? ButtonStateHover : ButtonStateNormal;
        Style style = ui_resolve_button_style_kind(props, state,
                                                   StyleKindButton());
        fill = style.background;
        stroke = style.border;
        text = style.foreground;
        font = ResolveFont(0, StyleFontValue(style.fields, style.font_size),
                           GetSmallFontSize());
    }
    DrawCircle(center_x, center_y, radius, fill);
    DrawCircleLines(center_x, center_y, radius, stroke);
    ui_paint_text_box("i",
                      (Rectangle){(float)(center_x - radius),
                                  (float)(center_y - radius),
                                  (float)diameter, (float)diameter},
                      font, text, TextWrapNone, TextAlignCenter,
                      TextAlignCenter, ui_active_font_token(), 0);

    if(active && IsMouseButtonReleased(MOUSE_BUTTON_LEFT)) {
        ConsumeRelease();
        return 1;
    }
    return 0;
}

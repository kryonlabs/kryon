#include "kryon.h"
#include "../src/ui/ui_internal.h"
#include "../src/ui/toolkit_store.h"
#include "runtime/button.h"
#include "runtime/split_button.h"
#include "runtime/instance.h"
#include <stdio.h>
#include <assert.h>

static void advance_store(ToolkitStore *store, int frames)
{
    for(int frame = 0; frame < frames; frame++)
        toolkit_store_frame(store);
}

static void check_motion_store_ownership(void)
{
    typedef struct CounterInstance {
        int count;
    } CounterInstance;
    ToolkitStore *first = toolkit_store_new();
    ToolkitStore *second = toolkit_store_new();
    ToolkitStore *previous = toolkit_store_swap(first);
    InteractionMotion *first_motion = &instance_state(ButtonInstance, 17)->motion;
    first_motion->hover.value = 0.75f;
    first_motion->press.value = 0.25f;
    CounterInstance *counter = instance_state(CounterInstance, 17);
    assert(counter->count == 0);
    counter->count = 42;
    assert(instance_state(CounterInstance, 17)->count == 42);
    assert(instance_state(CounterInstance, UINT64_C(1) << 40)->count == 0);
    assert(first_motion->hover.value == 0.75f);
    assert(!InstanceExpired(0) && !InstanceExpired(12));
    assert(InstanceExpired(13) && InstanceExpired(-1));
    InteractionMotion *collision = &instance_state(ButtonInstance, 17 + 512)->motion;
    assert(collision != first_motion && collision->hover.value == 0);
    collision->focus.value = 0.5f;
    assert(toolkit_store_swap(second) == first);
    InteractionMotion *second_motion = &instance_state(ButtonInstance, 17)->motion;
    assert(second_motion != first_motion && second_motion->hover.value == 0);
    assert(instance_state(CounterInstance, 17)->count == 0);
    second_motion->hover.value = 0.125f;
    assert(toolkit_store_swap(first) == second);
    advance_store(first, 1);
    assert(instance_state(ButtonInstance, 17)->motion.hover.value == 0.75f);
    assert(instance_state(ButtonInstance, 17)->motion.press.value == 0.25f);
    assert(instance_state(ButtonInstance, 17 + 512)->motion.focus.value == 0.5f);
    advance_store(first, 13);
    assert(instance_state(ButtonInstance, 17)->motion.hover.value == 0);
    toolkit_store_swap(second);
    /* Frames in the first host must not age the idle second host. */
    advance_store(second, 1);
    assert(instance_state(ButtonInstance, 17)->motion.hover.value == 0.125f);
    toolkit_store_swap(previous);
    toolkit_store_free(first);
    toolkit_store_free(second);
}

static void check_motion_store_growth(void)
{
    ToolkitStore *store = toolkit_store_new();
    ToolkitStore *previous = toolkit_store_swap(store);
    for(unsigned int key = 1; key <= 1536; key++) {
        InteractionMotion *motion = &instance_state(ButtonInstance, key)->motion;
        motion->hover.value = (float)key / 1536.0f;
    }
    /* Reordering a large set of live widgets must not evict their tracks. */
    advance_store(store, 1);
    for(unsigned int key = 1536; key > 0; key--)
        assert(instance_state(ButtonInstance, key)->motion.hover.value == (float)key / 1536.0f);
    advance_store(store, 14);
    for(unsigned int key = 1; key <= 1536; key++)
        assert(instance_state(ButtonInstance, key)->motion.hover.value == 0);
    instance_state(ButtonInstance, 9)->motion.focus.value = 0.25f;
    instance_state(ButtonInstance, 9 + 1024)->motion.focus.value = 0.5f;
    advance_store(store, 9);
    instance_state(ButtonInstance, 9 + 512)->motion.focus.value = 0.75f;
    advance_store(store, 5);
    assert(instance_state(ButtonInstance, 9 + 512)->motion.focus.value == 0.75f);
    assert(instance_state(ButtonInstance, 9)->motion.focus.value == 0);
    assert(instance_state(ButtonInstance, 9 + 1024)->motion.focus.value == 0);
    toolkit_store_swap(previous);
    toolkit_store_free(store);
}

static void check_composed_button_style(ButtonProps props)
{
    props.bounds = (Rectangle){20, 20, 100, 40};
    props.label = "";
    BeginTree(Key("button-composition-style"));
    props.id = 11;
    Button(props);
    props.id = 12;
    BeginButton(props);
    End();
    EndTree();
    int count = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&count);
    assert(count == 3);
    assert(nodes[0].kind == UI_WIDGET_SCREEN_NODE);
    assert(nodes[1].kind == UI_WIDGET_BUTTON_NODE);
    assert(nodes[2].kind == UI_WIDGET_BUTTON_NODE);
    const ButtonSpec *plain = &nodes[1].data.button.spec;
    const ButtonSpec *composed = &nodes[2].data.button.spec;
    assert(plain->focus_id == 11 && composed->focus_id == 12);
    assert(plain->font == composed->font);
    assert(plain->disabled == composed->disabled);
    assert(plain->loading == composed->loading);
    assert(plain->state == composed->state);
    assert(plain->selected == composed->selected);
    assert(plain->radius == composed->radius);
    assert(plain->border_width == composed->border_width);
    assert(plain->opacity == composed->opacity);
    assert(plain->gap == composed->gap);
    assert(plain->icon_size == composed->icon_size);
    assert(plain->content_offset.x == composed->content_offset.x);
    assert(plain->content_offset.y == composed->content_offset.y);
    assert(ColorToInt(plain->background) == ColorToInt(composed->background));
    assert(ColorToInt(plain->hover_background) == ColorToInt(composed->hover_background));
    assert(ColorToInt(plain->text) == ColorToInt(composed->text));
    assert(ColorToInt(plain->border) == ColorToInt(composed->border));
    assert(ColorToInt(plain->focus) == ColorToInt(composed->focus));
}

static void check_explicit_state_font(void)
{
    for(int state = ButtonStateNormal; state <= ButtonStateSelected; state++) {
        for(int explicit_font = 0; explicit_font <= 13; explicit_font += 13) {
            Style state_style = {.fields = StyleFontSize | StylePaddingX,
                .font_size = 27, .padding_x = 19};
            ButtonProps props = {.label = "Measured", .id = 21, .state = state,
                .font = explicit_font,
                .style.normal = {.fields = StyleFontSize | StylePaddingX,
                    .font_size = 17, .padding_x = 8}};
            props.style.hover = state_style;
            props.style.pressed = state_style;
            props.style.focused = state_style;
            props.style.disabled = state_style;
            props.style.loading = state_style;
            props.style.selected = state_style;
            int font = Scale(state == ButtonStateNormal ? 17 : 27);
            int padding = Scale(state == ButtonStateNormal ? 8 : 19);
            if(explicit_font > 0)
                font = explicit_font;
            BeginTree(Key("explicit-state-font"));
            Button(props);
            EndTree();
            int count = 0;
            const UIWidgetNode *nodes = GetTreeNodes(&count);
            assert(count == 2 && nodes[1].kind == UI_WIDGET_BUTTON_NODE);
            /* The node retains the request; resolving it must still give the
               same font used for measurement, without freezing Style defaults. */
            assert(nodes[1].data.button.spec.font == explicit_font);
            Style resolved = ResolveButtonStyle(props, state);
            assert(ResolveFont(nodes[1].data.button.spec.font,
                Scale(resolved.font_size), GetFontSize()) == font);
            assert(nodes[1].declared_bounds.width == TextWidth(props.label, font) + padding * 2);
        }
    }
}

static void check_natural_height(void)
{
    const float cases[][4] = {{0, 8, 18, 40}, {0, 20, 27, 67},
        {0, -10, 60, 60}, {24, 20, 27, 24}};
    for(int i = 0; i < 4; i++) {
        BeginTree(Key("natural-button-height"));
        Button((ButtonProps){.label = "Run", .id = 922,
            .bounds = {0, 0, 0, cases[i][0]},
            .style.normal = {.fields = StylePaddingY | StyleFontSize,
                .padding_y = cases[i][1], .font_size = cases[i][2]}});
        EndTree();
        int count = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        assert(count == 2 && nodes[1].bounds.height == cases[i][3]);
    }
}

static void check_child_style_padding(void)
{
    const ButtonState states[] = {ButtonStateNormal, ButtonStateHover, ButtonStateNormal};
    for(int i = 0; i < 3; i++) {
        ButtonProps props = {.bounds = {20, 20, 200, 100}, .id = 901, .state = states[i],
            .style.normal = {.fields = StylePaddingX | StylePaddingY, .padding_x = 25, .padding_y = 4},
            .style.hover = {.fields = StylePaddingX | StylePaddingY, .padding_x = 0, .padding_y = 0}};
        BeginTree(Key("child-style-padding"));
        BeginButton(props);
        Column((ColumnProps){0});
        End();
        End();
        EndTree();
        int count = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        assert(count == 3 && nodes[2].kind == UI_WIDGET_COLUMN_NODE);
        Rectangle expected = states[i] == ButtonStateHover
            ? (Rectangle){20, 20, 200, 100} : (Rectangle){45, 24, 150, 92};
        assert(nodes[2].bounds.x == expected.x && nodes[2].bounds.y == expected.y);
        assert(nodes[2].bounds.width == expected.width && nodes[2].bounds.height == expected.height);
    }
}

int main(void)
{
    const int enum_pairs[][2] = {
        {StyleBackground, 1},
        {StyleForeground, 2},
        {StyleBorder, 4},
        {StyleFocus, 8},
        {StyleRadius, 16},
        {StyleBorderWidth, 32},
        {StyleOpacity, 64},
        {StylePaddingX, 128},
        {StylePaddingY, 256},
        {StyleGap, 512},
        {StyleFontSize, 1024},
        {StyleIconSize, 2048},
        {StyleContentOffset, 4096},
        {StyleBackgroundEnd, 8192},
        {StyleMaterial, 16384},
        {ControlSizeMedium, 0},
        {ControlSizeSmall, 1},
        {ControlSizeLarge, 2},
        {ButtonToneNeutral, 0},
        {ButtonToneAccent, 1},
        {ButtonToneDanger, 2},
        {ButtonToneSuccess, 3},
        {ButtonToneWarning, 4},
        {ButtonEmphasisFilled, 0},
        {ButtonEmphasisSoft, 1},
        {ButtonEmphasisOutline, 2},
        {ButtonEmphasisGhost, 3},
        {ButtonEmphasisLink, 4},
        {ButtonStateAuto, 0},
        {ButtonStateNormal, 1},
        {ButtonStateHover, 2},
        {ButtonStatePressed, 3},
        {ButtonStateFocus, 4},
        {ButtonStateDisabled, 5},
        {ButtonStateLoading, 6},
        {ButtonStateSelected, 7},
    };
    for (size_t i = 0; i < sizeof(enum_pairs) / sizeof(enum_pairs[0]); i++) {
        assert(enum_pairs[i][0] == enum_pairs[i][1]);
    }
    for(int state = ButtonStateNormal; state <= ButtonStateSelected; state++) {
        InteractionState preview = ResolveInteraction(state, false, false, true, true, true, false);
        assert(preview.state == state);
        assert(preview.hovered == (state == ButtonStateHover));
        assert(preview.pressed == (state == ButtonStatePressed));
        assert(preview.focused == (state == ButtonStateFocus));
        InteractionState live = ResolveInteraction(ButtonStateAuto, false, false, false, true, true, false);
        assert(live.state == ButtonStateHover);
        assert(live.hovered && !live.pressed && live.focused);
    }
    Rectangle direct_bounds = {0};
    ButtonSpec direct_spec = {0};
    for(int scoped = 0; scoped <= 1; scoped++) {
        BeginDisabled(scoped);
        BeginTree(Key("disabled-button-style"));
        Button((ButtonProps){.label = "Measured", .id = 951, .disabled = !scoped,
            .style = {
                .normal = {.fields = StyleFontSize | StylePaddingX | StylePaddingY,
                    .font_size = 17, .padding_x = 8, .padding_y = 8},
                .disabled = {.fields = StyleFontSize | StylePaddingX | StylePaddingY | StyleForeground,
                    .font_size = 27, .padding_x = 19, .padding_y = 20,
                    .foreground = {17, 34, 51, 128}}}});
        EndTree();
        int count = 0;
        const UIWidgetNode *nodes = GetTreeNodes(&count);
        assert(count == 2 && nodes[1].kind == UI_WIDGET_BUTTON_NODE);
        if(!scoped) {
            direct_bounds = nodes[1].bounds;
            direct_spec = nodes[1].data.button.spec;
            assert(direct_bounds.height == 67);
        } else {
            const ButtonSpec *spec = &nodes[1].data.button.spec;
            assert(nodes[1].bounds.width == direct_bounds.width && nodes[1].bounds.height == direct_bounds.height);
            assert(spec->disabled && ColorToInt(spec->text) == ColorToInt(direct_spec.text));
            assert(ColorToInt(spec->background) == ColorToInt(direct_spec.background));
            assert(ColorToInt(spec->border) == ColorToInt(direct_spec.border));
        }
        EndDisabled();
    }
    const int text_alphas[] = {0, 128, 255};
    for(unsigned int index = 0; index < sizeof(text_alphas) / sizeof(text_alphas[0]); index++) {
        int alpha = text_alphas[index];
        for(int disabled = 0; disabled <= 1; disabled++) {
            BeginTree(Key("shared-text-style"));
            Text((TextProps){.text = "Styled", .font = 13, .color = RED,
                .disabled = disabled,
                .style = {.fields = StyleForeground | StyleFontSize | StyleOpacity,
                    .foreground = {0, 0, 0, alpha}, .font_size = 27, .opacity = 0.5f}});
            EndTree();
            int count = 0;
            const UIWidgetNode *nodes = GetTreeNodes(&count);
            assert(count == 2 && nodes[1].kind == UI_WIDGET_TEXT_NODE);
            int expected_alpha = disabled ? (int)(alpha * 0.45f) : alpha;
            expected_alpha = (int)(expected_alpha * 0.5f);
            assert(nodes[1].data.primitive.font == 27);
            assert(nodes[1].data.primitive.color.r == 0);
            assert(nodes[1].data.primitive.color.a == expected_alpha);
        }
    }
    const float split_cases[][4] = {
        {120, 40, 120, 80},
        {24, 40, 80, 40},
        {80, 40, 80, 40},
        {100.5f, 27.25f, 100.5f, 73.25f},
    };
    for(unsigned int index = 0; index < sizeof(split_cases) / sizeof(split_cases[0]); index++) {
        const float *input = split_cases[index];
        SplitLayout layout = ResolveLayout(input[0], input[1]);
        assert(layout.width == input[2]);
        assert(layout.action_width == input[3]);
        assert(layout.menu_offset == layout.action_width);
        assert(layout.menu_width == input[1]);
        assert(layout.menu_offset + layout.menu_width == layout.width);
        assert(layout.divider_inset == 8);
    }
    ButtonProps request = {.label = "Run", .icon = {.id = 1}};
    Style measure_style = {.padding_x = 16, .padding_y = 20, .icon_size = 8.5f, .gap = 8};
    Rectangle natural = MeasureBounds(request, measure_style, 40, 27, 24, 0, 1, false);
    assert(natural.width == 72.5f && natural.height == 67);
    request.bounds.width = 120;
    request.bounds.height = 24;
    Rectangle fixed = MeasureBounds(request, measure_style, 40, 27, 24, 0, 1, false);
    assert(fixed.width == 120 && fixed.height == 24);
    request.circle = true;
    Rectangle circle = MeasureBounds(request, measure_style, 40, 27, 24, 0, 1, false);
    assert(circle.width == 24 && circle.height == 24);
    request.circle = false;
    request.bounds.width = 0;
    request.full_width = true;
    assert(MeasureBounds(request, measure_style, 40, 27, 24, 300, 1, false).width == 300);
    assert(MeasureBounds(request, measure_style, 40, 27, 24, 10, 1, false).width == 24);
    request.icon_only = true;
    request.bounds.height = 0;
    Rectangle icon_only = MeasureBounds(request, measure_style, 40, 27, 24, 10, 1, false);
    assert(icon_only.height == 40 && icon_only.width == 40);
    request = (ButtonProps){.label = "Run", .bounds = {1.25f, 2.5f, 123.125f, 24.0625f}};
    Rectangle physical = MeasureBounds(request, measure_style, 80, 54, 48, 0, 1.3f, false);
    assert(physical.x == request.bounds.x && physical.y == request.bounds.y);
    assert(physical.width == request.bounds.width && physical.height == request.bounds.height);
    request.bounds.width = 0;
    request.bounds.height = 0;
    Rectangle scaled = MeasureBounds(request, measure_style, 80, 54, 48, 0, 2, true);
    assert(scaled.x == 1.25f && scaled.y == 2.5f && scaled.width == 145 && scaled.height == 134);
    const float icon_sizes[] = {-1, 0, 0.5f, 1, 8.5f, 18};
    for(int i = 0; i < 6; i++) {
        ContentSize measured = MeasureContent(40, 24, icon_sizes[i], 8, 18, true, false);
        float expected_size = icon_sizes[i] > 0 ? icon_sizes[i] : 0;
        assert(measured.icon_size == expected_size);
        if(expected_size == 0) {
            assert(measured.gap == 0 && measured.width == 60);
            Ring ring = LoadingRing(72, 40, icon_sizes[i], 0, 0xffffffffu, 0xffffffffu);
            assert(LoadingPaintRadius(ring) == 0);
        }
    }
    const float padding_cases[] = {-50, -1, 0, 12};
    for (int i = 0; i < 4; i++) {
        float padding = padding_cases[i];
        ContentSize measured = MeasureContent(40, 24, 18, 8, padding, true, false);
        ContentBox content = ContentBounds(measured.width, 40, padding, 0);
        assert(content.width == 50);
    }
    assert(ResolveFont(13, 27, 18) == 13);
    assert(ResolveFont(0, 27, 18) == 27);
    assert(ResolveFont(-1, 27, 18) == 27);
    assert(ResolveFont(0, 0, 18) == 18);
    assert(ResolveFont(0, -1, 18) == 18);
    assert(ResolveFont(0, 0, 0) == 16);
    StyleStates state_styles = {
        .normal = {.fields = 16, .radius = 10},
        .disabled = {.fields = 16, .radius = 0}
    };
    Palette appearance_palette = DefaultPalette(false);
    Metrics appearance_metrics = DefaultMetrics();
    StyleData appearance = ResolveAppearance(1, 0, 2, 0, false, false,
        true, true, true, appearance_palette, appearance_metrics, state_styles);
    StyleData disabled_default = DefaultButtonStyle(1, 0, 5, 0, false, false,
        appearance_palette, appearance_metrics);
    assert(appearance.radius == 0 && appearance.background == disabled_default.background);
    for(int dark = 0; dark < 2; dark++) {
        SetCurrentTheme(0, dark);
        SetThemeMode(dark ? THEME_MODE_DARK : THEME_MODE_LIGHT);
        assert(GetThemeRef() == NULL);
        Palette defaults = DefaultPalette(dark);
        unsigned int colors[] = {defaults.danger, defaults.success, defaults.warning};
        for(int tone = ButtonToneDanger; tone <= ButtonToneWarning; tone++) {
            Style style = ResolveButtonStyle((ButtonProps){.tone = tone}, ButtonStateLoading);
            assert((unsigned int)ColorToInt(style.foreground) == colors[tone - ButtonToneDanger]);
        }
        int amount = LightSurface(ColorToInt(GetThemeSurface())) ? 35 : 90;
        Style disabled = ResolveButtonStyle((ButtonProps){0}, ButtonStateDisabled);
        assert((unsigned int)ColorToInt(disabled.foreground) ==
            MixColor(defaults.text_disabled, ColorToInt(GetThemeText()), amount));
    }
    ContentSize icon_content = MeasureContent(40, 0, 64, 8, 11, true, false);
    assert(icon_content.width == 50 && icon_content.icon_size == 28 && icon_content.gap == 0);
    ButtonContent placed_icon = ContentLayout(icon_content.width, 40, 0, 64,
        8, true, false, false, 0, 0);
    assert(placed_icon.icon_x == 11 && placed_icon.icon_size == icon_content.icon_size);
    check_motion_store_ownership();
    check_motion_store_growth();
    assert(ButtonBackground(ButtonToneAccent, ButtonEmphasisOutline, ButtonStatePressed,
        0x092039ff, 0x006cffff, 0x2184ffff, 0, 0, 0, 0, 0) ==
        MixColor(0x092039ff, 0x006cffff, 12));
    assert(ButtonBackground(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateHover,
        0x092039ff, 0x006cffff, 0x2184ffff, 0, 0, 0, 0, 0) == 0x0570ffff);
    assert(ButtonBackground(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateNormal,
        0x092039ff, 0x006cffff, 0x2184ffff, 0, 0, 0, 0, 0) == 0x006cffff);
    assert(ButtonBackground(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateHover,
        0xffffffff, 0x006cffff, 0x2184ffff, 0, 0, 0, 0, 0) == 0x87bdffff);
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateLoading,
        0x092039ff, 0x006cffff, 0, 0, 0, 0) == 0x082c59ff);
    const unsigned int disabled_surfaces[] = {0x092039ff, 0xffffffff};
    for (int index = 0; index < 2; index++) {
        unsigned int surface = disabled_surfaces[index];
        assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisOutline, ButtonStateDisabled,
            surface, 0x006cffff, 0, 0, 0, 0) == MixColor(surface, 0x006cffff, 25));
        assert(ButtonBorder(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateDisabled,
            surface, 0x006cffff, 0x183858ff, 0, 0, 0) == MixColor(surface, 0x183858ff, 45));
    }
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisOutline, ButtonStateLoading,
        0x092039ff, 0x006cffff, 0, 0, 0, 0) == 0x082c5900);
    assert((ButtonBorder(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateLoading,
        0x092039ff, 0x006cffff, 0x183858ff, 0, 0, 0) & 255) == 0);
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateNormal,
        0x092039ff, 0x006cffff, 0, 0, 0, 0) == 0x006cffff);
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateLoading,
        0xffffffff, 0x006cffff, 0, 0, 0, 0) == 0xe0edffff);
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisOutline, ButtonStateLoading,
        0xffffffff, 0x006cffff, 0, 0, 0, 0) == 0xbfdaffff);
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisFilled, ButtonStateNormal,
        0xffffffff, 0x006cffff, 0, 0, 0, 0) == 0x006cffff);
    SetTheme(ThemeDefaultLight());
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisFilled, ButtonStatePressed,
        0xf8fbffff, 0xaa2ddcff, 0, 0, 0, 0) ==
        MixColor(0xf8fbffff, 0xaa2ddcff, 25));
    assert(ButtonBorder(ButtonToneAccent, ButtonEmphasisOutline, ButtonStatePressed,
        0xf8fbffff, 0xaa2ddcff, 0, 0, 0, 0) ==
        MixColor(0xf8fbffff, 0xaa2ddcff, 25));
    assert(ButtonBorder(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateNormal,
        0xf8fbffff, 0xaa2ddcff, 0xeeeeeeff, 0, 0, 0) ==
        MixColor(0xf8fbffff, 0xaa2ddcff, 14));
    assert(ButtonBorder(ButtonToneDanger, ButtonEmphasisSoft, ButtonStateNormal,
        0xf8fbffff, 0xaa2ddcff, 0xeeeeeeff, 0xff0017ff, 0, 0) ==
        MixColor(0xf8fbffff, 0xff0017ff, 14));
    assert(ButtonBorder(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateHover,
        0x092039ff, 0x006cffff, 0x183858ff, 0, 0, 0) ==
        MixColor(0x183858ff, 0x006cffff, 45));
    assert(ButtonBorder(ButtonToneNeutral, ButtonEmphasisSoft, ButtonStateNormal,
        0x092039ff, 0x006cffff, 0x183858ff, 0, 0, 0) ==
        MixColor(0x092039ff, 0x183858ff, 28));
    ButtonProps rounded_outline = {.emphasis = ButtonEmphasisOutline};
    for(int size = 0; size <= 2; size++) {
        for(int state = ButtonStateNormal; state <= ButtonStateSelected; state++) {
            ButtonProps rounded = {.size = size};
            ThemeMetrics metrics = GetThemeMetrics();
            float expected = metrics.radius_medium;
            if(size == ControlSizeMedium && (state == ButtonStateNormal || state == ButtonStateHover))
                expected = (metrics.radius_medium + metrics.radius_large) * 0.5f;
            assert(ResolveButtonStyle(rounded, state).radius == expected);
            rounded.style.normal.fields = StyleRadius;
            assert(ResolveButtonStyle(rounded, state).radius == 0);
        }
    }
    assert(ResolveButtonStyle(rounded_outline, ButtonStateNormal).radius == GetThemeMetrics().radius_large);
    assert(ResolveButtonStyle(rounded_outline, ButtonStateHover).radius == GetThemeMetrics().radius_large);
    assert(ResolveButtonStyle(rounded_outline, ButtonStateFocus).radius == GetThemeMetrics().radius_medium);
    rounded_outline.style.normal.fields = StyleRadius;
    rounded_outline.style.normal.radius = 0;
    assert(ResolveButtonStyle(rounded_outline, ButtonStateNormal).radius == 0);
    BeginTree(Key("text-inheritance"));
    BeginButton((ButtonProps){.bounds = {20, 20, 200, 100}, .font = 27, .id = 991,
        .style.normal = {.fields = StyleForeground, .foreground = {17, 34, 51, 0}}});
    Text((TextProps){.text = "Inherited", .wrap = TextWrapNone});
    Column((ColumnProps){.bounds = {0, 0, 180, 60}});
    Text((TextProps){.text = "Nested", .wrap = TextWrapNone});
    Text((TextProps){.text = "Explicit", .font = 13, .color = {68, 85, 102, 255}, .wrap = TextWrapNone});
    End();
    End();
    EndTree();
    int node_count = 0;
    int text_count = 0;
    const UIWidgetNode *nodes = GetTreeNodes(&node_count);
    for (int i = 0; i < node_count; i++) {
        if (nodes[i].kind == UI_WIDGET_TEXT_NODE) {
            assert(nodes[i].data.primitive.font == (text_count < 2 ? 27 : 13));
            assert((unsigned int)ColorToInt(nodes[i].data.primitive.color) ==
                (text_count < 2 ? 0x11223300u : 0x445566ffu));
            text_count++;
        }
    }
    assert(text_count == 3);
    for (int tone = ButtonToneDanger; tone <= ButtonToneWarning; tone++) {
        ButtonProps props = {.tone = tone, .emphasis = ButtonEmphasisFilled};
        Style normal = ResolveButtonStyle(props, ButtonStateNormal);
        Style hover = ResolveButtonStyle(props, ButtonStateHover);
        assert(hover.background.r >= normal.background.r);
        assert(hover.background.g >= normal.background.g);
        assert(hover.background.b >= normal.background.b);
        assert(ColorToInt(hover.background) != ColorToInt(normal.background));
        assert(hover.background.a == normal.background.a);
        assert(ColorToInt(hover.foreground) == ColorToInt(normal.foreground));
    }
    ButtonProps outline = {.tone = ButtonToneAccent, .emphasis = ButtonEmphasisOutline};
    ButtonProps filled = {.tone = ButtonToneAccent, .emphasis = ButtonEmphasisFilled};
    Style outline_press = ResolveButtonStyle(outline, ButtonStatePressed);
    Style filled_press = ResolveButtonStyle(filled, ButtonStatePressed);
    assert(outline_press.background.r > filled_press.background.r);
    assert(outline_press.background.g > filled_press.background.g);
    ButtonProps focused = {.tone = ButtonToneSuccess, .emphasis = ButtonEmphasisFilled};
    Style paint = ResolveButtonStyle(focused, ButtonStateFocus);
    Color success_focus = GetTheme().colors.success;
    success_focus.a = GetTheme().colors.focus.a;
    assert(ColorToInt(paint.focus) == ColorToInt(success_focus));
    focused.emphasis = ButtonEmphasisGhost;
    paint = ResolveButtonStyle(focused, ButtonStateFocus);
    assert(paint.focus.a == GetTheme().colors.focus.a * 18 / 100);
    focused.style.focused.fields = StyleFocus;
    focused.style.focused.focus = (Color){210, 20, 170, 128};
    paint = ResolveButtonStyle(focused, ButtonStateFocus);
    assert(ColorToInt(paint.focus) == 0xd214aa80u);
    focused.style.focused.focus = (Color){0};
    assert(ResolveButtonStyle(focused, ButtonStateFocus).focus.a == 0);
    Theme dark_theme = ThemeDefaultDark();
    for (int tone = ButtonToneDanger; tone <= ButtonToneWarning; tone++) {
        SetTheme(dark_theme);
        Color semantic = dark_theme.colors.danger;
        if (tone == ButtonToneSuccess)
            semantic = dark_theme.colors.success;
        if (tone == ButtonToneWarning)
            semantic = dark_theme.colors.warning;
        ButtonProps loading = {.tone = tone, .loading = true};
        Style material = ResolveButtonStyle(loading, ButtonStateAuto);
        assert((unsigned int)ColorToInt(material.background) ==
            MixColor(ColorToInt(dark_theme.colors.surface), ColorToInt(semantic), 14));
        loading.style.loading.fields = StyleBackground;
        loading.style.loading.background = (Color){20, 40, 60, 0};
        material = ResolveButtonStyle(loading, ButtonStateAuto);
        assert(ColorToInt(material.background) == ColorToInt(loading.style.loading.background));
    }
    unsigned char focus_alpha[] = {0, 128, 255};
    for (int alpha = 0; alpha < 3; alpha++) {
        dark_theme.colors.focus.a = focus_alpha[alpha];
        SetTheme(dark_theme);
        for (int tone = ButtonToneDanger; tone <= ButtonToneWarning; tone++) {
            ButtonProps props = {.tone = tone};
            Style style = ResolveButtonStyle(props, ButtonStateFocus);
            unsigned int expected_focus = (unsigned int)ColorToInt(dark_theme.colors.focus);
            if (tone == ButtonToneSuccess) {
                expected_focus = 0x00ffc300u | focus_alpha[alpha];
            }
            assert((unsigned int)ColorToInt(style.focus) == expected_focus);
        }
    }
    for(int dark = 0; dark < 2; dark++) {
        SetTheme(dark ? ThemeDefaultDark() : ThemeDefaultLight());
        for(int tone = 0; tone < 5; tone++) {
            for(int emphasis = 0; emphasis < 5; emphasis++) {
                for(int state = 1; state <= 7; state++) {
                    for(int size = 0; size < 3; size++) {
                        ButtonProps props = {.tone = tone, .emphasis = emphasis, .size = size};
                        props.state = state;
                        check_composed_button_style(props);
                        Style style = ResolveButtonStyle(props, state);
                        printf("[%d,%d,%d,%d,%d,%u,%u,%u,%u,%u,"
                               "%g,%g,%g,%g,%g,%g,%g,%g,%g,%g,%u,%d]\n",
                            dark, tone, emphasis, state, size, style.fields,
                            ColorToInt(style.background), ColorToInt(style.foreground),
                            ColorToInt(style.border), ColorToInt(style.focus),
                            style.radius, style.border_width, style.opacity,
                            style.padding_x, style.padding_y, style.gap,
                            style.font_size, style.icon_size,
                            style.content_offset.x, style.content_offset.y,
                            ColorToInt(style.background_end), style.material);
                    }
                }
            }
        }
    }
    check_explicit_state_font();
    check_natural_height();
    check_child_style_padding();
    return ferror(stdout) ? 1 : 0;
}

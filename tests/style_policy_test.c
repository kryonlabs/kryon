#include "runtime/style.h"
#include <assert.h>

int main(void)
{
    for (int state = 0; state <= 7; state++) {
        for (int bits = 0; bits < 64; bits++) {
            bool disabled = (bits & 1) != 0;
            bool loading = (bits & 2) != 0;
            bool pressed = (bits & 4) != 0;
            bool hovered = (bits & 8) != 0;
            bool focused = (bits & 16) != 0;
            bool selected = (bits & 32) != 0;
            int expected = ButtonStateNormal;
            if (selected) expected = ButtonStateSelected;
            if (focused) expected = ButtonStateFocus;
            if (hovered) expected = ButtonStateHover;
            if (pressed) expected = ButtonStatePressed;
            if (state != ButtonStateAuto) expected = state;
            if (loading) expected = ButtonStateLoading;
            if (disabled) expected = ButtonStateDisabled;
            InteractionState result = ResolveInteraction(state, disabled, loading,
                pressed, hovered, focused, selected);
            assert(result.state == expected);
            assert(result.hovered == (state == ButtonStateAuto ? hovered : expected == ButtonStateHover));
            assert(result.pressed == (state == ButtonStateAuto ? pressed : expected == ButtonStatePressed));
            assert(result.focused == (state == ButtonStateAuto ? focused : expected == ButtonStateFocus));
        }
        for (int bits = 0; bits < 8; bits++) {
            StateFlags flags = ResolveFlags(state, bits & 1, bits & 2, bits & 4);
            assert(flags.disabled == ((bits & 1) != 0 || state == ButtonStateDisabled));
            assert(flags.loading == ((bits & 2) != 0 || state == ButtonStateLoading));
            assert(flags.selected == ((bits & 4) != 0 || state == ButtonStateSelected));
        }
    }
    assert(DefaultFields() == 24575u);
    assert((DefaultFields() & StyleBackgroundEnd) == 0);
    StyleData named = {.fields = StyleTypeface, .typeface = {"semibold", 8}};
    StyleData other = {.typeface = {"ignored", 7}};
    assert(StringEqual(MergeValues(named, other).typeface, named.typeface));
    other.fields = StyleTypeface;
    assert(StringEqual(MergeValues(named, other).typeface, other.typeface));
    other.typeface = StringView(NULL, 0);
    assert(StringEqual(MergeValues(named, other).typeface, StringView("", 0)));
    assert(SizeValue(ControlSizeMedium, 32, 40, 48) == 40);
    assert(SizeValue(ControlSizeSmall, 32, 40, 48) == 32);
    assert(SizeValue(ControlSizeLarge, 32, 40, 48) == 48);
    assert(SizeValue(-1, 32, 40, 48) == 40);
    assert(SizeValue(99, 32, 40, 48) == 40);
    assert(FitHeight(0, 40, 18, 8) == 40);
    assert(FitHeight(0, 40, 27, 20) == 67);
    assert(FitHeight(0, 40, 60, -10) == 60);
    assert(FitHeight(24, 40, 27, 20) == 24);
    assert(FitHeight(0, 0, -1, -1) == 0);
    Rectangle content = ContentBounds(20, 10, 30, 40);
    assert(content.x == 30 && content.y == 40 && content.width == 0 && content.height == 0);
    content = ContentBounds(20, 10, -1, -2);
    assert(content.x == 0 && content.y == 0 && content.width == 20 && content.height == 10);
    StyleData base = {.fields = 8191, .background = 0x102030ff,
        .radius = 8, .opacity = 1, .offset_x = 3, .offset_y = -2};
    StyleData transparent = {.fields = 1 | 16 | 64 | 4096};
    StyleData result = MergeValues(base, transparent);
    assert(result.fields == 8191);
    assert(result.background == 0 && result.radius == 0 && result.opacity == 0);
    assert(result.offset_x == 0 && result.offset_y == 0);
    StyleData absent = {.radius = 99};
    assert(MergeValues(base, absent).radius == 8);
    StyleData gradient = {.fields = 8192, .background_end = 0x10203000};
    assert(MergeValues(base, gradient).background_end == 0x10203000);
    gradient.background_end = 0;
    assert(MergeValues(base, gradient).background_end == 0);
    StyleData normal = {.fields = 16, .radius = 1};
    StyleData hover = {.fields = 16, .radius = 2};
    StyleData pressed = {.fields = 16, .radius = 3};
    StyleData focused = {.fields = 16, .radius = 4};
    StyleData disabled = {.fields = 16, .radius = 5};
    StyleData loading = {.fields = 16, .radius = 6};
    StyleData selected = {.fields = 16, .radius = 7};
    StyleStates states = {.normal = normal, .hover = hover, .pressed = pressed,
        .focused = focused, .disabled = disabled, .loading = loading, .selected = selected};
    for(int state = 1; state <= 7; state++) {
        result = ResolveValues(base, states, state);
        assert(result.radius == state);
        assert(result.background == base.background);
    }
    StyleData layout = {.fields = 8192, .padding_x = 11, .padding_y = 13,
        .font_size = 19, .icon_size = 18, .gap = 7, .background_end = 0x12345600};
    normal = (StyleData){.background = 0x112233ff, .radius = 2};
    hover = (StyleData){.background = 0x44556680, .radius = 4};
    focused = (StyleData){.background = 0x77889900, .radius = 6};
    pressed = (StyleData){.background = 0, .radius = 8};
    result = TransitionValues(layout, normal, hover, pressed, focused, 0.5f, 0.5f, 0.5f);
    assert(result.radius == 6);
    assert(result.fields == layout.fields && result.background_end == layout.background_end);
    assert(result.padding_x == 11 && result.padding_y == 13);
    assert(result.font_size == 19 && result.icon_size == 18 && result.gap == 7);
    assert(TransitionValues(layout, normal, hover, pressed, focused, 0, 0, 0).background == normal.background);
    assert(TransitionValues(layout, normal, hover, pressed, focused, 0, 0, 1).background == focused.background);
    assert(TransitionValues(layout, normal, hover, pressed, focused, 1, 0, 1).background == hover.background);
    assert(TransitionValues(layout, normal, hover, pressed, focused, 1, 1, 1).background == 0);
    normal.fields = 8192;
    normal.background_end = 0x12345600;
    focused.fields = 8192;
    focused.background_end = 0;
    StyleFrame frame = TransitionFrame(layout, normal, hover, pressed, focused, 0.5f, 0.25f, 0.75f);
    assert(frame.value.padding_x == 11 && frame.value.font_size == 19);
    assert(frame.value.background == TransitionValues(layout, normal, hover, pressed, focused, 0.5f, 0.25f, 0.75f).background);
    assert(frame.fill.normal && !frame.fill.hover && !frame.fill.press && frame.fill.focus);
    assert(frame.fill.normal_end == 0x12345600 && frame.fill.focus_end == 0);
    assert(frame.fill.hover_start == hover.background && frame.fill.press_start == 0);
    assert(frame.fill.hover_amount == 0.5f && frame.fill.press_amount == 0.25f && frame.fill.focus_amount == 0.75f);
    return 0;
}

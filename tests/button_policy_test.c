#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>

#include "runtime/button.h"

Activation
ReadActivation(Rectangle bounds, int32_t id, bool enabled)
{
    (void)bounds;
    (void)id;
    (void)enabled;
    return (Activation){0};
}

int32_t
MeasureTextWidth(const char *text, int32_t font, const char *typeface)
{
    (void)text;
    (void)font;
    (void)typeface;
    return 0;
}

void *
InstanceState(const char *type, uint64_t key, size_t size)
{
    static unsigned char storage[4096];

    (void)type;
    (void)key;
    assert(size <= sizeof(storage));
    return storage;
}

int
main(void)
{
    StyleFacts facts = ButtonRoleFactsFor(0, 11, 22, StyleAny(),
        ButtonToneNeutral, ButtonEmphasisSoft, ControlSizeMedium,
        ButtonStateNormal);

    assert(facts.kind == StyleKindButton());
    assert(facts.name == 11);
    assert(facts.class_name == 22);
    assert(facts.role == StyleAny());
    assert(facts.tone == ButtonToneNeutral);
    assert(facts.emphasis == ButtonEmphasisSoft);
    assert(facts.size == ControlSizeMedium);
    assert(facts.state == ButtonStateNormal);

    facts = ButtonRoleFactsFor(StyleKindDropdown(), 0, 33,
        StyleAny(), ButtonToneAccent, ButtonEmphasisFilled, ControlSizeLarge,
        ButtonStateHover);
    assert(facts.kind == StyleKindDropdown());
    assert(facts.class_name == 33);
    assert(facts.tone == ButtonToneAccent);
    assert(facts.emphasis == ButtonEmphasisFilled);
    assert(facts.size == ControlSizeLarge);
    assert(facts.state == ButtonStateHover);

    assert(ButtonActionEnabled(true, false) == false);
    assert(ButtonActionEnabled(false, false) == true);
    assert(ButtonActionEnabled(false, true) == false);
    assert(ButtonArrowGlyph(ArrowRight) == 62);
    assert(ButtonArrowGlyph(ArrowUp) == 94);
    assert(ButtonArrowGlyph(ArrowDown) == 118);
    assert(ButtonArrowGlyph(ArrowLeft) == 60);
    StyleFrame appearance = {0};
    appearance.value.radius = 8;
    ButtonFrame circle = BuildFrame((ButtonProps){
        .bounds = {0, 0, 96, 96}, .circle = true},
        (ButtonInput){0}, appearance, (InteractionMotion){0},
        (Rectangle){0}, 0, 2, 16, 16);
    assert(circle.material.value.radius == 24);
    assert(circle.appearance.value.radius == 24);
    return 0;
}

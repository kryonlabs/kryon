#include "runtime/style_sheet.h"
#include <assert.h>

static StyleData
color_style(unsigned int field, unsigned int value)
{
    StyleData style = {0};
    style.fields = field;
    if(field == StyleBackground)
        style.background = value;
    if(field == StyleForeground)
        style.foreground = value;
    if(field == StyleBorder)
        style.border = value;
    if(field == StyleFocus)
        style.focus = value;
    return style;
}

static StyleData
resolve(StyleData base, StyleFacts facts, const StyleRule *rules,
        int rule_count, int state)
{
    StyleCascade cascade = BeginStyleCascade(base);
    for(int i = 0; i < rule_count; i++)
        cascade = ApplyStyleRule(cascade, rules[i], facts, state);
    return FinishStyleCascade(cascade);
}

int
main(void)
{
    StyleSelector any = StyleDefaultSelector();
    StyleSelector button = StyleDefaultSelector();
    StyleSelector primary = StyleDefaultSelector();
    StyleSelector save = StyleDefaultSelector();
    StyleSelector accent = StyleDefaultSelector();
    StyleFacts facts = StyleDefaultFacts(StyleKindButton());
    StyleRule rules[8] = {0};
    StyleData base = {0};
    StyleData result;

    facts.name = 17;
    facts.class_name = 3;
    facts.tone = ButtonToneAccent;
    facts.emphasis = ButtonEmphasisFilled;
    facts.size = ControlSizeMedium;
    facts.state = ButtonStateHover;

    assert(StyleSelectorMatches(any, facts));
    assert(StyleSelectorSpecificity(any) == 0);

    button.kind = StyleKindButton();
    primary.class_name = 3;
    save.name = 17;
    accent.tone = ButtonToneAccent;

    assert(StyleSelectorMatches(button, facts));
    assert(StyleSelectorMatches(primary, facts));
    assert(StyleSelectorMatches(save, facts));
    assert(StyleSelectorMatches(accent, facts));
    assert(StyleSelectorSpecificity(button) == 1);
    assert(StyleSelectorSpecificity(accent) == 10);
    assert(StyleSelectorSpecificity(primary) == 20);
    assert(StyleSelectorSpecificity(save) == 100);

    rules[0].selector = button;
    rules[0].state = StyleStateAny();
    rules[0].layer = 0;
    rules[0].order = 0;
    rules[0].style = color_style(StyleBackground | StyleForeground,
                                 0x111111ffu);
    rules[0].style.foreground = 0xeeeeeeffu;

    rules[1].selector = primary;
    rules[1].state = StyleStateAny();
    rules[1].layer = 0;
    rules[1].order = 1;
    rules[1].style = color_style(StyleBackground, 0x222222ffu);

    rules[2].selector = button;
    rules[2].state = ButtonStateHover;
    rules[2].layer = 0;
    rules[2].order = 2;
    rules[2].style = color_style(StyleBackground, 0x333333ffu);

    rules[3].selector = save;
    rules[3].state = StyleStateAny();
    rules[3].layer = 0;
    rules[3].order = 3;
    rules[3].style = color_style(StyleBorder, 0x444444ffu);

    rules[4].selector = primary;
    rules[4].state = ButtonStateHover;
    rules[4].layer = 0;
    rules[4].order = 4;
    rules[4].style = color_style(StyleBackground, 0x555555ffu);

    rules[5].selector = primary;
    rules[5].state = ButtonStatePressed;
    rules[5].layer = 0;
    rules[5].order = 5;
    rules[5].style = color_style(StyleBackground, 0x666666ffu);

    rules[6].selector = primary;
    rules[6].state = ButtonStateNormal;
    rules[6].layer = 1;
    rules[6].order = 0;
    rules[6].style.fields = StyleRadius | StylePaddingX;
    rules[6].style.radius = 6.0f;
    rules[6].style.padding_x = 12.0f;

    rules[7].selector = primary;
    rules[7].state = StyleStateAny();
    rules[7].layer = 1;
    rules[7].order = 1;
    rules[7].style.fields = StylePaddingX;
    rules[7].style.padding_x = 16.0f;

    base.fields = StyleOpacity;
    base.opacity = 1.0f;

    result = resolve(base, facts, rules, 8, ButtonStateHover);
    assert(result.background == 0x555555ffu);
    assert(result.foreground == 0xeeeeeeffu);
    assert(result.border == 0x444444ffu);
    assert(result.radius == 6.0f);
    assert(result.padding_x == 16.0f);
    assert(result.opacity == 1.0f);
    assert((result.fields & StyleOpacity) != 0);

    result = resolve(base, facts, rules, 8, ButtonStatePressed);
    assert(result.background == 0x666666ffu);

    facts.class_name = 99;
    result = resolve(base, facts, rules, 8, ButtonStateHover);
    assert(result.background == 0x333333ffu);
    assert(result.border == 0x444444ffu);

    facts.kind = StyleKindText();
    result = resolve(base, facts, rules, 8, ButtonStateHover);
    assert(result.background == 0);
    assert(result.foreground == 0);
    assert(result.opacity == 1.0f);

    return 0;
}

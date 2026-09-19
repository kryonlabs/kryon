#include "ui_style_sheet.h"
#include <assert.h>

static StyleRule
background_rule(int kind, unsigned int color)
{
    StyleRule rule = {0};

    rule.selector = StyleDefaultSelector();
    rule.selector.kind = kind;
    rule.state = StyleStateAny();
    rule.style.fields = StyleBackground;
    rule.style.background = color;
    return rule;
}

static void
test_cached_resolution(void)
{
    StyleRule rules[4] = {
        background_rule(StyleKindButton(), 0x112233ffu),
        background_rule(StyleKindButton(), 0x334455ffu),
        background_rule(StyleKindButton(), 0x556677ffu),
        background_rule(StyleKindButton(), 0),
    };
    StyleSheet sheet = {rules, 4};
    StylePack pack = {.id = "cache", .sheet = &sheet};
    rules[1].selector.class_name = 7;
    rules[2].state = ButtonStateHover;
    rules[2].order = 2;
    rules[3].selector.role = 3;
    rules[3].order = 3;
    ClearStylePacks();
    assert(RegisterStylePack(pack));
    for(int i = 0; i < 2000; i++) {
        StyleFacts facts = StyleControlRoleFacts(StyleKindButton(), i % 5,
            i % 8, i % 4, i % 3, i % 5, i % 3, i % 8);
        facts.validation = i % 3;
        facts.orientation = i % 2;
        facts.placement = i % 4;
        for(int repeat = 0; repeat < 2; repeat++) {
            StyleData base = {.fields = StyleOpacity | StyleRadius,
                .opacity = repeat ? 0.25f : 1.0f, .radius = (float)i,
                .foreground = (unsigned int)(i + repeat)};
            StyleData expected = ResolveStyle(&sheet, base, facts, i % 8);
            StyleData actual = ResolveActiveStyle(base, facts, i % 8);
            assert(actual.fields == expected.fields);
            assert(actual.background == expected.background);
            assert(actual.foreground == expected.foreground);
            assert(actual.radius == expected.radius);
            assert(actual.opacity == expected.opacity);
        }
    }
    StyleFacts facts = StyleDefaultFacts(StyleKindButton());
    assert(ResolveActiveStyle((StyleData){0}, facts, ButtonStateNormal).background == 0x112233ffu);
    rules[0].style.background = 0xaabbccffu;
    assert(RegisterStylePack(pack));
    assert(ResolveActiveStyle((StyleData){0}, facts, ButtonStateNormal).background == 0xaabbccffu);
    ClearStylePacks();
    assert(ResolveActiveStyle((StyleData){0}, facts, ButtonStateNormal).fields == 0);
}

int
main(void)
{
    test_cached_resolution();
    StyleRule first_rules[1] = {
        background_rule(StyleKindButton(), 0x111111ffu),
    };
    StyleRule glow_rules[1] = {
        background_rule(StyleKindButton(), 0x222222ffu),
    };
    StyleSheet first_sheet = {.rules = first_rules, .rule_count = 1};
    StyleSheet glow_sheet = {.rules = glow_rules, .rule_count = 1};
    StylePackOption options[4] = {0};
    StyleFacts button_facts = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneNeutral, ButtonEmphasisSoft, ControlSizeMedium,
        ButtonStateNormal);
    StyleFacts text_facts = StyleTextFacts(0, 0, 0, ButtonStateNormal);
    StyleData base = {0};
    StyleData resolved;
    uint64_t version;

    base.fields = StyleOpacity;
    base.opacity = 1.0f;

    ClearStylePacks();
    version = StylePackVersion();
    assert(GetStylePackCount() == 0);
    assert(GetActiveStylePack() == NULL);
    resolved = ResolveActiveStyle(base, button_facts, ButtonStateNormal);
    assert(resolved.background == 0);
    assert(resolved.opacity == 1.0f);
    assert(!RegisterStylePack((StylePack){0}));
    assert(StylePackVersion() == version);

    assert(RegisterStylePack((StylePack){
        .id = "first",
        .label = "First",
        .description = "Current default Kryon look",
        .sheet = &first_sheet,
    }));
    assert(GetStylePackCount() == 1);
    assert(GetActiveStylePack() != NULL);
    assert(GetActiveStylePack()->sheet == &first_sheet);
    assert(GetActiveStylePackId() != NULL);
    assert(StylePackVersion() == version + 1);
    resolved = ResolveActiveStyle(base, button_facts, ButtonStateNormal);
    assert(resolved.background == 0x111111ffu);
    assert(resolved.opacity == 1.0f);
    resolved = ResolveActiveStyle(base, text_facts, ButtonStateNormal);
    assert(resolved.background == 0);

    assert(RegisterStylePack((StylePack){
        .id = "glow",
        .label = "Glow",
        .description = "Current glow treatment",
        .sheet = &glow_sheet,
    }));
    assert(GetStylePackCount() == 2);
    assert(FindStylePack("glow")->sheet == &glow_sheet);
    assert(GetActiveStylePack()->sheet == &first_sheet);

    version = StylePackVersion();
    assert(SetActiveStylePack("glow"));
    assert(StylePackVersion() == version + 1);
    assert(GetActiveStylePack()->sheet == &glow_sheet);
    resolved = ResolveActiveStyle(base, button_facts, ButtonStateNormal);
    assert(resolved.background == 0x222222ffu);
    assert(!SetActiveStylePack("missing"));
    assert(StylePackVersion() == version + 1);
    assert(SetActiveStylePack("glow"));
    assert(StylePackVersion() == version + 1);

    assert(GetStylePackOptions(options, 4) == 2);
    assert(options[0].id != NULL && !options[0].active);
    assert(options[1].id != NULL && options[1].active);

    assert(RegisterStylePack((StylePack){
        .id = "glow",
        .label = "Glow Updated",
        .description = "Replacement",
        .sheet = &first_sheet,
    }));
    assert(GetStylePackCount() == 2);
    assert(GetActiveStylePack()->sheet == &first_sheet);
    assert(GetActiveStylePack()->label[5] == 'U');
    resolved = ResolveStyle(&glow_sheet, base, button_facts,
                            ButtonStateNormal);
    assert(resolved.background == 0x222222ffu);
    resolved = ResolveActiveStyle(base, button_facts, ButtonStateNormal);
    assert(resolved.background == 0x111111ffu);

    ClearStylePacks();
    assert(GetStylePackCount() == 0);
    assert(GetActiveStylePack() == NULL);

    return 0;
}

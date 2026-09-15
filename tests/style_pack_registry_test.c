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

int
main(void)
{
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

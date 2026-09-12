#include "ui_style_sheet.h"
#include <assert.h>

int
main(void)
{
    StyleRule vanilla_rules[1] = {0};
    StyleRule glow_rules[1] = {0};
    StyleSheet vanilla_sheet = {.rules = vanilla_rules, .rule_count = 1};
    StyleSheet glow_sheet = {.rules = glow_rules, .rule_count = 1};
    StylePackOption options[4] = {0};
    uint64_t version;

    ClearStylePacks();
    version = StylePackVersion();
    assert(GetStylePackCount() == 0);
    assert(GetActiveStylePack() == NULL);
    assert(!RegisterStylePack((StylePack){0}));
    assert(StylePackVersion() == version);

    assert(RegisterStylePack((StylePack){
        .id = "vanilla",
        .label = "Vanilla",
        .description = "Current default Kryon look",
        .sheet = &vanilla_sheet,
    }));
    assert(GetStylePackCount() == 1);
    assert(GetActiveStylePack() != NULL);
    assert(GetActiveStylePack()->sheet == &vanilla_sheet);
    assert(GetActiveStylePackId() != NULL);
    assert(StylePackVersion() == version + 1);

    assert(RegisterStylePack((StylePack){
        .id = "glow",
        .label = "Glow",
        .description = "Current glow treatment",
        .sheet = &glow_sheet,
    }));
    assert(GetStylePackCount() == 2);
    assert(FindStylePack("glow")->sheet == &glow_sheet);
    assert(GetActiveStylePack()->sheet == &vanilla_sheet);

    version = StylePackVersion();
    assert(SetActiveStylePack("glow"));
    assert(StylePackVersion() == version + 1);
    assert(GetActiveStylePack()->sheet == &glow_sheet);
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
        .sheet = &vanilla_sheet,
    }));
    assert(GetStylePackCount() == 2);
    assert(GetActiveStylePack()->sheet == &vanilla_sheet);
    assert(GetActiveStylePack()->label[5] == 'U');

    ClearStylePacks();
    assert(GetStylePackCount() == 0);
    assert(GetActiveStylePack() == NULL);

    return 0;
}

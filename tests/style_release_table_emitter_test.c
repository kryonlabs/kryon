#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"

#include <assert.h>
#include <string.h>

bool RegisterCompiledBuiltInStylePacks(void);

typedef struct ResolveCase {
    const char *pack_id;
    StyleFacts facts;
    int state;
} ResolveCase;

static int
style_data_equal(StyleData a, StyleData b)
{
    return a.fields == b.fields &&
           a.background == b.background &&
           a.foreground == b.foreground &&
           a.border == b.border &&
           a.focus == b.focus &&
           a.radius == b.radius &&
           a.border_width == b.border_width &&
           a.opacity == b.opacity &&
           a.padding_x == b.padding_x &&
           a.padding_y == b.padding_y &&
           a.gap == b.gap &&
           a.font_size == b.font_size &&
           a.icon_size == b.icon_size &&
           a.offset_x == b.offset_x &&
           a.offset_y == b.offset_y &&
           a.background_end == b.background_end &&
           a.material == b.material &&
           a.letter_spacing == b.letter_spacing &&
           StringEqual(a.typeface, b.typeface);
}

static void
capture_source_loaded_builtins(const ResolveCase *cases, int count,
                               StyleData *out)
{
    ClearStylePacks();
    assert(RegisterBuiltInStylePacks());
    assert(GetStylePackCount() == 3);
    assert(strcmp(GetActiveStylePackId(), "material") == 0);
    for(int i = 0; i < count; i++) {
        assert(SetActiveStylePack(cases[i].pack_id));
        out[i] = ResolveActiveStyle((StyleData){0}, cases[i].facts,
                                    cases[i].state);
        assert(out[i].fields != 0u);
    }
}

int
main(void)
{
    ResolveCase cases[] = {
        {"material", StyleControlFacts(StyleKindButton(), 0, 0,
             ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
             ButtonStateHover), ButtonStateHover},
        {"material", StyleDefaultFacts(StyleKindCard()), ButtonStateNormal},
        {"classic", StyleControlFacts(StyleKindButton(), 0, 0,
             ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
             ButtonStateHover), ButtonStateHover},
        {"classic", StyleDefaultFacts(StyleKindDropdown()), ButtonStateNormal},
        {"lightfield", StyleDefaultFacts(StyleKindSurface()), ButtonStateNormal},
        {"lightfield", StyleDefaultFacts(StyleKindTextField()), ButtonStateNormal},
    };
    StyleData source_loaded[sizeof(cases) / sizeof(cases[0])];
    int count = (int)(sizeof(cases) / sizeof(cases[0]));

    capture_source_loaded_builtins(cases, count, source_loaded);

    ClearStylePacks();
    KssResetParseInvocationCount();
    assert(RegisterCompiledBuiltInStylePacks());
    assert(KssParseInvocationCount() == 0);
    assert(GetStylePackCount() == 3);
    assert(strcmp(GetActiveStylePackId(), "material") == 0);

    for(int round = 0; round < 4; round++) {
        for(int i = 0; i < count; i++) {
            StyleData compiled;

            assert(SetActiveStylePack(cases[i].pack_id));
            compiled = ResolveActiveStyle((StyleData){0}, cases[i].facts,
                                          cases[i].state);
            assert(style_data_equal(compiled, source_loaded[i]));
        }
    }
    assert(KssParseInvocationCount() == 0);

    ClearStylePacks();
    return 0;
}

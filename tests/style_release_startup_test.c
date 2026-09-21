#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"

#include <assert.h>
#include <string.h>

int
main(void)
{
    StyleFacts accent = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleFacts surface = StyleDefaultFacts(StyleKindSurface());
    StyleData resolved;

    ClearStylePacks();
    KssResetParseInvocationCount();
    assert(RegisterBuiltInStylePacks());
    assert(KssParseInvocationCount() == 0);
    assert(GetStylePackCount() == 3);
    assert(strcmp(GetActiveStylePackId(), "material") == 0);

    resolved = ResolveActiveStyle((StyleData){0}, accent, ButtonStateHover);
    assert(resolved.background == 0xd5bbffffu);
    assert(resolved.foreground == 0x171022ffu);
    assert(resolved.material == MaterialFlat);

    assert(SetActiveStylePack("lightfield"));
    resolved = ResolveActiveStyle((StyleData){0}, surface, ButtonStateNormal);
    assert(resolved.material == MaterialLightfield);
    assert(resolved.background_end == 0x222936eeu);

    assert(EnsureBuiltInStylePacks());
    assert(KssParseInvocationCount() == 0);
    assert(strcmp(GetActiveStylePackId(), "lightfield") == 0);

    assert(ReapplyBuiltInStyleTheme(""));
    assert(KssParseInvocationCount() == 0);
    assert(strcmp(GetActiveStylePackId(), "lightfield") == 0);

    ClearStylePacks();
    return 0;
}

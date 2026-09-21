#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"

#include <assert.h>

bool RegisterCompiledOverlayStylePack(void);

int
main(void)
{
    StyleFacts facts = StyleDefaultFacts(StyleKindButton());
    StyleData resolved;

    ClearStylePacks();
    KssResetParseInvocationCount();
    assert(RegisterCompiledOverlayStylePack());
    assert(KssParseInvocationCount() == 0);
    assert(GetStylePackCount() == 1);
    assert(SetActiveStylePack("matched.demo.glow.dark"));

    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateNormal);
    assert(resolved.background == 0x00ff00ffu);
    assert(resolved.foreground == 0xf0f0f0ffu);
    assert(resolved.border == 0x445566ffu);
    assert(resolved.radius == 0.0f);
    assert(resolved.padding_y == 80.0f);
    assert(resolved.border_width == 0.0f);
    assert(resolved.letter_spacing == 9.0f);
    assert(resolved.material == MaterialFlat);
    assert(StringEqual(resolved.typeface, StringView("semibold", 8)));

    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStatePressed);
    assert(resolved.background == 0x304050ffu);
    assert(KssParseInvocationCount() == 0);

    ClearStylePacks();
    return 0;
}

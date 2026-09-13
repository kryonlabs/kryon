#include "ui_style_sheet.h"

#include <assert.h>
#include <string.h>

int
main(void)
{
    const char *source =
        "@pack app.brand;\n"
        "@layer components;\n"
        "Button[tone=Accent]:hover {\n"
        "  background: #123456;\n"
        "  radius: 5;\n"
        "  typeface: semibold;\n"
        "}\n";
    StyleFacts facts = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneAccent, ButtonEmphasisSoft, ControlSizeMedium,
        ButtonStateHover);
    StyleData resolved;

    ClearStylePacks();
    assert(RegisterStylePackSource(source, "Brand", "App brand"));
    assert(GetStylePackCount() == 1);
    assert(strcmp(GetActiveStylePackId(), "app.brand") == 0);
    assert(strcmp(GetActiveStylePack()->label, "Brand") == 0);
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x123456ffu);
    assert(resolved.radius == 5.0f);
    assert(StringEqual(resolved.typeface, StringView("semibold", 8)));

    assert(RegisterStylePackSource(
        "@pack app.brand; Button { background: #abcdef; }",
        "Brand Updated", ""));
    assert(GetStylePackCount() == 1);
    assert(strcmp(GetActiveStylePack()->label, "Brand Updated") == 0);
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0xabcdefffu);

    assert(!RegisterStylePackSource("@layer components;", "Empty", ""));

    ClearStylePacks();
    return 0;
}

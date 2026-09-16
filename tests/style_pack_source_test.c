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

    const char *base = "@pack base; tokens { color { accent: #112233; } } "
        "Button { background: accent; radius: 9; material: glass; } "
        "Button:hover { border: accent; }";
    StyleColorToken colors[] = {{"accent", 0x44aa88ffu}};
    assert(RegisterStylePackSource(base, "Base", ""));
    assert(RegisterStylePackVariant("green", base, "Green", colors, 1));
    assert(SetActiveStylePack("green"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x44aa88ffu);
    assert(resolved.border == 0x44aa88ffu);
    assert(resolved.radius == 9.0f);
    assert(resolved.material == MaterialGlass);
    assert(SetActiveStylePack("base"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x112233ffu);
    uint64_t version = StylePackVersion();
    assert(!RegisterStylePackVariant("green", base, "Broken", NULL, 1));
    assert(StylePackVersion() == version);
    assert(SetActiveStylePack("green"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x44aa88ffu);

    /* Declared '@variant' blocks become selectable packs under
     * '<pack>.<variant>' with their label; the variant sheet resolves base
     * and variant rules together. */
    const char *variantSource =
        "@pack lf; tokens { color { accent: #112233; } } "
        "@variant glow \"Glow\" { accent: #00ff00; } "
        "Button { background: accent; radius: 6; } "
        "@variant glow \"Glow\" { Button { radius: 12; } }";
    assert(RegisterStylePackSource(variantSource, "Lightfield", ""));
    assert(GetStylePackCount() == 5);
    assert(SetActiveStylePack("lf.glow"));
    assert(strcmp(GetActiveStylePack()->label, "Glow") == 0);
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x00ff00ffu);
    assert(resolved.radius == 12.0f);
    assert(SetActiveStylePack("lf"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x112233ffu);
    assert(resolved.radius == 6.0f);

    ClearStylePacks();
    return 0;
}

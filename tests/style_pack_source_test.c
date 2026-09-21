#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"

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

    /* Theme overlays: SetStyleTheme re-resolves registered packs (and their
     * declared variants) under the shared parse environment. */
    assert(RegisterStylePackSource(
        "@pack themed; tokens { color { accent: #111111; } } "
        "@theme dark { accent: #222222; } "
        "Button { background: accent; }",
        "Themed", ""));
    assert(SetActiveStylePack("themed"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x111111ffu);
    assert(SetStyleTheme("dark"));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x222222ffu);
    /* The active pack survives a theme switch. */
    assert(strcmp(GetActiveStylePackId(), "themed") == 0);
    assert(SetStyleTheme(""));
    resolved = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    assert(resolved.background == 0x111111ffu);

    ClearStylePacks();

    const char *release_source =
        "@pack release.typed; tokens { color { accent: #446688; } } "
        "Button { background: accent; radius: 7; opacity: 0.5; }";
    StyleRule release_rules[8] = {0};
    KssParseResult release_parse = {0};
    char diagnostic[256];
    StyleSheet release_sheet;
    StyleData typed;
    StyleData reparsed;

    KssResetParseInvocationCount();
    assert(kss_parse_string(release_source, release_rules, 8, &release_parse,
                            diagnostic, sizeof(diagnostic)));
    assert(KssParseInvocationCount() > 0);
    release_sheet.rules = release_rules;
    release_sheet.rule_count = release_parse.rule_count;
    reparsed = ResolveStyle(&release_sheet, (StyleData){0}, facts,
                            ButtonStateHover);

    KssResetParseInvocationCount();
    assert(RegisterStylePack((StylePack){
        .id = "release.typed",
        .label = "Release Typed",
        .description = "Precompiled release table",
        .sheet = &release_sheet,
    }));
    assert(SetActiveStylePack("release.typed"));
    typed = ResolveActiveStyle((StyleData){0}, facts, ButtonStateHover);
    typed = ResolveActiveStyle(typed, facts, ButtonStateHover);
    assert(KssParseInvocationCount() == 0);
    assert(typed.background == reparsed.background);
    assert(typed.radius == reparsed.radius);
    assert(typed.opacity == reparsed.opacity);

    ClearStylePacks();
    return 0;
}

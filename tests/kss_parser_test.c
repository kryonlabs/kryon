#include "ui_style_sheet.h"
#include "ui/kss_parser.h"

#include <assert.h>
#include <string.h>

int
main(void)
{
    const char *source =
        "@pack glow;\n"
        "@layer components;\n"
        "Button {\n"
        "  background: #111111;\n"
        "  foreground: #eeeeeeff;\n"
        "  radius: 6;\n"
        "  padding-x: 12;\n"
        "}\n"
        "Button[tone=Accent][emphasis=Filled]:hover {\n"
        "  background: #2f6bff;\n"
        "  border-width: 2;\n"
        "}\n";
    StyleRule rules[8] = {0};
    KssParseResult result = {0};
    StyleSheet sheet;
    StyleFacts accent = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleFacts danger = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneDanger, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleData base = {0};
    StyleData resolved;
    char diagnostic[128];

    assert(kss_parse_string(source, rules, 8, &result, diagnostic,
                            sizeof(diagnostic)));
    assert(strcmp(result.pack_id, "glow") == 0);
    assert(result.rule_count == 2);
    assert(rules[0].selector.kind == StyleKindButton());
    assert(rules[0].layer == 1);
    assert(rules[0].style.background == 0x111111ffu);
    assert(rules[0].style.foreground == 0xeeeeeeffu);
    assert(rules[0].style.radius == 6.0f);
    assert(rules[0].style.padding_x == 12.0f);
    assert(rules[1].selector.tone == ButtonToneAccent);
    assert(rules[1].selector.emphasis == ButtonEmphasisFilled);
    assert(rules[1].state == ButtonStateHover);
    assert(rules[1].style.background == 0x2f6bffffu);

    sheet.rules = rules;
    sheet.rule_count = result.rule_count;
    resolved = ResolveStyle(&sheet, base, accent, ButtonStateHover);
    assert(resolved.background == 0x2f6bffffu);
    assert(resolved.foreground == 0xeeeeeeffu);
    assert(resolved.border_width == 2.0f);
    assert(resolved.radius == 6.0f);

    resolved = ResolveStyle(&sheet, base, danger, ButtonStateHover);
    assert(resolved.background == 0x111111ffu);
    assert(resolved.border_width == 0.0f);

    assert(!kss_parse_string("Button { mystery: 1; }", rules, 8, &result,
                             diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown property") != NULL);

    return 0;
}

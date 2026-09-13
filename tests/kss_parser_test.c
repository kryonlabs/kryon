#include "ui_style_sheet.h"
#include "ui/kss_parser.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *
read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    long size;
    char *data;

    assert(f != NULL);
    assert(fseek(f, 0, SEEK_END) == 0);
    size = ftell(f);
    assert(size >= 0);
    assert(fseek(f, 0, SEEK_SET) == 0);
    data = (char *)malloc((size_t)size + 1);
    assert(data != NULL);
    assert(fread(data, 1, (size_t)size, f) == (size_t)size);
    data[size] = '\0';
    fclose(f);
    return data;
}

static void
assert_pack_parses(const char *path, const char *id)
{
    StyleRule rules[256] = {0};
    KssParseResult result = {0};
    char diagnostic[256];
    char *source = read_file(path);

    if(!kss_parse_string(source, rules, 256, &result, diagnostic,
                         sizeof(diagnostic))) {
        fprintf(stderr, "%s: %s\n", path, diagnostic);
        assert(0);
    }
    assert(strcmp(result.pack_id, id) == 0);
    assert(result.rule_count > 0);
    free(source);
}

int
main(void)
{
    const char *source =
        "@pack glow;\n"
        "tokens {\n"
        "  color { face: #111111; ink: #eeeeeeff; accent: #2f6bff; }\n"
        "  length { radius.md: 6; space.3: 12; line: 2; }\n"
        "  duration { fast: 80ms; normal: 0.14s; }\n"
        "}\n"
        "@layer components;\n"
        "Button {\n"
        "  background: face;\n"
        "  foreground: ink;\n"
        "  radius: radius.md;\n"
        "  padding-x: space.3;\n"
        "  typeface: semibold;\n"
        "}\n"
        "Button[tone=Accent][emphasis=Filled]:hover {\n"
        "  background: accent;\n"
        "  border-width: line;\n"
        "}\n"
        "Button.primary {\n"
        "  padding-y: fast;\n"
        "}\n"
        "Button[class=primary]:pressed {\n"
        "  focus: accent;\n"
        "  opacity: normal;\n"
        "}\n"
        "Segment:selected {\n"
        "  foreground: accent;\n"
        "}\n"
        "MenuItem:selected {\n"
        "  background: accent;\n"
        "}\n"
        "ListBoxItem:selected {\n"
        "  foreground: accent;\n"
        "}\n"
        "TreeViewItem:selected {\n"
        "  background: accent;\n"
        "}\n"
        "ListBoxMultiItem:selected {\n"
        "  foreground: accent;\n"
        "}\n"
        "DragDropTarget:hover {\n"
        "  border: accent;\n"
        "}\n"
        "SpinboxValue {\n"
        "  background: face;\n"
        "}\n"
        "ColorPickerSwatch {\n"
        "  border: accent;\n"
        "}\n"
        "SliderThumb {\n"
        "  background: accent;\n"
        "}\n"
        "Scroll {\n"
        "  background: face;\n"
        "}\n"
        "ScrollThumb:pressed {\n"
        "  background: accent;\n"
        "}\n"
        "ToggleThumb {\n"
        "  background: accent;\n"
        "}\n"
        "Menu[role=Popup] {\n"
        "  background: accent;\n"
        "}\n"
        "Progress[role=Fill] {\n"
        "  background: accent;\n"
        "}\n"
        "Separator[role=Bullet] {\n"
        "  foreground: accent;\n"
        "}\n"
        "Checkbox[role=Mark] {\n"
        "  background: accent;\n"
        "}\n"
        "Radio[role=Ring] {\n"
        "  border: accent;\n"
        "}\n"
        "Guide[role=Anchor] {\n"
        "  border: accent;\n"
        "}\n"
        "Image[role=Label] {\n"
        "  foreground: accent;\n"
        "}\n"
        "Popup[role=Panel] {\n"
        "  border: accent;\n"
        "}\n"
        "Canvas {\n"
        "  background: accent;\n"
        "}\n"
        "DragValue {\n"
        "  border: accent;\n"
        "}\n"
        "Heading {\n"
        "  font-size: 24;\n"
        "}\n"
        "ParagraphText {\n"
        "  foreground: accent;\n"
        "}\n"
        "Focus[role=Box]:focus {\n"
        "  border: accent;\n"
        "}\n";
    StyleRule rules[29] = {0};
    KssParseResult result = {0};
    StyleSheet sheet;
    StyleFacts accent = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneAccent, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleFacts primary = StyleControlFacts(StyleKindButton(), 0,
        StyleClassId("primary"), ButtonToneNeutral, ButtonEmphasisSoft,
        ControlSizeMedium, ButtonStatePressed);
    StyleFacts danger = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneDanger, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleData base = {0};
    StyleData resolved;
    char diagnostic[128];

    assert(kss_parse_string(source, rules, 29, &result, diagnostic,
                            sizeof(diagnostic)));
    assert(strcmp(result.pack_id, "glow") == 0);
    assert(result.rule_count == 29);
    assert(rules[0].selector.kind == StyleKindButton());
    assert(rules[0].layer == 1);
    assert(rules[0].style.background == 0x111111ffu);
    assert(rules[0].style.foreground == 0xeeeeeeffu);
    assert(rules[0].style.radius == 6.0f);
    assert(rules[0].style.padding_x == 12.0f);
    assert(StringEqual(rules[0].style.typeface, StringView("semibold", 8)));
    assert(rules[1].selector.tone == ButtonToneAccent);
    assert(rules[1].selector.emphasis == ButtonEmphasisFilled);
    assert(rules[1].state == ButtonStateHover);
    assert(rules[1].style.background == 0x2f6bffffu);
    assert(rules[2].selector.kind == StyleKindButton());
    assert(rules[2].selector.class_name == StyleClassId("primary"));
    assert(rules[2].style.padding_y == 80.0f);
    assert(rules[3].selector.kind == StyleKindButton());
    assert(rules[3].selector.class_name == StyleClassId("primary"));
    assert(rules[3].state == ButtonStatePressed);
    assert(rules[3].style.focus == 0x2f6bffffu);
    assert(rules[3].style.opacity == 140.0f);
    assert(rules[4].selector.kind == StyleKindSegment());
    assert(rules[4].state == ButtonStateSelected);
    assert(rules[4].style.foreground == 0x2f6bffffu);
    assert(rules[28].selector.kind == StyleKindFocus());
    assert(rules[28].selector.role == 9);
    assert(rules[28].state == ButtonStateFocus);
    assert(rules[28].style.border == 0x2f6bffffu);

    sheet.rules = rules;
    sheet.rule_count = result.rule_count;
    resolved = ResolveStyle(&sheet, base, accent, ButtonStateHover);
    assert(resolved.background == 0x2f6bffffu);
    assert(resolved.foreground == 0xeeeeeeffu);
    assert(resolved.border_width == 2.0f);
    assert(resolved.radius == 6.0f);

    resolved = ResolveStyle(&sheet, base, primary, ButtonStatePressed);
    assert(resolved.background == 0x111111ffu);
    assert(resolved.padding_y == 80.0f);
    assert(resolved.focus == 0x2f6bffffu);
    assert(resolved.opacity == 140.0f);

    resolved = ResolveStyle(&sheet, base, danger, ButtonStateHover);
    assert(resolved.background == 0x111111ffu);
    assert(resolved.border_width == 0.0f);

    assert(!kss_parse_string("Button { mystery: 1; }", rules, 12, &result,
                             diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown property") != NULL);

    const char *legacy_aliases[] = {
        "background-color", "color", "border-color", "focus-color",
        "background_end", "border_width", "padding_x", "padding_y",
        "font_size", "icon_size", "offset_x", "offset_y", "font-family",
    };
    for(size_t i = 0; i < sizeof(legacy_aliases) / sizeof(legacy_aliases[0]); i++) {
        char legacy_source[128];
        snprintf(legacy_source, sizeof(legacy_source), "Button { %s: #111111; }",
                 legacy_aliases[i]);
        assert(!kss_parse_string(legacy_source, rules, 12, &result,
                                 diagnostic, sizeof(diagnostic)));
        assert(strstr(diagnostic, "unknown property") != NULL);
    }
    assert(!kss_parse_string("@theme dark; Button { background: #111111; }",
                             rules, 12, &result, diagnostic,
                             sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown directive") != NULL);
    assert(!kss_parse_string("@layer legacy; Button { background: #111111; }",
                             rules, 12, &result, diagnostic,
                             sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown layer") != NULL);
    assert(!kss_parse_string(
               "tokens { colors { face: #111111; } } Button { background: #111111; }",
               rules, 12, &result, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown token group") != NULL);

    assert_pack_parses("styles/kryon/material.kss", "material");
    assert_pack_parses("styles/kryon/tk.kss", "tk");
    assert_pack_parses("styles/kryon/vanilla.kss", "vanilla");
    assert_pack_parses("styles/kryon/glow.kss", "glow");
    assert_pack_parses("styles/kryon/lightfield.kss", "lightfield");

    return 0;
}

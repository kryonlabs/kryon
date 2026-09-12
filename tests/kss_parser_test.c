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
        "}\n"
        "@layer components;\n"
        "Button {\n"
        "  background: face;\n"
        "  foreground: ink;\n"
        "  radius: radius.md;\n"
        "  padding-x: space.3;\n"
        "}\n"
        "Button[tone=Accent][emphasis=Filled]:hover {\n"
        "  background: accent;\n"
        "  border-width: line;\n"
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
        "Focus[role=Box]:focus {\n"
        "  border: accent;\n"
        "}\n";
    StyleRule rules[23] = {0};
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

    assert(kss_parse_string(source, rules, 23, &result, diagnostic,
                            sizeof(diagnostic)));
    assert(strcmp(result.pack_id, "glow") == 0);
    assert(result.rule_count == 23);
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
    assert(rules[2].selector.kind == StyleKindSegment());
    assert(rules[2].state == ButtonStateSelected);
    assert(rules[2].style.foreground == 0x2f6bffffu);
    assert(rules[3].selector.kind == StyleKindMenuItem());
    assert(rules[3].state == ButtonStateSelected);
    assert(rules[3].style.background == 0x2f6bffffu);
    assert(rules[4].selector.kind == StyleKindListBoxItem());
    assert(rules[4].state == ButtonStateSelected);
    assert(rules[4].style.foreground == 0x2f6bffffu);
    assert(rules[5].selector.kind == StyleKindTreeViewItem());
    assert(rules[5].state == ButtonStateSelected);
    assert(rules[5].style.background == 0x2f6bffffu);
    assert(rules[6].selector.kind == StyleKindListBoxMultiItem());
    assert(rules[6].state == ButtonStateSelected);
    assert(rules[6].style.foreground == 0x2f6bffffu);
    assert(rules[7].selector.kind == StyleKindDragDropTarget());
    assert(rules[7].state == ButtonStateHover);
    assert(rules[7].style.border == 0x2f6bffffu);
    assert(rules[8].selector.kind == StyleKindSpinboxValue());
    assert(rules[8].style.background == 0x111111ffu);
    assert(rules[9].selector.kind == StyleKindColorPickerSwatch());
    assert(rules[9].style.border == 0x2f6bffffu);
    assert(rules[10].selector.kind == StyleKindSliderThumb());
    assert(rules[10].style.background == 0x2f6bffffu);
    assert(rules[11].selector.kind == StyleKindScroll());
    assert(rules[11].style.background == 0x111111ffu);
    assert(rules[12].selector.kind == StyleKindScrollThumb());
    assert(rules[12].state == ButtonStatePressed);
    assert(rules[12].style.background == 0x2f6bffffu);
    assert(rules[13].selector.kind == StyleKindToggleThumb());
    assert(rules[13].style.background == 0x2f6bffffu);
    assert(rules[14].selector.kind == StyleKindMenu());
    assert(rules[14].selector.role == 2);
    assert(rules[14].style.background == 0x2f6bffffu);
    assert(rules[15].selector.kind == StyleKindProgress());
    assert(rules[15].selector.role == 5);
    assert(rules[15].style.background == 0x2f6bffffu);
    assert(rules[16].selector.kind == StyleKindSeparator());
    assert(rules[16].selector.role == 8);
    assert(rules[16].style.foreground == 0x2f6bffffu);
    assert(rules[17].selector.kind == StyleKindCheckbox());
    assert(rules[17].selector.role == 10);
    assert(rules[17].style.background == 0x2f6bffffu);
    assert(rules[18].selector.kind == StyleKindRadio());
    assert(rules[18].selector.role == 11);
    assert(rules[18].style.border == 0x2f6bffffu);
    assert(rules[19].selector.kind == StyleKindGuide());
    assert(rules[19].selector.role == 24);
    assert(rules[19].style.border == 0x2f6bffffu);
    assert(rules[20].selector.kind == StyleKindImage());
    assert(rules[20].selector.role == 6);
    assert(rules[20].style.foreground == 0x2f6bffffu);
    assert(rules[21].selector.kind == StyleKindPopup());
    assert(rules[21].selector.role == 2);
    assert(rules[21].style.border == 0x2f6bffffu);
    assert(rules[22].selector.kind == StyleKindFocus());
    assert(rules[22].selector.role == 9);
    assert(rules[22].state == ButtonStateFocus);
    assert(rules[22].style.border == 0x2f6bffffu);

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

    assert(!kss_parse_string("Button { mystery: 1; }", rules, 12, &result,
                             diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown property") != NULL);

    assert_pack_parses("styles/kryon/material.kss", "kryon.material");
    assert_pack_parses("styles/kryon/tk.kss", "kryon.tk");
    assert_pack_parses("styles/kryon/vanilla.kss", "kryon.vanilla");
    assert_pack_parses("styles/kryon/glow.kss", "kryon.glow");
    assert_pack_parses("styles/kryon/lightfield.kss", "kryon.lightfield");

    return 0;
}

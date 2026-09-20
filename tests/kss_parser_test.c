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


static void
assert_diagnostic_contains(const char *source, const char *message,
                           const char *location)
{
    StyleRule rules[12];
    KssParseResult result = {0};
    char diagnostic[256];

    assert(!kss_parse_string(source, rules, 12, &result, diagnostic,
                             sizeof(diagnostic)));
    assert(strstr(diagnostic, message) != NULL);
    assert(strstr(diagnostic, location) != NULL);
}


static void
assert_rule_source(const KssRuleSource *source, const char *file, int file_index,
                   int line, int column)
{
    assert(strcmp(source->file, file) == 0);
    assert(source->file_index == file_index);
    assert(source->line == line);
    assert(source->column == column);
    assert(source->selector_length > 0);
    assert(source->body_length > 0);
    assert(source->end > source->body_start);
}

static void
test_trace_sources(void)
{
    const char *source =
        "@pack inspect;\n"
        "@import <base>;\n"
        "@layer components;\n"
        "Button { background: #222222; }\n"
        "Button.primary { background: #333333; }\n"
        "Button { foreground: #eeeeee; }\n";
    StyleRule rules[8] = {0};
    KssRuleSource sources[8] = {0};
    KssParseResult result = {0};
    char diagnostic[256];

    ClearStyleModules();
    assert(RegisterStyleModule("base",
        "Surface { padding-x: 7; }\n"
        "Button { background: #111111; }\n"));
    assert(kss_parse_trace_string(source, rules, sources, 8, &result,
                                  diagnostic, sizeof(diagnostic)));
    assert(strcmp(result.pack_id, "inspect") == 0);
    assert(result.rule_count == 5);

    assert(rules[0].selector.kind == StyleKindSurface());
    assert(rules[0].style.padding_x == 7.0f);
    assert_rule_source(&sources[0], "base", 1, 1, 1);
    assert(rules[1].selector.kind == StyleKindButton());
    assert(rules[1].style.background == 0x111111ffu);
    assert_rule_source(&sources[1], "base", 1, 2, 1);
    assert(rules[3].selector.class_name == StyleClassId("primary"));
    assert(rules[3].style.background == 0x333333ffu);
    assert_rule_source(&sources[3], "", 0, 5, 1);
    assert(rules[4].style.foreground == 0xeeeeeeffu);
    assert_rule_source(&sources[4], "", 0, 6, 1);
    ClearStyleModules();
}

static void
test_diagnostics(void)
{
    assert_diagnostic_contains(
        "Button {\n"
        "  background: #111111;\n"
        "  mystery: 1;\n"
        "}\n",
        "unknown property", "3:11");
    assert_diagnostic_contains(
        "tokens {\n"
        "  font { heading: system; }\n"
        "}\n",
        "unknown token group", "2:7");
    assert_diagnostic_contains(
        "tokens {\n"
        "  radius { control: 4; }\n"
        "}\n",
        "unknown token group", "2:9");
    assert_diagnostic_contains(
        "tokens {\n"
        "  easing { standard: linear; }\n"
        "}\n",
        "unknown token group", "2:9");
    assert_diagnostic_contains(
        "tokens {\n"
        "  length { control { pad: 4; } }\n"
        "}\n",
        "expected ':' after token name", "2:20");
    assert_diagnostic_contains(
        "Button {\n"
        "  background: #123;\n"
        "}\n",
        "expected hex color", "2:19");
    assert_diagnostic_contains(
        "Button { background: color-mix(red, blue); }\n",
        "expected hex color", "1:22");
    assert_diagnostic_contains(
        "Button { padding-x: 12px; }\n",
        "expected ';'", "1:23");
    assert_diagnostic_contains(
        "Button, Surface { background: #111111; }\n",
        "expected '{'", "1:7");
    assert_diagnostic_contains(
        "@layer base { Button { background: #111111; } }\n",
        "expected ';'", "1:13");
    assert_diagnostic_contains(
        "Button { transition: opacity 100ms; }\n",
        "unknown property", "1:21");
    assert_diagnostic_contains(
        "Button { font_family: system; }\n",
        "unknown property", "1:22");
    assert_diagnostic_contains(
        "@theme dark;\n"
        "Button { background: #111111; }\n",
        "expected '{' after @theme name", "1:12");
    assert_diagnostic_contains(
        "@import \"missing\";\n"
        "Button { background: #111111; }\n",
        "missing import: missing", "1:19");
}

static void
test_color_override_after_sixteen(void)
{
    const char *source =
        "@pack override_capacity;\n"
        "tokens { color { row: #171c25; } }\n"
        "Dropdown { background: row; }\n";
    StyleColorToken colors[20] = {0};
    char names[19][12];
    StyleRule rules[4] = {0};
    KssParseResult result = {0};
    char diagnostic[256];

    for(int i = 0; i < 19; i++) {
        snprintf(names[i], sizeof(names[i]), "unused%d", i);
        colors[i].name = names[i];
        colors[i].color = 0x111111ffu;
    }
    colors[19].name = "row";
    colors[19].color = 0xe2eefcffu;

    assert(kss_parse_variant(source, colors, 20, rules, 4, &result,
                             diagnostic, sizeof(diagnostic)));
    assert(result.rule_count == 1);
    assert(rules[0].style.background == colors[19].color);
}

int
main(void)
{
    test_diagnostics();
    test_trace_sources();
    test_color_override_after_sixteen();
    const char *source =
        "@pack sample;\n"
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
        "  letter-spacing: 2;\n"
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
        "* {\n"
        "  gap: space.3;\n"
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
        "Toolbar[role=BottomBar] {\n"
        "  padding-x: 24;\n"
        "}\n"
        "Toolbar[role=BottomAction] {\n"
        "  icon-size: 24;\n"
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
        "Page {\n"
        "  padding-x: 8;\n"
        "}\n"
        "Section {\n"
        "  gap: 6;\n"
        "}\n"
        "Focus[role=Box]:focus {\n"
        "  border: accent;\n"
        "}\n";
    StyleRule rules[40] = {0};
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

    assert(kss_parse_string(source, rules, 40, &result, diagnostic,
                            sizeof(diagnostic)));
    assert(strcmp(result.pack_id, "sample") == 0);
    assert(result.rule_count == 34);
    assert(rules[0].selector.kind == StyleKindButton());
    assert(rules[0].layer == 1);
    assert(rules[0].style.background == 0x111111ffu);
    assert(rules[0].style.foreground == 0xeeeeeeffu);
    assert(rules[0].style.radius == 6.0f);
    assert(rules[0].style.padding_x == 12.0f);
    assert(StringEqual(rules[0].style.typeface, StringView("semibold", 8)));
    assert(rules[0].style.letter_spacing == 2.0f);
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
    assert(rules[4].selector.kind == StyleKindAny());
    assert(rules[4].style.gap == 12.0f);
    assert(rules[5].selector.kind == StyleKindSegment());
    assert(rules[5].state == ButtonStateSelected);
    assert(rules[5].style.foreground == 0x2f6bffffu);
    int saw_bottom_bar = 0;
    int saw_bottom_action = 0;
    for(int i = 0; i < result.rule_count; i++) {
        if(rules[i].selector.kind == StyleKindToolbar() &&
           rules[i].selector.role == 28 &&
           rules[i].style.padding_x == 24.0f) {
            saw_bottom_bar = 1;
        }
        if(rules[i].selector.kind == StyleKindToolbar() &&
           rules[i].selector.role == 29 &&
           rules[i].style.icon_size == 24.0f) {
            saw_bottom_action = 1;
        }
    }
    assert(saw_bottom_bar);
    assert(saw_bottom_action);
    assert(rules[31].selector.kind == StyleKindPage());
    assert(rules[31].style.padding_x == 8.0f);
    assert(rules[32].selector.kind == StyleKindSection());
    assert(rules[32].style.gap == 6.0f);
    assert(rules[33].selector.kind == StyleKindFocus());
    assert(rules[33].selector.role == 9);
    assert(rules[33].state == ButtonStateFocus);
    assert(rules[33].style.border == 0x2f6bffffu);

    sheet.rules = rules;
    sheet.rule_count = result.rule_count;
    resolved = ResolveStyle(&sheet, base, accent, ButtonStateHover);
    assert(resolved.background == 0x2f6bffffu);
    assert(resolved.foreground == 0xeeeeeeffu);
    assert(resolved.border_width == 2.0f);
    assert(resolved.radius == 6.0f);
    assert(resolved.letter_spacing == 2.0f);

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
    assert(strstr(diagnostic, "expected '{'") != NULL);
    assert(!kss_parse_string("@layer legacy; Button { background: #111111; }",
                             rules, 12, &result, diagnostic,
                             sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown layer") != NULL);
    assert(!kss_parse_string(
               "tokens { colors { face: #111111; } } Button { background: #111111; }",
               rules, 12, &result, diagnostic, sizeof(diagnostic)));
    assert(strstr(diagnostic, "unknown token group") != NULL);

    assert_pack_parses("styles/kryon/material.kss", "material");
    assert_pack_parses("styles/kryon/classic.kss", "classic");
    assert_pack_parses("styles/kryon/lightfield.kss", "lightfield");

    return 0;
}

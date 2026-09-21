#include "ui_style_pack_props.generated.h"
#include "ui/kss_parser.h"
#include "embedded_assets.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

typedef struct BuiltinPackInput {
    const char *id;
    const char *label;
    const char *description;
    const char *path;
} BuiltinPackInput;

typedef struct ResolveCase {
    const char *pack_id;
    StyleFacts facts;
    int state;
} ResolveCase;

typedef struct StyleSnapshot {
    StyleData values[16];
    int count;
} StyleSnapshot;

static const BuiltinPackInput builtin_inputs[] = {
    {
        "material",
        "Material",
        "Clean Material-like controls with flat paint",
        "styles/kryon/material.kss",
    },
    {
        "classic",
        "Classic",
        "Dense toolkit controls for desktop utilities",
        "styles/kryon/classic.kss",
    },
    {
        "lightfield",
        "Lightfield",
        "Premium translucent controls with glow-capable treatment",
        "styles/kryon/lightfield.kss",
    },
};

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

static int
style_selector_equal(StyleSelector a, StyleSelector b)
{
    return a.kind == b.kind &&
           a.name == b.name &&
           a.class_name == b.class_name &&
           a.role == b.role &&
           a.tone == b.tone &&
           a.emphasis == b.emphasis &&
           a.size == b.size &&
           a.state == b.state &&
           a.validation == b.validation &&
           a.orientation == b.orientation &&
           a.placement == b.placement;
}

static int
style_rule_equal(StyleRule a, StyleRule b)
{
    return style_selector_equal(a.selector, b.selector) &&
           a.state == b.state &&
           a.layer == b.layer &&
           a.order == b.order &&
           style_data_equal(a.style, b.style);
}

static void
assert_rule_tables_equal(const StyleRule *a, const StyleRule *b, int count)
{
    for(int i = 0; i < count; i++)
        assert(style_rule_equal(a[i], b[i]));
}

static void
parse_release_source(const char *source, const char *variant,
                     const char *theme, StyleRule *rules, int capacity,
                     KssParseResult *result)
{
    char diagnostic[256];

    memset(rules, 0, (size_t)capacity * sizeof(rules[0]));
    memset(result, 0, sizeof(*result));
    assert(kss_parse_with_environment(source, variant, theme, rules, capacity,
                                      result, diagnostic,
                                      sizeof(diagnostic)));
    assert(result->pack_id[0] != '\0');
    assert(result->rule_count > 0);
}

static void
record_dynamic_builtin_snapshot(const ResolveCase *cases, int case_count,
                                StyleSnapshot *snapshot)
{
    ClearStylePacks();
    assert(RegisterBuiltInStylePacks());
    snapshot->count = case_count;
    for(int i = 0; i < case_count; i++) {
        assert(SetActiveStylePack(cases[i].pack_id));
        snapshot->values[i] = ResolveActiveStyle((StyleData){0},
                                                 cases[i].facts,
                                                 cases[i].state);
        assert(snapshot->values[i].fields != 0u);
    }
}

static void
register_compiled_builtin_tables(StyleRule rules[][384],
                                 StyleSheet sheets[], char *sources[])
{
    for(size_t i = 0; i < sizeof(builtin_inputs) / sizeof(builtin_inputs[0]); i++) {
        KssParseResult result;

        sources[i] = LoadEmbeddedAssetText(builtin_inputs[i].path);
        assert(sources[i] != NULL);
        parse_release_source(sources[i], NULL, NULL, rules[i], 384, &result);
        assert(strcmp(result.pack_id, builtin_inputs[i].id) == 0);
        sheets[i].rules = rules[i];
        sheets[i].rule_count = result.rule_count;
        assert(RegisterStylePack((StylePack){
            .id = builtin_inputs[i].id,
            .label = builtin_inputs[i].label,
            .description = builtin_inputs[i].description,
            .sheet = &sheets[i],
        }));
    }
}

static void
test_builtin_release_tables_match_runtime_sources(void)
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
    StyleSnapshot dynamic = {0};
    StyleRule compiled_rules[3][384];
    StyleSheet compiled_sheets[3];
    char *sources[3] = {0};

    record_dynamic_builtin_snapshot(cases,
        (int)(sizeof(cases) / sizeof(cases[0])), &dynamic);

    ClearStylePacks();
    KssResetParseInvocationCount();
    register_compiled_builtin_tables(compiled_rules, compiled_sheets, sources);
    assert(KssParseInvocationCount() == 3);

    KssResetParseInvocationCount();
    for(int round = 0; round < 4; round++) {
        for(int i = 0; i < dynamic.count; i++) {
            StyleData resolved;

            assert(SetActiveStylePack(cases[i].pack_id));
            resolved = ResolveActiveStyle((StyleData){0}, cases[i].facts,
                                          cases[i].state);
            assert(style_data_equal(resolved, dynamic.values[i]));
        }
    }
    assert(KssParseInvocationCount() == 0);

    for(size_t i = 0; i < sizeof(sources) / sizeof(sources[0]); i++)
        free(sources[i]);
    ClearStylePacks();
}

static void
test_import_overlay_table_reproducibility(void)
{
    const char *module =
        "@pack release.tokens;\n"
        "tokens { color { accent: #112233; ink: #eeeeee; } length { r: 3; } }\n"
        "Button { background: accent; foreground: ink; radius: r; }\n";
    const char *source =
        "@pack release.overlay;\n"
        "@import <release.tokens>;\n"
        "@theme dark { accent: #445566; }\n"
        "@variant glow \"Glow\" { accent: #00ff00; r: 11; }\n"
        "Button:hover { border: accent; radius: r; typeface: semibold; }\n";
    StyleRule dynamic_rules[32];
    StyleRule compiled_rules[32];
    KssParseResult dynamic_result;
    KssParseResult compiled_result;
    StyleSheet dynamic_sheet;
    StyleSheet compiled_sheet;
    StyleFacts hover = StyleControlFacts(StyleKindButton(), 0, 0,
        ButtonToneNeutral, ButtonEmphasisFilled, ControlSizeMedium,
        ButtonStateHover);
    StyleData dynamic_resolved;
    StyleData compiled_resolved;

    ClearStyleModules();
    assert(RegisterStyleModule("release.tokens", module));
    parse_release_source(source, "glow", "dark", dynamic_rules, 32,
                         &dynamic_result);
    parse_release_source(source, "glow", "dark", compiled_rules, 32,
                         &compiled_result);
    assert(dynamic_result.rule_count == compiled_result.rule_count);
    assert_rule_tables_equal(dynamic_rules, compiled_rules,
                             dynamic_result.rule_count);

    dynamic_sheet.rules = dynamic_rules;
    dynamic_sheet.rule_count = dynamic_result.rule_count;
    dynamic_resolved = ResolveStyle(&dynamic_sheet, (StyleData){0}, hover,
                                    ButtonStateHover);
    assert(dynamic_resolved.background == 0x112233ffu);
    assert(dynamic_resolved.border == 0x00ff00ffu);
    assert(dynamic_resolved.radius == 11.0f);
    assert(StringEqual(dynamic_resolved.typeface, StringView("semibold", 8)));

    ClearStylePacks();
    compiled_sheet.rules = compiled_rules;
    compiled_sheet.rule_count = compiled_result.rule_count;
    KssResetParseInvocationCount();
    assert(RegisterStylePack((StylePack){
        .id = "release.overlay.glow.dark",
        .label = "Release Overlay Glow Dark",
        .description = "Compiled import, theme, and variant release table",
        .sheet = &compiled_sheet,
    }));
    assert(SetActiveStylePack("release.overlay.glow.dark"));
    for(int i = 0; i < 8; i++) {
        compiled_resolved = ResolveActiveStyle((StyleData){0}, hover,
                                               ButtonStateHover);
        assert(style_data_equal(compiled_resolved, dynamic_resolved));
    }
    assert(KssParseInvocationCount() == 0);

    ClearStylePacks();
    ClearStyleModules();
}

int
main(void)
{
    test_builtin_release_tables_match_runtime_sources();
    test_import_overlay_table_reproducibility();
    return 0;
}

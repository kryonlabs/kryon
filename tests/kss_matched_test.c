/* Matched-fixture and invalid-input coverage for the shared KSS module
 * (runtime/kss_parser.kry). Drives the generated parser directly so rule
 * provenance is asserted alongside values. */
#include "ui_style_sheet.h"
#include "ui/kss_parser.h"
#include "runtime/kss_parser.h"

#include <assert.h>
#include <stddef.h>
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
    assert(size > 0);
    assert(fseek(f, 0, SEEK_SET) == 0);
    data = (char *)malloc((size_t)size + 1);
    assert(data != NULL);
    assert(fread(data, 1, (size_t)size, f) == (size_t)size);
    data[size] = '\0';
    fclose(f);
    return data;
}

typedef struct Collected {
    StyleRule rules[64];
    KssOrigin origins[64];
    KssRuleSpan spans[64];
    int count;
} Collected;

static bool
collect(const char *source, const char *path, const char *module_source,
        const char *module_path, const char *variant, Collected *out)
{
    KssEnvironment env = KssDefaultEnvironment();
    KssParser p;

    memset(out, 0, sizeof(*out));
    env = KssEnvironmentWithNames(env, StringView("dark", 4),
        StringView("high", 4), StringView("comfortable", 11),
        StringView("mouse", 5), StringView("desktop", 7),
        StringView(variant, variant != NULL ? strlen(variant) : 0));
    p = KssBegin(StringView(source, strlen(source)),
                 StringView(path, strlen(path)), env);
    if(variant != NULL)
        p = KssSetVariant(p, kss_parser_KssMakeName(StringView(variant, strlen(variant))));
    for(;;) {
        if(p.status == KssStatusRule) {
            assert(out->count < 64);
            out->rules[out->count] = p.rule;
            out->origins[out->count] = p.origin;
            out->spans[out->count] = p.rule_span;
            out->count++;
            p.status = KssStatusContinue;
            continue;
        }
        if(p.status != KssStatusContinue)
            break;
        p = KssStep(p);
        if(p.status == KssStatusNeedImport) {
            char name[96];
            int length = p.pending_import.length;
            if(length > 95)
                length = 95;
            memcpy(name, p.pending_import.bytes, (size_t)length);
            name[length] = '\0';
            if(strcmp(name, "matched-module") == 0)
                p = KssProvideImport(p,
                                     StringView(module_source,
                                                strlen(module_source)),
                                     StringView(module_path,
                                                strlen(module_path)));
            else
                p = KssFailImport(p);
        }
    }
    return p.status == KssStatusDone;
}

static void
test_matched_fixture(void)
{
    char *source = read_file("tests/fixtures/kss/matched.kss");
    char *module = read_file("tests/fixtures/kss/matched_module.kss");
    Collected collected;
    Collected glow;
    StyleRule rules[64];
    KssParseResult result;
    char diagnostic[256];
    const StyleRule *surface;
    const StyleRule *button;
    const StyleRule *pressed;
    const StyleRule *quiet;
    const StyleRule *contrast;

    assert(collect(source, "matched.kss", module, "matched_module.kss", NULL,
                   &collected));
    /* Import order first, then document order; density(compact) excluded. */
    assert(collected.count == 5);
    surface = &collected.rules[0];
    assert(surface->selector.kind == StyleKindSurface());
    assert(surface->style.padding_x == 7.0f);
    assert(collected.spans[0].file == 1);

    contrast = &collected.rules[1];
    assert(contrast->selector.kind == StyleKindButton());
    assert(contrast->style.border_width == 2.0f);
    assert(contrast->layer == 0);

    button = &collected.rules[2];
    assert(button->selector.kind == StyleKindButton());
    assert(button->layer == 1);
    assert(button->style.background == 0xffcc00ffu);
    assert(button->style.foreground == 0xf0f0f0ffu);
    assert(button->style.border == 0x445566ffu);
    assert(button->style.radius == 0.0f);
    assert((button->style.fields & (uint32_t)StyleRadius) != 0u);
    assert(button->style.padding_y == 80.0f);
    assert(button->style.letter_spacing == 2.0f);
    assert(button->style.material == MaterialFlat);
    assert(StringEqual(button->style.typeface, StringView("semibold", 8)));
    assert(collected.origins[2].file == 0);
    assert(collected.origins[2].line == 26);

    pressed = &collected.rules[3];
    assert(pressed->state == ButtonStatePressed);
    assert(pressed->style.background == 0x304050ffu);

    quiet = &collected.rules[4];
    assert(quiet->style.background == 0x00000000u);
    assert(quiet->style.foreground == 0x000000ffu);
    assert(quiet->style.border == 0xffffffffu);

    /* The declared variant is enumerated but contributes nothing unless the
     * parse environment selects it. */
    assert(RegisterStyleModule("matched-module", module));
    assert(kss_parse_with_variant(source, NULL, rules, 64, &result,
                                  diagnostic, sizeof(diagnostic)));
    assert(result.variant_count == 2);
    assert(strcmp(result.variants[0].name, "glow") == 0);
    assert(strcmp(result.variants[0].label, "Glow") == 0);
    /* Default environment: no theme, no contrast overlay, so the sheet
     * yields import rules plus the three pack rules. */
    assert(result.rule_count == 4);

    assert(collect(source, "matched.kss", module, "matched_module.kss",
                  "glow", &glow));
    /* The variant overlay wins over the dark theme for accent (origin
     * variant sorts above theme), and the variant rule lands after the
     * environment rule and before the pack layer. */
    assert(glow.count == 6);
    assert(glow.rules[2].style.background == 0x00ff00ffu);
    assert(glow.rules[2].style.letter_spacing == 2.0f);
    assert(glow.rules[3].style.letter_spacing == 9.0f);
    assert(glow.rules[3].layer == 1);
    assert(glow.rules[5].style.background == 0x00000000u);

    /* Resolution parity anchor: the shared cascade must produce the same
     * winners the Go suite asserts from this fixture. */
    {
        StyleSheet sheet;
        StyleFacts facts;
        StyleData value;

        sheet.rules = collected.rules;
        sheet.rule_count = collected.count;
        facts = StyleDefaultFacts(StyleKindButton());
        value = ResolveStyle(&sheet, (StyleData){0}, facts, ButtonStateNormal);
        assert(value.background == 0xffcc00ffu);
        assert(value.foreground == 0xf0f0f0ffu);
        assert(value.border == 0x445566ffu);
        assert(value.radius == 0.0f);
        assert((value.fields & (uint32_t)StyleRadius) != 0u);
        assert(value.padding_y == 80.0f);
        assert(value.border_width == 2.0f);
        assert(value.letter_spacing == 2.0f);
        assert(value.material == MaterialFlat);
        assert(StringEqual(value.typeface, StringView("semibold", 8)));
        value = ResolveStyle(&sheet, (StyleData){0}, facts, ButtonStatePressed);
        assert(value.background == 0x304050ffu);
        facts.class_name = StyleClassId("quiet");
        value = ResolveStyle(&sheet, (StyleData){0}, facts, ButtonStateNormal);
        assert(value.background == 0x00000000u);
        assert(value.foreground == 0x000000ffu);
        assert(value.border == 0xffffffffu);
        /* The glow variant rule wins over the same-layer base rule because
         * it is declared after it. */
        sheet.rules = glow.rules;
        sheet.rule_count = glow.count;
        facts = StyleDefaultFacts(StyleKindButton());
        value = ResolveStyle(&sheet, (StyleData){0}, facts, ButtonStateNormal);
        assert(value.letter_spacing == 9.0f);
        assert(value.background == 0x00ff00ffu);
    }

    free(source);
    free(module);
}

static void
test_truncation(const char *source)
{
    size_t length = strlen(source);
    StyleRule rules[8];
    KssParseResult result;
    char diagnostic[256];

    for(size_t cut = 0; cut <= length; cut++) {
        char *prefix = (char *)malloc(cut + 1);
        assert(prefix != NULL);
        memcpy(prefix, source, cut);
        prefix[cut] = '\0';
        (void)kss_parse_string(prefix, rules, 8, &result, diagnostic,
                               sizeof(diagnostic));
        free(prefix);
    }
}

static void
test_mutation(const char *source)
{
    size_t length = strlen(source);
    StyleRule rules[8];
    KssParseResult result;
    char diagnostic[256];
    char *copy = (char *)malloc(length + 1);
    uint32_t seed = 0x5eed1234u;

    assert(copy != NULL);
    for(int iteration = 0; iteration < 4000; iteration++) {
        size_t index;

        memcpy(copy, source, length + 1);
        for(int flip = 0; flip < 3; flip++) {
            seed = seed * 1103515245u + 12345u;
            index = (seed >> 8) % length;
            seed = seed * 1103515245u + 12345u;
            copy[index] = (char)((seed >> 16) & 0xff);
        }
        (void)kss_parse_string(copy, rules, 8, &result, diagnostic,
                               sizeof(diagnostic));
    }
    free(copy);
}

static void
test_environment_names(void)
{
    FILE *file = fopen("tests/fixtures/kss/environments.txt", "r");
    char names[6][64];
    int expected[5];
    int fields;
    int cases = 0;

    assert(file != NULL);
    while((fields = fscanf(file, "%63s %63s %63s %63s %63s %63s %d %d %d %d %d",
        names[0], names[1], names[2], names[3], names[4], names[5],
        &expected[0], &expected[1], &expected[2], &expected[3], &expected[4])) != EOF) {
        String values[6];
        assert(fields == 11);
        for(int i = 0; i < 6; i++) {
            if(strcmp(names[i], "-") == 0)
                names[i][0] = '\0';
            values[i] = StringView(names[i], strlen(names[i]));
        }
        KssEnvironment env = KssEnvironmentWithNames(KssDefaultEnvironment(),
            values[0], values[1], values[2], values[3], values[4], values[5]);
        assert(env.theme == expected[0]);
        assert(env.contrast == expected[1]);
        assert(env.density == expected[2]);
        assert(env.pointer == expected[3]);
        assert(env.platform == expected[4]);
        assert(env.variant.length == (int)strlen(names[5]));
        assert(memcmp(env.variant.bytes, names[5], env.variant.length) == 0);
        cases++;
    }
    assert(cases == 7);
    fclose(file);
}

static void
test_css_values(void)
{
    char *source = read_file("tests/fixtures/kss/css-values.kss");
    KssEnvironment env = KssDefaultEnvironment();
    env = KssEnvironmentWithNames(env, StringView("dark", 4),
        StringView("", 0), StringView("", 0), StringView("", 0),
        StringView("", 0), StringView("", 0));
    KssParser p = KssBeginDeclarative(StringView(source, strlen(source)),
        StringView("css-values.kss", 14), env);
    int rules = 0;
    int colors = 0;
    int captions = 0;
    while(p.status == KssStatusContinue || p.status == KssStatusRule) {
        if(p.status == KssStatusRule) {
            rules++;
            for(int i = 0; i < p.declaration_count; i++) {
                KssDeclaration entry = p.declarations[i];
                if(entry.rule != p.rule.order)
                    continue;
                String name = StringView(source + entry.name_start, entry.name_length);
                String text = StringView(source + entry.value_start, entry.value_length);
                KssCSSValue value = KssResolveCSSValue(p, name, text);
                assert(value.valid);
                if(StringEqual(name, StringView("background", 10))) {
                    assert(value.kind == KssCSSColor);
                    assert(value.color == (colors < 2 ? 0x112233ffu : 0x445566ffu));
                    assert(value.token_origin == (colors < 2 ? KssOriginPack : KssOriginTheme));
                    KssCSSValue override = KssOverrideCSSValue(value, StringView("#abcdef", 7), true);
                    assert(override.kind == KssCSSLiteral);
                    assert(StringEqual(override.text, StringView("#abcdef", 7)));
                    colors++;
                }
                if(StringEqual(name, StringView("--caption", 9))) {
                    assert(StringEqual(value.text, StringView("\"café; }\"", strlen("\"café; }\""))));
                    captions++;
                }
                if(StringEqual(name, StringView("width", 5))) {
                    assert(value.kind == KssCSSNumber && value.number == 150);
                }
            }
            p.status = KssStatusContinue;
        } else {
            p = KssStep(p);
        }
    }
    assert(p.status == KssStatusDone);
    assert(rules == 4 && colors == 3 && captions == 1);
    assert(!KssResolveCSSValue(p, StringView("unknown-property", 16), StringView("1", 1)).valid);
    const char *invalid = "1e9999999999999999999999";
    assert(KssResolveCSSValue(p, StringView("width", 5), StringView(invalid, strlen(invalid))).kind == KssCSSLiteral);
    free(source);
}

static void
test_selector_predicates(void)
{
    FILE *file = fopen("tests/fixtures/kss/selector-predicates.tsv", "r");
    char line[1024];
    int count = 0;
    assert(file != NULL);
    while(fgets(line, sizeof(line), file) != NULL) {
        char *fields[5] = {line};
        for(int i = 0; i < 4; i++) {
            char *separator = strchr(fields[i], '\t');
            assert(separator != NULL);
            *separator = '\0';
            fields[i + 1] = separator + 1;
        }
        bool present = strcmp(fields[1], "<missing>") != 0;
        if(!present)
            fields[1][0] = '\0';
        for(int i = 1; i <= 2; i++) {
            if(strcmp(fields[i], "<empty>") == 0)
                fields[i][0] = '\0';
        }
        bool actual;
        if(strcmp(fields[0], "A") == 0) {
            actual = KssAttributeMatches(StringView(fields[1], strlen(fields[1])),
                StringView(fields[2], strlen(fields[2])), StringView(fields[3], strlen(fields[3])),
                present);
        } else {
            assert(strcmp(fields[0], "N") == 0);
            actual = KssNthMatches(StringView(fields[1], strlen(fields[1])), atoi(fields[2]));
        }
        assert(actual == (atoi(fields[4]) != 0));
        count++;
    }
    assert(count == 40);
    fclose(file);
}

typedef struct FactField { const char *name; size_t offset; char type; } FactField;

static const FactField selector_S_fields[] = {
    {"kind", offsetof(KssStateFacts, kind), 's'},
    {"tag", offsetof(KssStateFacts, tag), 's'},
    {"any_active", offsetof(KssStateFacts, any_active), 'b'},
    {"requested", offsetof(KssStateFacts, requested), 'b'},
    {"hover", offsetof(KssStateFacts, hover), 'b'},
    {"pressed", offsetof(KssStateFacts, pressed), 'b'},
    {"focus", offsetof(KssStateFacts, focus), 'b'},
    {"disabled", offsetof(KssStateFacts, disabled), 'b'},
    {"node_disabled", offsetof(KssStateFacts, node_disabled), 'b'},
    {"readonly", offsetof(KssStateFacts, readonly), 'b'},
    {"readonly_camel", offsetof(KssStateFacts, readonly_camel), 'b'},
    {"node_readonly", offsetof(KssStateFacts, node_readonly), 'b'},
    {"required", offsetof(KssStateFacts, required), 'b'},
    {"node_required", offsetof(KssStateFacts, node_required), 'b'},
    {"valid", offsetof(KssStateFacts, valid), 'b'},
    {"invalid", offsetof(KssStateFacts, invalid), 'b'},
    {"aria_invalid", offsetof(KssStateFacts, aria_invalid), 'b'},
    {"extra_invalid", offsetof(KssStateFacts, extra_invalid), 'b'},
    {"placeholder_shown", offsetof(KssStateFacts, placeholder_shown), 'b'},
    {"placeholder_shown_camel", offsetof(KssStateFacts, placeholder_shown_camel), 'b'},
    {"placeholder_present", offsetof(KssStateFacts, placeholder_present), 'b'},
    {"value_present", offsetof(KssStateFacts, value_present), 'b'},
};

static const FactField selector_R_fields[] = {
    {"has_parent", offsetof(KssStructuralFacts, has_parent), 'b'},
    {"has_scope", offsetof(KssStructuralFacts, has_scope), 'b'},
    {"is_scope", offsetof(KssStructuralFacts, is_scope), 'b'},
    {"sibling_index", offsetof(KssStructuralFacts, sibling_index), 'i'},
    {"sibling_count", offsetof(KssStructuralFacts, sibling_count), 'i'},
    {"type_index", offsetof(KssStructuralFacts, type_index), 'i'},
    {"type_count", offsetof(KssStructuralFacts, type_count), 'i'},
    {"has_children", offsetof(KssStructuralFacts, has_children), 'b'},
    {"has_text", offsetof(KssStructuralFacts, has_text), 'b'},
    {"focus_within", offsetof(KssStructuralFacts, focus_within), 'b'},
    {"target", offsetof(KssStructuralFacts, target), 'b'},
};

static void
test_selector_facts(void)
{
    FILE *file = fopen("tests/fixtures/kss/selector-facts.tsv", "r");
    char line[1024];
    int count = 0;
    assert(file != NULL);
    while(fgets(line, sizeof(line), file) != NULL) {
        char *fields[4] = {line};
        for(int i = 0; i < 3; i++) {
            char *separator = strchr(fields[i], '\t');
            assert(separator != NULL);
            *separator = '\0';
            fields[i + 1] = separator + 1;
        }
        bool state = strcmp(fields[0], "S") == 0;
        assert(state || strcmp(fields[0], "R") == 0);
        KssStateFacts state_facts = {0};
        KssStructuralFacts structural = {0};
        char *record = state ? (char *)&state_facts : (char *)&structural;
        const FactField *table = state ? selector_S_fields : selector_R_fields;
        size_t length = state ? sizeof(selector_S_fields) / sizeof(*table) : sizeof(selector_R_fields) / sizeof(*table);
        char *assignment = strtok(fields[2], ",");
        while(assignment != NULL && strcmp(assignment, "-") != 0) {
            char *value = strchr(assignment, '=');
            assert(value != NULL);
            *value++ = '\0';
            bool found = false;
            for(size_t i = 0; i < length; i++) {
                if(strcmp(table[i].name, assignment) != 0)
                    continue;
                void *field = record + table[i].offset;
                if(table[i].type == 'b')
                    *(bool *)field = atoi(value) != 0;
                else if(table[i].type == 'i')
                    *(int32_t *)field = atoi(value);
                else
                    *(String *)field = StringView(value, strlen(value));
                found = true;
                break;
            }
            assert(found);
            assignment = strtok(NULL, ",");
        }
        String query = StringView(fields[1], strlen(fields[1]));
        int actual = state ? (int)KssStateMatches(query, state_facts) : KssStructuralMatch(query, structural);
        assert(actual == atoi(fields[3]));
        count++;
    }
    assert(count == 55);
    fclose(file);
}

static void
test_selector_grammar(void)
{
    FILE *file = fopen("tests/fixtures/kss/selector-grammar.tsv", "r");
    char line[4096];
    int cases = 0;
    assert(file != NULL);
    while(fgets(line, sizeof(line), file) != NULL) {
        line[strcspn(line, "\r\n")] = '\0';
        char *fields[9] = {line};
        for(int i = 0; i < 8; i++) {
            char *separator = strchr(fields[i], '\t');
            assert(separator != NULL);
            *separator = '\0';
            fields[i + 1] = separator + 1;
        }
        String source = StringView(fields[1], strlen(fields[1]));
        KssSelectorCursor parser = KssBeginSelector(source);
        KssSelectorAtom last = {0};
        KssSelectorSpan last_part = {0};
        KssCursor cursor = {0};
        cursor.source = source;
        bool atom_mode = strcmp(fields[0], "A") == 0;
        bool valid = false;
        int count = 0;
        for(size_t steps = 0; steps <= source.length + 1; steps++) {
            if(atom_mode) {
                KssSelectorAtom atom = KssSelectorNext(parser);
                if(!atom.ok)
                    break;
                if(atom.done) {
                    valid = true;
                    break;
                }
                assert(atom.parser.cursor.pos > parser.cursor.pos);
                parser = atom.parser;
                last = atom;
            } else {
                KssSelectorSpan part = KssSelectorPart(cursor, strcmp(fields[0], "S") == 0);
                if(!part.ok)
                    break;
                if(part.done) {
                    valid = true;
                    break;
                }
                assert(part.parser.pos > cursor.pos);
                cursor = part.parser;
                last_part = part;
            }
            count++;
        }
        assert(valid == (atoi(fields[2]) != 0));
        if(valid) {
            assert(count == atoi(fields[3]));
            assert(parser.specificity == atoi(fields[4]));
            String actual[4] = {{0}};
            char relation[2] = {(char)last_part.combinator, '\0'};
            if(atom_mode) {
                actual[0] = last.canonical.length > 0
                    ? StringView((const char *)last.canonical.bytes, last.canonical.length) : last.name;
                actual[1] = last.value;
                actual[2] = last.argument;
                actual[3] = last.operation;
            } else {
                actual[0] = StringView(source.data + last_part.start, last_part.length);
                actual[1] = last_part.combinator == 32 ? StringView("space", 5)
                    : StringView(relation, last_part.combinator != 0 ? 1 : 0);
            }
            for(int i = 0; i < 4; i++) {
                const char *expected = strcmp(fields[i + 5], "-") == 0 ? "" : fields[i + 5];
                assert(StringEqual(actual[i], StringView(expected, strlen(expected))));
            }
        }
        cases++;
    }
    assert(cases == 32);
    fclose(file);
}

int
main(void)
{
    char *source = read_file("tests/fixtures/kss/matched.kss");

    ClearStyleModules();
    test_environment_names();
    test_css_values();
    test_selector_predicates();
    test_selector_facts();
    test_selector_grammar();
    test_matched_fixture();
    test_truncation(source);
    test_mutation(source);
    test_mutation("@theme dark { accent: #ffcc00; } @env contrast(high) { "
                  "Button { background: accent; } } tokens { color { a: #1; } }");
    free(source);
    printf("kss matched ok\n");
    return 0;
}

/* Matched-fixture and invalid-input coverage for the shared KSS module
 * (runtime/kss_parser.kry). Drives the generated parser directly so rule
 * provenance is asserted alongside values. */
#include "ui_style_sheet.h"
#include "ui/kss_parser.h"
#include "runtime/kss_parser.h"

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
    env.theme = KssThemeDark;
    env.contrast = KssContrastHigh;
    env.density = KssDensityComfortable;
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
    assert(collected.origins[2].line == 27);

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
    assert(result.variant_count == 1);
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
    assert(glow.rules[2].style.letter_spacing == 9.0f);
    assert(glow.rules[2].layer == 0);
    assert(glow.rules[3].style.background == 0x00ff00ffu);
    assert(glow.rules[3].style.letter_spacing == 2.0f);
    assert(glow.rules[5].style.background == 0x00000000u);

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

int
main(void)
{
    char *source = read_file("tests/fixtures/kss/matched.kss");

    ClearStyleModules();
    test_matched_fixture();
    test_truncation(source);
    test_mutation(source);
    test_mutation("@theme dark { accent: #ffcc00; } @env contrast(high) { "
                  "Button { background: accent; } } tokens { color { a: #1; } }");
    free(source);
    printf("kss matched ok\n");
    return 0;
}

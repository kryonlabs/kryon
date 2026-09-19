/* KSS formatter: stable re-layout with verbatim construct text, preserved
 * comments, idempotent output, and semantic round-trip. */
#include "ui_style_sheet.h"
#include "ui/kss_parser.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static const char *ugly =
    "@version 1;\n"
    "@pack fmt.demo;\n"
    "   // lead comment\n"
    "tokens {\n"
    "      color {\n"
    "        accent: #112233;\n"
    "      }\n"
    "        length { pad: 12; }\n"
    "}\n"
    "\n\n\n"
    "@theme dark {\n"
    "  accent: #ffcc00;\n"
    "}\n"
    "@layer components;\n"
    "/* block comment */\n"
    "Button {\n"
    "    background: accent;\n"
    "      radius: 0;\n"
    "}\n"
    "Button:pressed { background: #304050; opacity: 0.5; }";

int
main(void)
{
    char once[4096];
    char twice[4096];
    char diagnostic[256];
    StyleRule rules_before[16];
    StyleRule rules_after[16];
    KssParseResult before;
    KssParseResult after;
    int length;

    ClearStyleModules();
    length = kss_format_string(ugly, once, sizeof(once), diagnostic,
                               sizeof(diagnostic));
    assert(length > 0);

    /* Comments survive. */
    assert(strstr(once, "// lead comment") != NULL);
    assert(strstr(once, "/* block comment */") != NULL);

    /* Single-line declaration blocks are reflowed. */
    assert(strstr(once, "    length {\n        pad: 12;\n    }") != NULL);
    assert(strstr(once, "Button:pressed {\n    background: #304050;\n    opacity: 0.5;\n}") != NULL);

    /* Idempotent. */
    length = kss_format_string(once, twice, sizeof(twice), diagnostic,
                               sizeof(diagnostic));
    assert(length > 0);
    assert(strcmp(once, twice) == 0);

    /* Semantic round-trip: same rules with the same winners. */
    assert(kss_parse_string(ugly, rules_before, 16, &before, diagnostic,
                            sizeof(diagnostic)));
    assert(kss_parse_string(once, rules_after, 16, &after, diagnostic,
                            sizeof(diagnostic)));
    assert(before.rule_count == after.rule_count);
    for(int i = 0; i < after.rule_count; i++) {
        assert(rules_before[i].selector.kind == rules_after[i].selector.kind);
        assert(rules_before[i].style.background == rules_after[i].style.background);
        assert(rules_before[i].style.radius == rules_after[i].style.radius);
        assert(rules_before[i].style.fields == rules_after[i].style.fields);
        assert(rules_before[i].layer == rules_after[i].layer);
    }

    printf("%s", once);
    printf("kss formatter ok\n");
    return 0;
}

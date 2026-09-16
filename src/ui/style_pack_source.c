#include "ui_style_sheet.h"

#include "kss_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STYLE_SOURCE_PACK_MAX 32
#define STYLE_SOURCE_RULE_MAX 320

typedef struct StyleSourcePack {
    char id[64];
    char base_id[64];
    const char *source;
    const char *label;
    const char *description;
    StyleRule rules[STYLE_SOURCE_RULE_MAX];
    StyleSheet sheet;
} StyleSourcePack;

static StyleSourcePack source_packs[STYLE_SOURCE_PACK_MAX];
static int source_pack_count;
static bool reapply_style_theme(const char *theme);

static const char *active_style_theme = "";

bool
SetStyleTheme(const char *theme)
{
    const char *value = theme != NULL ? theme : "";
    char active[64];

    if(strcmp(active_style_theme, value) == 0 && source_pack_count >= 0)
        return true;
    snprintf(active, sizeof(active), "%s", GetActiveStylePackId());
    active_style_theme = value;
    if(!reapply_style_theme(value)) {
        active_style_theme = "";
        return false;
    }
    if(!ReapplyBuiltInStyleTheme(value)) {
        active_style_theme = "";
        return false;
    }
    if(active[0] != '\0')
        SetActiveStylePack(active);
    return true;
}

static char *
style_copy_text(const char *text)
{
    size_t len;
    char *copy;

    if(text == NULL)
        return NULL;
    len = strlen(text);
    copy = (char *)malloc(len + 1);
    if(copy == NULL)
        return NULL;
    memcpy(copy, text, len + 1);
    return copy;
}

static StyleSourcePack *
style_source_pack_slot(const char *id)
{
    for(int i = 0; i < source_pack_count; i++)
        if(strcmp(source_packs[i].id, id) == 0)
            return &source_packs[i];
    if(source_pack_count >= STYLE_SOURCE_PACK_MAX)
        return NULL;
    return &source_packs[source_pack_count++];
}

static bool register_style_variant(const char *source, const char *base_id,
                                   const char *variant_name,
                                   const char *variant_label);
static bool reapply_style_theme(const char *theme);

/* Re-register every stored base source pack under the current theme; each
 * registration re-derives its declared variants. Variant slots themselves
 * carry their base id in `base_id`. */

static bool
register_style_source(const char *source, const char *label,
                      const char *description, const char *id,
                      const StyleColorToken *colors, int color_count)
{
    KssParseResult result = {0};
    StyleRule rules[STYLE_SOURCE_RULE_MAX] = {0};
    char diagnostic[256];
    StyleSourcePack *slot;
    char *source_copy = NULL;
    char *label_copy = NULL;
    char *description_copy = NULL;

    if(source == NULL)
        return false;
    source_copy = style_copy_text(source);
    if(source_copy == NULL)
        return false;
    if(colors != NULL || color_count > 0) {
        /* Legacy color-substitution variants bring their own palette and do
         * not participate in theme overlays. */
        if(!kss_parse_variant(source_copy, colors, color_count, rules,
                              STYLE_SOURCE_RULE_MAX, &result,
                              diagnostic, sizeof(diagnostic))) {
            free(source_copy);
            return false;
        }
    } else if(!kss_parse_with_environment(source_copy, NULL, active_style_theme,
                                          rules, STYLE_SOURCE_RULE_MAX, &result,
                                          diagnostic, sizeof(diagnostic))) {
        free(source_copy);
        return false;
    }
    if(result.pack_id[0] == '\0' || result.rule_count <= 0)
    {
        free(source_copy);
        return false;
    }

    if(id != NULL) {
        if(id[0] == '\0' || strlen(id) >= sizeof(result.pack_id)) {
            free(source_copy);
            return false;
        }
        snprintf(result.pack_id, sizeof(result.pack_id), "%s", id);
    }
    slot = style_source_pack_slot(result.pack_id);
    if(slot == NULL) {
        free(source_copy);
        return false;
    }
    label_copy = style_copy_text(label != NULL ? label : result.pack_id);
    description_copy = style_copy_text(description != NULL ? description : "");
    if(label_copy == NULL || description_copy == NULL) {
        free(source_copy);
        free(label_copy);
        free(description_copy);
        return false;
    }

    free((void *)slot->source);
    free((void *)slot->label);
    free((void *)slot->description);
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->id, sizeof(slot->id), "%s", result.pack_id);
    snprintf(slot->base_id, sizeof(slot->base_id), "%s", result.pack_id);
    slot->source = source_copy;
    slot->label = label_copy;
    slot->description = description_copy;
    memcpy(slot->rules, rules, (size_t)result.rule_count * sizeof(rules[0]));
    slot->sheet.rules = slot->rules;
    slot->sheet.rule_count = result.rule_count;

    if(!RegisterStylePack((StylePack){
        .id = slot->id,
        .label = slot->label,
        .description = slot->description,
        .sheet = &slot->sheet,
    }))
        return false;

    /* Declared '@variant' blocks become selectable pack options under
     * '<pack>.<variant>'; each re-parses the same source with that variant
     * active so base and variant rules resolve together. */
    for(int i = 0; i < result.variant_count && i < KSS_VARIANT_MAX; i++) {
        if(!register_style_variant(source_copy, result.pack_id,
                                   result.variants[i].name,
                                   result.variants[i].label))
            return false;
    }
    return true;
}

static bool
register_style_variant(const char *source, const char *base_id,
                       const char *variant_name, const char *variant_label)
{
    KssParseResult result = {0};
    StyleRule rules[STYLE_SOURCE_RULE_MAX] = {0};
    char diagnostic[256];
    char variant_id[sizeof(((StyleSourcePack *)0)->id)] = {0};
    char variant_source[96] = {0};
    StyleSourcePack *slot;
    char *source_copy = NULL;
    char *label_copy = NULL;
    char *description_copy = NULL;

    snprintf(variant_id, sizeof(variant_id), "%s.%s", base_id, variant_name);
    snprintf(variant_source, sizeof(variant_source), "%s", variant_name);
    if(!kss_parse_with_environment(source, variant_source, active_style_theme,
                                   rules, STYLE_SOURCE_RULE_MAX, &result,
                                   diagnostic, sizeof(diagnostic)))
        return false;
    if(result.rule_count <= 0)
        return false;
    slot = style_source_pack_slot(variant_id);
    if(slot == NULL)
        return false;
    source_copy = style_copy_text(source);
    label_copy = style_copy_text(variant_label);
    description_copy = style_copy_text("");
    if(source_copy == NULL || label_copy == NULL || description_copy == NULL) {
        free(source_copy);
        free(label_copy);
        free(description_copy);
        return false;
    }
    free((void *)slot->source);
    free((void *)slot->label);
    free((void *)slot->description);
    memset(slot, 0, sizeof(*slot));
    snprintf(slot->id, sizeof(slot->id), "%s", variant_id);
    snprintf(slot->base_id, sizeof(slot->base_id), "%s", base_id);
    slot->source = source_copy;
    slot->label = label_copy;
    slot->description = description_copy;
    memcpy(slot->rules, rules, (size_t)result.rule_count * sizeof(rules[0]));
    slot->sheet.rules = slot->rules;
    slot->sheet.rule_count = result.rule_count;
    return RegisterStylePack((StylePack){
        .id = slot->id,
        .label = slot->label,
        .description = slot->description,
        .sheet = &slot->sheet,
    });
}

bool
RegisterStylePackSource(const char *source, const char *label,
                        const char *description)
{
    return register_style_source(source, label, description, NULL, NULL, 0);
}

bool
RegisterStylePackVariant(const char *id, const char *source, const char *label,
                         const StyleColorToken *colors, int color_count)
{
    if(id == NULL)
        return false;
    return register_style_source(source, label, "", id, colors, color_count);
}

static bool
reapply_style_theme(const char *theme)
{
    (void)theme;
    for(int i = 0; i < source_pack_count; i++) {
        StyleSourcePack *slot = &source_packs[i];
        char id[64];
        char label[256];
        char description[256];
        int written;

        if(slot->base_id[0] == '\0' || strcmp(slot->id, slot->base_id) != 0)
            continue; /* variant slots re-derive from their base */
        written = snprintf(id, sizeof(id), "%s", slot->id);
        if(written <= 0)
            continue;
        snprintf(label, sizeof(label), "%s", slot->label != NULL ? slot->label : "");
        snprintf(description, sizeof(description), "%s",
                 slot->description != NULL ? slot->description : "");
        if(!register_style_source(slot->source, label, description, id,
                                  NULL, 0))
            return false;
    }
    return true;
}

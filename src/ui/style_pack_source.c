#include "ui_style_sheet.h"

#include "kss_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STYLE_SOURCE_PACK_MAX 32
#define STYLE_SOURCE_RULE_MAX 320

typedef struct StyleSourcePack {
    char id[64];
    const char *source;
    const char *label;
    const char *description;
    StyleRule rules[STYLE_SOURCE_RULE_MAX];
    StyleSheet sheet;
} StyleSourcePack;

static StyleSourcePack source_packs[STYLE_SOURCE_PACK_MAX];
static int source_pack_count;

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
    if(!kss_parse_variant(source_copy, colors, color_count, rules,
                          STYLE_SOURCE_RULE_MAX, &result,
                          diagnostic, sizeof(diagnostic)))
    {
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

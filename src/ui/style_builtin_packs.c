#include "ui_style_sheet.h"

#include "kss_parser.h"
#include "embedded_assets.h"

#include <stdlib.h>
#include <string.h>

#define BUILTIN_STYLE_RULE_MAX 256

typedef struct BuiltInStylePack {
    const char *id;
    const char *label;
    const char *description;
    const char *path;
    StyleRule rules[BUILTIN_STYLE_RULE_MAX];
    StyleSheet sheet;
} BuiltInStylePack;

static BuiltInStylePack builtin_style_packs[] = {
    {
        .id = "kryon.material",
        .label = "Material",
        .description = "Clean Material-like controls with flat paint",
        .path = "styles/kryon/material.kss",
    },
    {
        .id = "kryon.tk",
        .label = "TK",
        .description = "Dense toolkit controls for desktop utilities",
        .path = "styles/kryon/tk.kss",
    },
    {
        .id = "kryon.vanilla",
        .label = "Vanilla",
        .description = "Current Kryon default controls as an explicit pack",
        .path = "styles/kryon/vanilla.kss",
    },
    {
        .id = "kryon.glow",
        .label = "Glow",
        .description = "Current glow treatment as an explicit pack",
        .path = "styles/kryon/glow.kss",
    },
    {
        .id = "kryon.lightfield",
        .label = "Lightfield",
        .description = "Premium translucent Lightfield controls",
        .path = "styles/kryon/lightfield.kss",
    },
};

static bool
register_builtin_style_pack(BuiltInStylePack *pack)
{
    KssParseResult result = {0};
    char diagnostic[256];
    char *source;
    bool ok;

    source = LoadEmbeddedAssetText(pack->path);
    if(source == NULL)
        return false;

    memset(pack->rules, 0, sizeof(pack->rules));
    ok = kss_parse_string(source, pack->rules, BUILTIN_STYLE_RULE_MAX, &result,
                          diagnostic, sizeof(diagnostic));
    free(source);
    if(!ok || strcmp(result.pack_id, pack->id) != 0 || result.rule_count <= 0)
        return false;

    pack->sheet.rules = pack->rules;
    pack->sheet.rule_count = result.rule_count;
    return RegisterStylePack((StylePack){
        .id = pack->id,
        .label = pack->label,
        .description = pack->description,
        .sheet = &pack->sheet,
    });
}

bool
RegisterBuiltInStylePacks(void)
{
    int count = (int)(sizeof(builtin_style_packs) /
                      sizeof(builtin_style_packs[0]));

    for(int i = 0; i < count; i++)
        if(!register_builtin_style_pack(&builtin_style_packs[i]))
            return false;
    return SetActiveStylePack("kryon.material");
}

bool
EnsureBuiltInStylePacks(void)
{
    const char *active = GetActiveStylePackId();
    int count = (int)(sizeof(builtin_style_packs) /
                      sizeof(builtin_style_packs[0]));

    for(int i = 0; i < count; i++)
        if(FindStylePack(builtin_style_packs[i].id) == NULL &&
           !register_builtin_style_pack(&builtin_style_packs[i]))
            return false;

    if(active != NULL && active[0] != '\0')
        return SetActiveStylePack(active);
    if(GetActiveStylePack() != NULL)
        return true;
    return SetActiveStylePack("kryon.material");
}

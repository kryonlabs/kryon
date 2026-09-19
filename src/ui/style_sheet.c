#include "ui_style_sheet.h"

#include <string.h>

static StylePack style_packs[STYLE_PACK_MAX];
static int style_pack_count;
static int active_style_pack = -1;
static uint64_t style_pack_version;

#define STYLE_CACHE_COUNT 256
typedef struct {
    uint64_t version;
    int valid;
    int active_state;
    StyleFacts facts;
    StyleData value;
} StyleCacheEntry;

static StyleCacheEntry active_style_cache[STYLE_CACHE_COUNT];

static unsigned int
style_cache_slot(StyleFacts facts, int active_state)
{
    uint32_t values[] = {facts.kind, facts.name, facts.class_name, facts.role,
        facts.tone, facts.emphasis, facts.size, facts.state, facts.validation,
        facts.orientation, facts.placement, active_state};
    uint32_t hash = 2166136261u;

    for(unsigned int i = 0; i < sizeof(values) / sizeof(values[0]); i++)
        hash = (hash ^ values[i]) * 16777619u;
    return hash % STYLE_CACHE_COUNT;
}

static int
style_pack_index(const char *id)
{
    if(id == NULL || id[0] == '\0')
        return -1;
    for(int i = 0; i < style_pack_count; i++)
        if(style_packs[i].id != NULL && strcmp(style_packs[i].id, id) == 0)
            return i;
    return -1;
}

bool
RegisterStylePack(StylePack pack)
{
    int index;

    if(pack.id == NULL || pack.id[0] == '\0' || pack.sheet == NULL)
        return false;

    index = style_pack_index(pack.id);
    if(index < 0) {
        if(style_pack_count >= STYLE_PACK_MAX)
            return false;
        index = style_pack_count++;
    }

    style_packs[index] = pack;
    if(active_style_pack < 0)
        active_style_pack = index;
    style_pack_version++;
    return true;
}

void
ClearStylePacks(void)
{
    memset(style_packs, 0, sizeof(style_packs));
    style_pack_count = 0;
    active_style_pack = -1;
    style_pack_version++;
}

int
GetStylePackCount(void)
{
    return style_pack_count;
}

const StylePack *
GetStylePackAt(int index)
{
    if(index < 0 || index >= style_pack_count)
        return NULL;
    return &style_packs[index];
}

const StylePack *
FindStylePack(const char *id)
{
    int index = style_pack_index(id);
    if(index < 0)
        return NULL;
    return &style_packs[index];
}

bool
SetActiveStylePack(const char *id)
{
    int index = style_pack_index(id);
    if(index < 0)
        return false;
    if(active_style_pack != index) {
        active_style_pack = index;
        style_pack_version++;
    }
    return true;
}

const StylePack *
GetActiveStylePack(void)
{
    if(active_style_pack < 0 || active_style_pack >= style_pack_count)
        return NULL;
    return &style_packs[active_style_pack];
}

const char *
GetActiveStylePackId(void)
{
    const StylePack *pack = GetActiveStylePack();
    return pack != NULL ? pack->id : NULL;
}

int
GetStylePackOptions(StylePackOption *options, int capacity)
{
    int count = style_pack_count;

    if(options == NULL || capacity <= 0)
        return count;

    if(count > capacity)
        count = capacity;
    for(int i = 0; i < count; i++) {
        options[i].id = style_packs[i].id;
        options[i].label = style_packs[i].label;
        options[i].description = style_packs[i].description;
        options[i].active = i == active_style_pack;
    }
    return style_pack_count;
}

uint64_t
StylePackVersion(void)
{
    return style_pack_version;
}

int32_t
StyleClassId(const char *class_name)
{
    uint32_t hash = 2166136261u;

    if(class_name == NULL || class_name[0] == '\0')
        return 0;

    while(*class_name != '\0') {
        hash ^= (uint8_t)*class_name;
        hash *= 16777619u;
        class_name++;
    }
    hash &= 0x7fffffffu;
    return hash == 0 ? 1 : (int32_t)hash;
}

StyleData
ResolveStyle(const StyleSheet *sheet, StyleData base, StyleFacts facts,
             int active_state)
{
    StyleCascade cascade;

    if(sheet == NULL || sheet->rules == NULL || sheet->rule_count <= 0)
        return base;

    cascade = BeginStyleCascade(base);
    for(int i = 0; i < sheet->rule_count; i++) {
        /* Avoid copying the entire per-field cascade for an unmatched rule. */
        if(StyleRuleMatches(sheet->rules[i], facts, active_state))
            cascade = ApplyStyleRule(cascade, sheet->rules[i], facts, active_state);
    }
    return FinishStyleCascade(cascade);
}

StyleData
ResolveActiveStyle(StyleData base, StyleFacts facts, int active_state)
{
    const StylePack *pack = GetActiveStylePack();
    StyleCacheEntry *entry;

    if(pack == NULL)
        return base;
    entry = &active_style_cache[style_cache_slot(facts, active_state)];
    if(!entry->valid || entry->version != style_pack_version ||
       entry->active_state != active_state ||
       memcmp(&entry->facts, &facts, sizeof(facts)) != 0) {
        entry->value = ResolveStyle(pack->sheet, (StyleData){0}, facts, active_state);
        entry->facts = facts;
        entry->active_state = active_state;
        entry->version = style_pack_version;
        entry->valid = 1;
    }
    /* Cache only sheet declarations: live base values must still pass through. */
    return MergeValues(base, entry->value);
}

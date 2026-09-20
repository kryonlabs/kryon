#ifndef KRYON_STYLE_SHEET_H
#define KRYON_STYLE_SHEET_H

#include "runtime/style_sheet.h"
#include "ui_style_picker_props.generated.h"
#include <stdbool.h>
#include <stdint.h>

#define STYLE_PACK_MAX 32

typedef struct StylePack {
    const char *id;
    const char *label;
    const char *description;
    const StyleSheet *sheet;
} StylePack;

typedef struct StyleColorToken {
    const char *name;
    uint32_t color;
} StyleColorToken;

bool RegisterStylePackVariant(const char *id, const char *source,
                              const char *label, const StyleColorToken *colors,
                              int color_count);
bool RegisterStylePack(StylePack pack);
bool RegisterStylePackSource(const char *source, const char *label,
                             const char *description);
bool RegisterBuiltInStylePacks(void);
bool ReapplyBuiltInStyleTheme(const char *theme);
bool EnsureBuiltInStylePacks(void);
void ClearStylePacks(void);
int GetStylePackCount(void);
const StylePack *GetStylePackAt(int index);
const StylePack *FindStylePack(const char *id);
bool SetActiveStylePack(const char *id);
bool SetStyleTheme(const char *theme);
const StylePack *GetActiveStylePack(void);
const char *GetActiveStylePackId(void);
int GetStylePackOptions(StylePackOption *options, int capacity);
uint64_t StylePackVersion(void);
int32_t StyleClassId(const char *class_name);
StyleData ResolveStyle(const StyleSheet *sheet, StyleData base,
                       StyleFacts facts, int active_state);
StyleData ResolveActiveStyle(StyleData base, StyleFacts facts,
                             int active_state);
bool StylePicker(StylePickerProps props);

#endif /* KRYON_STYLE_SHEET_H */

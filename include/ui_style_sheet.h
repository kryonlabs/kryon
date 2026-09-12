#ifndef KRYON_STYLE_SHEET_H
#define KRYON_STYLE_SHEET_H

#include "runtime/style_sheet.h"
#include <stdbool.h>
#include <stdint.h>

#define STYLE_PACK_MAX 32

typedef struct StylePack {
    const char *id;
    const char *label;
    const char *description;
    const StyleSheet *sheet;
} StylePack;

typedef struct StylePackOption {
    const char *id;
    const char *label;
    const char *description;
    bool active;
} StylePackOption;

bool RegisterStylePack(StylePack pack);
void ClearStylePacks(void);
int GetStylePackCount(void);
const StylePack *GetStylePackAt(int index);
const StylePack *FindStylePack(const char *id);
bool SetActiveStylePack(const char *id);
const StylePack *GetActiveStylePack(void);
const char *GetActiveStylePackId(void);
int GetStylePackOptions(StylePackOption *options, int capacity);
uint64_t StylePackVersion(void);

#endif /* KRYON_STYLE_SHEET_H */
